#ifndef IRUNNABLE_H
#define IRUNNABLE_H

#include <StandardDefines.h>

/**
 * Interface for a runnable task (Java Runnable–style).
 * Implement Run() to define the task body.
 */
DefineStandardPointers(IRunnable)
class IRunnable {
    Public Virtual ~IRunnable() = default;

    /** Execute the task. */
    Public Virtual Void Run() = 0;
};

#endif // IRUNNABLE_H
