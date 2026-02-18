#ifndef IDEVICETIME_H
#define IDEVICETIME_H

#include <StandardDefines.h>

/**
 * Device time: sync from network (e.g. NTP) and get current UTC time in ms from epoch.
 */
DefineStandardPointers(IDeviceTime)
class IDeviceTime {
    Public Virtual ~IDeviceTime() = default;

    /**
     * Sets the device time from the network (e.g. NTP).
     * Blocks until sync completes or times out.
     * @return true if the device time was set successfully, false otherwise.
     */
    Public Virtual Bool SyncTimeFromNetwork() = 0;

    /**
     * Returns current UTC time in milliseconds from Unix epoch (1970-01-01 00:00:00 UTC).
     * Only valid after a successful SyncTimeFromNetwork(); otherwise may return 0 or a small value.
     */
    Public Virtual ULongLong GetCurrentTimeMsFromEpoch() = 0;
};

#endif // IDEVICETIME_H
