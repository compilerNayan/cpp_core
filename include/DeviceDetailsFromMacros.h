#ifndef DEVICEDETAILSFROMMACROS_H
#define DEVICEDETAILSFROMMACROS_H

#include "IDeviceDetails.h"
#include <StandardDefines.h>

#ifndef DEVICE_SERIAL_NUMBER
#define DEVICE_SERIAL_NUMBER "AX9STEMN7K"
#endif
#ifndef DEVICE_SECRET
#define DEVICE_SECRET "dummy-secret"
#endif
#ifndef DEVICE_VERSION
#define DEVICE_VERSION "0.0.0"
#endif

/**
 * Implementation of IDeviceDetails from compile-time macros (strings).
 * Define when building e.g. -DDEVICE_SERIAL_NUMBER=\"SN001\" -DDEVICE_SECRET=\"mysecret\" -DDEVICE_VERSION=\"1.0.0\".
 * If not defined, the defaults above are used.
 */
/* @Component */
class DeviceDetailsFromMacros : public IDeviceDetails {
    Public DeviceDetailsFromMacros() = default;

    Public ~DeviceDetailsFromMacros() override = default;

    Public Virtual StdString GetSerialNumber() const override {
        return StdString(DEVICE_SERIAL_NUMBER);
    }

    Public Virtual StdString GetDeviceSecret() const override {
        return StdString(DEVICE_SECRET);
    }

    Public Virtual StdString GetVersion() const override {
        return StdString(DEVICE_VERSION);
    }
};

#endif // DEVICEDETAILSFROMMACROS_H
