#ifdef ARDUINO

#ifndef ARDUINO_THREAD_POOL_H
#define ARDUINO_THREAD_POOL_H

#include "IThreadPool.h"
#include <StandardDefines.h>
#include <queue>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define THREAD_POOL_ESP32_STACK_SIZE 8192
#define THREAD_POOL_ESP32_PRIORITY 1
#define THREAD_POOL_ESP32_MAX_QUEUE_SIGNALS 512

/* @Component */
class ThreadPool final : public IThreadPool {

    Private Size poolSize;
    Private std::queue<std::function<Void()>*> taskQueue;
    Private SemaphoreHandle_t mutex;
    Private SemaphoreHandle_t taskAvailable;
    Private SemaphoreHandle_t allDone;
    Private SemaphoreHandle_t workersExited;
    Private volatile Bool shutdownFlag;
    Private volatile Bool shutdownNowFlag;
    Private volatile Size runningCount;
    Private StdVector<TaskHandle_t> workerHandles;

    Private Void WorkerLoop() {
        for (;;) {
            if (xSemaphoreTake(taskAvailable, portMAX_DELAY) != pdTRUE) {
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
                if (shutdownFlag && taskQueue.empty()) {
                    xSemaphoreGive(mutex);
                    if (workersExited) { xSemaphoreGive(workersExited); }
                    vTaskDelete(nullptr);
                    return;
                }
                if (!taskQueue.empty()) {
                    taskPtr = taskQueue.front();
                    taskQueue.pop();
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
                    if (taskQueue.empty() && runningCount == 0) {
                        xSemaphoreGive(allDone);
                    }
                    xSemaphoreGive(mutex);
                }
            }
        }
    }

    Private Static Void WorkerTrampoline(Void* param) {
        ThreadPool* self = static_cast<ThreadPool*>(param);
        if (self) {
            self->WorkerLoop();
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
        mutex = xSemaphoreCreateMutex();
        taskAvailable = xSemaphoreCreateCounting(THREAD_POOL_ESP32_MAX_QUEUE_SIGNALS, 0);
        allDone = xSemaphoreCreateBinary();
        workersExited = xSemaphoreCreateCounting(static_cast<UBaseType_t>(poolSize), 0);
        if (!mutex || !taskAvailable || !allDone || !workersExited) {
            if (mutex) { vSemaphoreDelete(mutex); mutex = nullptr; }
            if (taskAvailable) { vSemaphoreDelete(taskAvailable); taskAvailable = nullptr; }
            if (allDone) { vSemaphoreDelete(allDone); allDone = nullptr; }
            if (workersExited) { vSemaphoreDelete(workersExited); workersExited = nullptr; }
            poolSize = 0;
            return;
        }
        workerHandles.reserve(poolSize);
        for (Size i = 0; i < poolSize; ++i) {
            TaskHandle_t h = nullptr;
            xTaskCreate(WorkerTrampoline, "tp_worker", THREAD_POOL_ESP32_STACK_SIZE,
                        this, THREAD_POOL_ESP32_PRIORITY, &h);
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
        if (taskAvailable) {
            vSemaphoreDelete(taskAvailable);
            taskAvailable = nullptr;
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
        std::function<Void()>* copy = new std::function<Void()>(std::move(task));
        if (!copy) {
            xSemaphoreGive(mutex);
            return false;
        }
        taskQueue.push(copy);
        xSemaphoreGive(mutex);
        xSemaphoreGive(taskAvailable);
        return true;
    }

    Void Shutdown() override {
        shutdownFlag = true;
        for (Size i = 0; i < poolSize; ++i) {
            xSemaphoreGive(taskAvailable);
        }
    }

    Void ShutdownNow() override {
        shutdownNowFlag = true;
        if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
            while (!taskQueue.empty()) {
                std::function<Void()>* t = taskQueue.front();
                taskQueue.pop();
                delete t;
            }
            xSemaphoreGive(mutex);
        }
        for (Size i = 0; i < poolSize; ++i) {
            xSemaphoreGive(taskAvailable);
        }
    }

    Bool WaitForCompletion(CUInt timeoutMs = 0) override {
        if (!mutex) { return true; }
        TickType_t ticks = (timeoutMs == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
        for (;;) {
            if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
                return false;
            }
            Bool idle = taskQueue.empty() && (runningCount == 0);
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
            Bool done = taskQueue.empty() && (runningCount == 0);
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
        Size n = taskQueue.size();
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
