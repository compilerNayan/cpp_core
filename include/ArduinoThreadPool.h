#ifdef ARDUINO

#ifndef ARDUINO_THREAD_POOL_H
#define ARDUINO_THREAD_POOL_H

#include "IThreadPool.h"
#include "IRunnable.h"
#include <StandardDefines.h>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define THREAD_POOL_ESP32_STACK_HEAVY 8192   // 8 KB for heavy-duty (e.g. Firebase, SSL)
#define THREAD_POOL_ESP32_STACK_LIGHT 4096  // 4 KB for light tasks
#define THREAD_POOL_ESP32_PRIORITY 1
// Core 0 = system core, Core 1 = application core
#define THREAD_POOL_ESP32_SYSTEM_CORE 0
#define THREAD_POOL_ESP32_APP_CORE 1

/*
 * No worker pool: each Execute()/Submit() creates a new FreeRTOS task.
 * Stack size: 8 KB if heavyDuty, else 4 KB. Core from argument.
 */
/* @Component */
class ThreadPool final : public IThreadPool {

    struct ExecuteParam {
        ThreadPool* self;
        IRunnablePtr runnable;
    };

    struct SubmitParam {
        ThreadPool* self;
        std::function<Void()>* func;
    };

    Private SemaphoreHandle_t mutex;
    Private SemaphoreHandle_t allDone;
    Private volatile Bool shutdownFlag;
    Private volatile Bool shutdownNowFlag;
    Private volatile Size runningCount;

    Private Void OnTaskComplete() {
        if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
            if (runningCount > 0) {
                --runningCount;
            }
            if (runningCount == 0 && allDone) {
                xSemaphoreGive(allDone);
            }
            xSemaphoreGive(mutex);
        }
    }

    Private Static Void ExecuteTrampoline(Void* param) {
        ExecuteParam* p = static_cast<ExecuteParam*>(param);
        if (!p || !p->self || !p->runnable) {
            if (p) { delete p; }
            vTaskDelete(nullptr);
            return;
        }
        ThreadPool* self = p->self;
        IRunnablePtr runnable = p->runnable;
        delete p;
        p = nullptr;
        try {
            runnable->Run();
        } catch (...) {
        }
        self->OnTaskComplete();
        vTaskDelete(nullptr);
    }

    Private Static Void SubmitTrampoline(Void* param) {
        SubmitParam* p = static_cast<SubmitParam*>(param);
        if (!p || !p->self) {
            if (p) {
                if (p->func) { delete p->func; }
                delete p;
            }
            vTaskDelete(nullptr);
            return;
        }
        ThreadPool* self = p->self;
        std::function<Void()>* func = p->func;
        delete p;
        p = nullptr;
        try {
            if (func) {
                (*func)();
                delete func;
            }
        } catch (...) {
            if (func) { delete func; }
        }
        self->OnTaskComplete();
        vTaskDelete(nullptr);
    }

    Private BaseType_t CoreToId(ThreadPoolCore core) {
        return (core == ThreadPoolCore::Application) ? THREAD_POOL_ESP32_APP_CORE : THREAD_POOL_ESP32_SYSTEM_CORE;
    }

    Private UBaseType_t StackSize(Bool heavyDuty) {
        return heavyDuty ? THREAD_POOL_ESP32_STACK_HEAVY : THREAD_POOL_ESP32_STACK_LIGHT;
    }

Public
    ThreadPool()
        : mutex(nullptr)
        , allDone(nullptr)
        , shutdownFlag(false)
        , shutdownNowFlag(false)
        , runningCount(0) {
        mutex = xSemaphoreCreateMutex();
        allDone = xSemaphoreCreateBinary();
        if (allDone) {
            xSemaphoreTake(allDone, 0);  // start taken so first completion gives it
        }
    }

    Explicit ThreadPool(CSize /* numThreads */)
        : ThreadPool() {}

    ~ThreadPool() override {
        if (!shutdownFlag && !shutdownNowFlag) {
            Shutdown();
            WaitForCompletion(0);
        }
        if (allDone) {
            vSemaphoreDelete(allDone);
            allDone = nullptr;
        }
        if (mutex) {
            vSemaphoreDelete(mutex);
            mutex = nullptr;
        }
    }

    Bool Submit(std::function<Void()> task) override {
        if (!task || !mutex) return false;
        if (shutdownFlag || shutdownNowFlag) return false;
        std::function<Void()>* copy = new std::function<Void()>(std::move(task));
        if (!copy) return false;
        if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
            delete copy;
            return false;
        }
        if (shutdownFlag || shutdownNowFlag) {
            xSemaphoreGive(mutex);
            delete copy;
            return false;
        }
        ++runningCount;
        xSemaphoreGive(mutex);
        SubmitParam* p = new SubmitParam{this, copy};
        if (!p) {
            delete copy;
            if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
                --runningCount;
                if (runningCount == 0 && allDone) xSemaphoreGive(allDone);
                xSemaphoreGive(mutex);
            }
            return false;
        }
        BaseType_t coreId = THREAD_POOL_ESP32_SYSTEM_CORE;
        UBaseType_t stack = THREAD_POOL_ESP32_STACK_LIGHT;
        TaskHandle_t h = nullptr;
        BaseType_t created = xTaskCreatePinnedToCore(
            SubmitTrampoline, "tp_submit", stack,
            p, THREAD_POOL_ESP32_PRIORITY, &h, coreId);
        if (created != pdPASS) {
            delete p->func;
            delete p;
            if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
                --runningCount;
                if (runningCount == 0 && allDone) xSemaphoreGive(allDone);
                xSemaphoreGive(mutex);
            }
            return false;
        }
        return true;
    }

    Bool Execute(IRunnablePtr runnable, ThreadPoolCore core = ThreadPoolCore::System, Bool heavyDuty = false) override {
        if (!runnable || !mutex) return false;
        if (shutdownFlag || shutdownNowFlag) return false;
        if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
            return false;
        }
        if (shutdownFlag || shutdownNowFlag) {
            xSemaphoreGive(mutex);
            return false;
        }
        ++runningCount;
        xSemaphoreGive(mutex);
        ExecuteParam* p = new ExecuteParam{this, runnable};
        if (!p) {
            if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
                --runningCount;
                if (runningCount == 0 && allDone) xSemaphoreGive(allDone);
                xSemaphoreGive(mutex);
            }
            return false;
        }
        BaseType_t coreId = CoreToId(core);
        UBaseType_t stack = StackSize(heavyDuty);
        TaskHandle_t h = nullptr;
        BaseType_t created = xTaskCreatePinnedToCore(
            ExecuteTrampoline, "tp_exec", stack,
            p, THREAD_POOL_ESP32_PRIORITY, &h, coreId);
        if (created != pdPASS) {
            delete p;
            if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
                --runningCount;
                if (runningCount == 0 && allDone) xSemaphoreGive(allDone);
                xSemaphoreGive(mutex);
            }
            return false;
        }
        return true;
    }

    Void Shutdown() override {
        shutdownFlag = true;
    }

    Void ShutdownNow() override {
        shutdownNowFlag = true;
        shutdownFlag = true;
    }

    Bool WaitForCompletion(CUInt timeoutMs = 0) override {
        if (!mutex) return true;
        TickType_t ticks = (timeoutMs == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
        for (;;) {
            if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
                return false;
            }
            Bool idle = (runningCount == 0);
            xSemaphoreGive(mutex);
            if (idle) return true;
            if (xSemaphoreTake(allDone, ticks) != pdTRUE) {
                return false;
            }
            if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
                return false;
            }
            Bool done = (runningCount == 0);
            xSemaphoreGive(mutex);
            if (done) return true;
        }
    }

    Size GetPoolSize() const override {
        return 0;  // no fixed pool; one task per Execute/Submit
    }

    Size GetPendingCount() const override {
        return 0;  // no queue
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
