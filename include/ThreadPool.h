#ifndef ARDUINO

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "IThreadPool.h"
#include <StandardDefines.h>


#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>

/* @Component */
class ThreadPool final : public IThreadPool {

    Private Size poolSize;
    Private StdVector<std::thread> workers;
    Private std::queue<std::function<Void()>> taskQueue;
    Private mutable std::mutex mutex;
    Private std::condition_variable cvTask;
    Private std::condition_variable cvDone;
    Private std::atomic<Bool> shutdown{false};
    Private std::atomic<Bool> shutdownNow{false};
    Private std::atomic<Size> runningCount{0};

    Private Void WorkerLoop() {
        while (true) {
            std::function<Void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex);
                cvTask.wait(lock, [this] {
                    return shutdownNow || (shutdown && taskQueue.empty()) || !taskQueue.empty();
                });
                if (shutdownNow) {
                    return;
                }
                if (shutdown && taskQueue.empty()) {
                    return;
                }
                if (!taskQueue.empty()) {
                    task = std::move(taskQueue.front());
                    taskQueue.pop();
                    ++runningCount;
                }
            }
            if (task) {
                try {
                    task();
                } catch (...) {
                    // Swallow exceptions so one task does not kill the worker
                }
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    --runningCount;
                    cvDone.notify_all();
                }
            }
        }
    }

Public
    ThreadPool() : ThreadPool(4) {}

    Explicit ThreadPool(CSize numThreads)
        : poolSize(numThreads > 0 ? numThreads : 1) {
        workers.reserve(poolSize);
        for (Size i = 0; i < poolSize; ++i) {
            workers.emplace_back(&ThreadPool::WorkerLoop, this);
        }
    }

    ~ThreadPool() override {
        if (!shutdown && !shutdownNow) {
            Shutdown();
            WaitForCompletion(0);
        }
        for (auto& w : workers) {
            if (w.joinable()) {
                w.join();
            }
        }
    }

    // ============================================================================
    // THREAD POOL OPERATIONS
    // ============================================================================

    Bool Submit(std::function<Void()> task) override {
        if (!task) {
            return false;
        }
        std::lock_guard<std::mutex> lock(mutex);
        if (shutdown || shutdownNow) {
            return false;
        }
        taskQueue.push(std::move(task));
        cvTask.notify_one();
        return true;
    }

    Bool Execute(IRunnablePtr runnable, ThreadPoolCore core = ThreadPoolCore::System, Bool heavyDuty = false) override {
        (void)core;
        (void)heavyDuty;  // reserved for later
        if (!runnable) return false;
        return Submit([runnable]() { runnable->Run(); });
    }

    Void Shutdown() override {
        shutdown = true;
        cvTask.notify_all();
    }

    Void ShutdownNow() override {
        shutdownNow = true;
        {
            std::lock_guard<std::mutex> lock(mutex);
            while (!taskQueue.empty()) {
                taskQueue.pop();
            }
        }
        cvTask.notify_all();
    }

    Bool WaitForCompletion(CUInt timeoutMs = 0) override {
        std::unique_lock<std::mutex> lock(mutex);
        auto pred = [this] {
            return taskQueue.empty() && runningCount == 0;
        };
        if (timeoutMs == 0) {
            cvDone.wait(lock, [this, &pred] {
                return pred() || shutdownNow;
            });
            return pred();
        }
        return cvDone.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this, &pred] {
            return pred() || shutdownNow;
        }) && pred();
    }

    Size GetPoolSize() const override {
        return poolSize;
    }

    Size GetPendingCount() const override {
        std::lock_guard<std::mutex> lock(mutex);
        return taskQueue.size();
    }

    Bool IsShutdown() const override {
        return shutdown || shutdownNow;
    }

    Bool IsRunning() const override {
        return !shutdown && !shutdownNow;
    }
};


#endif // THREAD_POOL_H

#endif // ARDUINO
