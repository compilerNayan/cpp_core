#ifndef IDEVICETIMESYNC_H
#define IDEVICETIMESYNC_H

#include <StandardDefines.h>

/**
 * Syncs the device system time from a standard internet time source (e.g. NTP).
 * Implementations typically use pool.ntp.org or similar.
 */
DefineStandardPointers(IDeviceTimeSync)
class IDeviceTimeSync {
    Public Virtual ~IDeviceTimeSync() = default;

    /**
     * Sets the device time from the network (e.g. NTP).
     * Blocks until sync completes or times out.
     * @return true if the device time was set successfully, false otherwise.
     */
    Public Virtual Bool SyncTimeFromNetwork() = 0;
};

#endif // IDEVICETIMESYNC_H
