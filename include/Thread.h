#ifndef THREAD_H
#define THREAD_H

#include <StandardDefines.h>

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <thread>
#include <chrono>
#endif

/**
 * Cross-platform thread utilities.
 * Sleep() uses delay() on Arduino and std::this_thread::sleep_for on desktop.
 */
class Thread {
    /** Suspend the current execution for the given duration (milliseconds). */
    Public Static Void Sleep(ULong durationMs) {
#ifdef ARDUINO
        delay(static_cast<unsigned long>(durationMs));
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(durationMs));
#endif
    }
};

#endif // THREAD_H
