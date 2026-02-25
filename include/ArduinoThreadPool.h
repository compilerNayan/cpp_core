#ifdef ARDUINO

#ifndef ARDUINO_THREAD_POOL_H
#define ARDUINO_THREAD_POOL_H

#include "IThreadPool.h"
#include "IRunnable.h"
#include <StandardDefines.h>
#include <queue>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define THREAD_POOL_ESP32_STACK_SIZE 8192
#define THREAD_POOL_ESP32_PRIORITY 1
#define THREAD_POOL_ESP32_MAX_QUEUE_SIGNALS 512
// Core 0 = system core, Core 1 = application core. Split workers across both.
#define THREAD_POOL_ESP32_SYSTEM_CORE 0
#define THREAD_POOL_ESP32_APP_CORE 1

/* @Component */
class ThreadPool final : public IThreadPool {

    struct WorkerParam {
        ThreadPool* self;
        BaseType_t coreId;
    };

    Private Size poolSize;
    Private Size systemCoreCount;
    Private Size appCoreCount;
    Private std::queue<std::function<Void()>*> taskQueueSystem;
    Private std::queue<std::function<Void()>*> taskQueueApp;
    Private SemaphoreHandle_t mutex;
    Private SemaphoreHandle_t taskAvailableSystem;
    Private SemaphoreHandle_t taskAvailableApp;
    Private SemaphoreHandle_t allDone;
    Private SemaphoreHandle_t workersExited;
    Private volatile Bool shutdownFlag;
    Private volatile Bool shutdownNowFlag;
    Private volatile Size runningCount;
    Private StdVector<TaskHandle_t> workerHandles;
    Private WorkerParam workerParamSystem;
    Private WorkerParam workerParamApp;

    Private Void WorkerLoop(BaseType_t coreId) {
        SemaphoreHandle_t mySem = (coreId == THREAD_POOL_ESP32_SYSTEM_CORE) ? taskAvailableSystem : taskAvailableApp;
        std::queue<std::function<Void()>*>* myQueue = (coreId == THREAD_POOL_ESP32_SYSTEM_CORE) ? &taskQueueSystem : &taskQueueApp;
        for (;;) {
            if (xSemaphoreTake(mySem, portMAX_DELAY) != pdTRUE) {
                continue;
            }
            std::function<Void()>* taskPtr = nullptr;
            {
                if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
                    continue;
                }
                if (shutdownNowFlag) {
                    xSemaphoreGive(mutex);
                    if (workersExited) { xSemaphoreGive(workersExited); }
                    vTaskDelete(nullptr);
                    return;
                }
                if (shutdownFlag && myQueue->empty()) {
                    xSemaphoreGive(mutex);
                    if (workersExited) { xSemaphoreGive(workersExited); }
                    vTaskDelete(nullptr);
                    return;
                }
                if (!myQueue->empty()) {
                    taskPtr = myQueue->front();
                    myQueue->pop();
                    ++runningCount;
                }
                xSemaphoreGive(mutex);
            }
            if (taskPtr) {
                try {
                    (*taskPtr)();
                } catch (...) {
                }
                delete taskPtr;
                taskPtr = nullptr;
                if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
                    --runningCount;
                    if (taskQueueSystem.empty() && taskQueueApp.empty() && runningCount == 0) {
                        xSemaphoreGive(allDone);
                    }
                    xSemaphoreGive(mutex);
                }
            }
        }
    }

    Private Static Void WorkerTrampoline(Void* param) {
        WorkerParam* wp = static_cast<WorkerParam*>(param);
        if (wp && wp->self) {
            wp->self->WorkerLoop(wp->coreId);
        }
        vTaskDelete(nullptr);
    }

Public
    ThreadPool() : ThreadPool(4) {}

    Explicit ThreadPool(CSize numThreads)
        : poolSize(numThreads > 0 ? numThreads : 1)
        , shutdownFlag(false)
        , shutdownNowFlag(false)
        , runningCount(0)
        , workersExited(nullptr) {
        systemCoreCount = (poolSize + 1) / 2;
        appCoreCount = poolSize / 2;
        workerParamSystem.self = this;
        workerParamSystem.coreId = THREAD_POOL_ESP32_SYSTEM_CORE;
        workerParamApp.self = this;
        workerParamApp.coreId = THREAD_POOL_ESP32_APP_CORE;
        mutex = xSemaphoreCreateMutex();
        taskAvailableSystem = xSemaphoreCreateCounting(THREAD_POOL_ESP32_MAX_QUEUE_SIGNALS, 0);
        taskAvailableApp = xSemaphoreCreateCounting(THREAD_POOL_ESP32_MAX_QUEUE_SIGNALS, 0);
        allDone = xSemaphoreCreateBinary();
        workersExited = xSemaphoreCreateCounting(static_cast<UBaseType_t>(poolSize), 0);
        if (!mutex || !taskAvailableSystem || !taskAvailableApp || !allDone || !workersExited) {
            if (mutex) { vSemaphoreDelete(mutex); mutex = nullptr; }
            if (taskAvailableSystem) { vSemaphoreDelete(taskAvailableSystem); taskAvailableSystem = nullptr; }
            if (taskAvailableApp) { vSemaphoreDelete(taskAvailableApp); taskAvailableApp = nullptr; }
            if (allDone) { vSemaphoreDelete(allDone); allDone = nullptr; }
            if (workersExited) { vSemaphoreDelete(workersExited); workersExited = nullptr; }
            poolSize = 0;
            return;
        }
        workerHandles.reserve(poolSize);
        for (Size i = 0; i < poolSize; ++i) {
            TaskHandle_t h = nullptr;
            BaseType_t coreId = (i < systemCoreCount) ? THREAD_POOL_ESP32_SYSTEM_CORE : THREAD_POOL_ESP32_APP_CORE;
            WorkerParam* param = (coreId == THREAD_POOL_ESP32_SYSTEM_CORE) ? &workerParamSystem : &workerParamApp;
            xTaskCreatePinnedToCore(WorkerTrampoline, "tp_worker", THREAD_POOL_ESP32_STACK_SIZE,
                        param, THREAD_POOL_ESP32_PRIORITY, &h, coreId);
            if (h) {
                workerHandles.push_back(h);
            }
        }
    }

    ~ThreadPool() override {
        if (poolSize > 0 && mutex && !shutdownFlag && !shutdownNowFlag) {
            Shutdown();
            WaitForCompletion(0);
            for (Size i = 0; i < poolSize && workersExited; ++i) {
                xSemaphoreTake(workersExited, portMAX_DELAY);
            }
        }
        if (taskAvailableSystem) {
            vSemaphoreDelete(taskAvailableSystem);
            taskAvailableSystem = nullptr;
        }
        if (taskAvailableApp) {
            vSemaphoreDelete(taskAvailableApp);
            taskAvailableApp = nullptr;
        }
        if (allDone) {
            vSemaphoreDelete(allDone);
            allDone = nullptr;
        }
        if (workersExited) {
            vSemaphoreDelete(workersExited);
            workersExited = nullptr;
        }
        if (mutex) {
            vSemaphoreDelete(mutex);
            mutex = nullptr;
        }
    }

    Bool Submit(std::function<Void()> task) override {
        return SubmitToCore(std::move(task), ThreadPoolCore::System);
    }

    Bool SubmitToCore(std::function<Void()> task, ThreadPoolCore core) {
        if (!task || !mutex) {
            return false;
        }
        if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
            return false;
        }
        if (shutdownFlag || shutdownNowFlag) {
            xSemaphoreGive(mutex);
            return false;
        }
        std::queue<std::function<Void()>*>* q = (core == ThreadPoolCore::System) ? &taskQueueSystem : &taskQueueApp;
        SemaphoreHandle_t sem = (core == ThreadPoolCore::System) ? taskAvailableSystem : taskAvailableApp;
        std::function<Void()>* copy = new std::function<Void()>(std::move(task));
        if (!copy) {
            xSemaphoreGive(mutex);
            return false;
        }
        q->push(copy);
        xSemaphoreGive(mutex);
        xSemaphoreGive(sem);
        return true;
    }

    Bool Execute(IRunnablePtr runnable, ThreadPoolCore core = ThreadPoolCore::System) override {
        if (!runnable) return false;
        return SubmitToCore([runnable]() { runnable->Run(); }, core);
    }

    Void Shutdown() override {
        shutdownFlag = true;
        for (Size i = 0; i < systemCoreCount; ++i) {
            xSemaphoreGive(taskAvailableSystem);
        }
        for (Size i = 0; i < appCoreCount; ++i) {
            xSemaphoreGive(taskAvailableApp);
        }
    }

    Void ShutdownNow() override {
        shutdownNowFlag = true;
        if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
            while (!taskQueueSystem.empty()) {
                std::function<Void()>* t = taskQueueSystem.front();
                taskQueueSystem.pop();
                delete t;
            }
            while (!taskQueueApp.empty()) {
                std::function<Void()>* t = taskQueueApp.front();
                taskQueueApp.pop();
                delete t;
            }
            xSemaphoreGive(mutex);
        }
        for (Size i = 0; i < systemCoreCount; ++i) {
            xSemaphoreGive(taskAvailableSystem);
        }
        for (Size i = 0; i < appCoreCount; ++i) {
            xSemaphoreGive(taskAvailableApp);
        }
    }

    Bool WaitForCompletion(CUInt timeoutMs = 0) override {
        if (!mutex) { return true; }
        TickType_t ticks = (timeoutMs == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
        for (;;) {
            if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
                return false;
            }
            Bool idle = taskQueueSystem.empty() && taskQueueApp.empty() && (runningCount == 0);
            xSemaphoreGive(mutex);
            if (idle) {
                return true;
            }
            if (xSemaphoreTake(allDone, ticks) != pdTRUE) {
                return false;
            }
            if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
                return false;
            }
            Bool done = taskQueueSystem.empty() && taskQueueApp.empty() && (runningCount == 0);
            xSemaphoreGive(mutex);
            if (done) {
                return true;
            }
        }
    }

    Size GetPoolSize() const override {
        return poolSize;
    }

    Size GetPendingCount() const override {
        if (!mutex) { return 0; }
        if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
            return 0;
        }
        Size n = taskQueueSystem.size() + taskQueueApp.size();
        xSemaphoreGive(mutex);
        return n;
    }

    Bool IsShutdown() const override {
        return shutdownFlag || shutdownNowFlag;
    }

    Bool IsRunning() const override {
        return !shutdownFlag && !shutdownNowFlag;
    }
};

#endif // ARDUINO_THREAD_POOL_H

#endif // ARDUINO
