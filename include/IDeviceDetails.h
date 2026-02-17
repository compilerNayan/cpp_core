#ifndef IDEVICEDETAILS_H
#define IDEVICEDETAILS_H

#include <StandardDefines.h>

/**
 * Provides device identity/details (serial number, secret, version).
 * Values are typically set at compile time via macros.
 */
DefineStandardPointers(IDeviceDetails)
class IDeviceDetails {
    Public Virtual ~IDeviceDetails() = default;

    /** Device serial number. */
    Public Virtual StdString GetSerialNumber() const = 0;

    /** Device secret (e.g. for auth). */
    Public Virtual StdString GetDeviceSecret() const = 0;

    /** Firmware/application version. */
    Public Virtual StdString GetVersion() const = 0;
};

#endif // IDEVICEDETAILS_H
