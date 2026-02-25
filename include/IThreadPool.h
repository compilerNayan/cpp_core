#ifndef I_THREAD_POOL_H
#define I_THREAD_POOL_H

#include <StandardDefines.h>
#include <functional>
#include <memory>
#include "IRunnable.h"
#include "ThreadPoolCore.h"

DefineStandardPointers(IThreadPool)
class IThreadPool {

    Public Virtual ~IThreadPool() = default;

    // ============================================================================
    // THREAD POOL OPERATIONS
    // ============================================================================

    /**
     * @brief Submits a task to be executed by a worker thread
     * @param task Callable (e.g. std::function<void()>) to run on the pool
     * @return true if task was accepted, false if pool is shutdown or full
     */
    Public Virtual Bool Submit(std::function<Void()> task) = 0;

    /**
     * @brief Submits a runnable to be executed by a worker thread on the specified core.
     * @param runnable The runnable to execute
     * @param core System core (0) or Application core (1); default System
     * @return true if task was accepted, false if pool is shutdown, full, or runnable is null
     */
    Public Virtual Bool Execute(IRunnablePtr runnable, ThreadPoolCore core = ThreadPoolCore::System) = 0;

    /**
     * @brief Submits a runnable by type: creates an instance of T (default constructor) and executes Run() on it.
     * @tparam T Runnable type (must inherit from IRunnable)
     * @param core System core (0) or Application core (1); default System
     * @return true if task was accepted, false if pool is shutdown or full
     */
    template<typename T>
    Bool Execute(ThreadPoolCore core = ThreadPoolCore::System) {
        return Execute(std::make_shared<T>(), core);
    }

    /**
     * @brief Stops accepting new tasks; already queued tasks may still run
     */
    Public Virtual Void Shutdown() = 0;

    /**
     * @brief Stops the pool and does not wait for pending tasks to finish
     */
    Public Virtual Void ShutdownNow() = 0;

    /**
     * @brief Blocks until all submitted tasks have completed or timeout
     * @param timeoutMs Maximum time to wait in milliseconds (0 = wait indefinitely)
     * @return true if all tasks completed, false if timeout occurred
     */
    Public Virtual Bool WaitForCompletion(CUInt timeoutMs = 0) = 0;

    /**
     * @brief Number of worker threads in the pool
     */
    Public Virtual Size GetPoolSize() const = 0;

    /**
     * @brief Number of tasks currently queued and not yet started
     */
    Public Virtual Size GetPendingCount() const = 0;

    /**
     * @brief Whether Shutdown() or ShutdownNow() has been called
     */
    Public Virtual Bool IsShutdown() const = 0;

    /**
     * @brief Whether the pool is running (not shutdown)
     */
    Public Virtual Bool IsRunning() const = 0;
};

#endif // I_THREAD_POOL_H
