#ifndef THREAD_POOL_STACK_SIZE_H
#define THREAD_POOL_STACK_SIZE_H

#include <cstddef>

/**
 * Stack size hint for IThreadPool::Execute (e.g. FreeRTOS task stack in bytes on ESP32).
 * Platforms that ignore per-task stack still accept the parameter for a uniform API.
 */
enum class ThreadPoolStackSize {
    KB_1,
    KB_2,
    KB_3,
    KB_4,
    KB_5,
    KB_6,
    KB_7,
    KB_8,
    KB_9,
    KB_10,
    KB_11,
    KB_12,
    KB_13,
    KB_14,
    KB_15,
    KB_16,
    KB_24,
    KB_32,
};

constexpr std::size_t ThreadPoolStackBytes(ThreadPoolStackSize size) {
    constexpr std::size_t K = 1024;
    switch (size) {
        case ThreadPoolStackSize::KB_1:
            return 1 * K;
        case ThreadPoolStackSize::KB_2:
            return 2 * K;
        case ThreadPoolStackSize::KB_3:
            return 3 * K;
        case ThreadPoolStackSize::KB_4:
            return 4 * K;
        case ThreadPoolStackSize::KB_5:
            return 5 * K;
        case ThreadPoolStackSize::KB_6:
            return 6 * K;
        case ThreadPoolStackSize::KB_7:
            return 7 * K;
        case ThreadPoolStackSize::KB_8:
            return 8 * K;
        case ThreadPoolStackSize::KB_9:
            return 9 * K;
        case ThreadPoolStackSize::KB_10:
            return 10 * K;
        case ThreadPoolStackSize::KB_11:
            return 11 * K;
        case ThreadPoolStackSize::KB_12:
            return 12 * K;
        case ThreadPoolStackSize::KB_13:
            return 13 * K;
        case ThreadPoolStackSize::KB_14:
            return 14 * K;
        case ThreadPoolStackSize::KB_15:
            return 15 * K;
        case ThreadPoolStackSize::KB_16:
            return 16 * K;
        case ThreadPoolStackSize::KB_24:
            return 24 * K;
        case ThreadPoolStackSize::KB_32:
            return 32 * K;
    }
    return 8 * K;
}

#endif // THREAD_POOL_STACK_SIZE_H
