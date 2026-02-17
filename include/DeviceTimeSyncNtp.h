#ifndef DEVICETIMESYNCNTP_H
#define DEVICETIMESYNCNTP_H

#include "IDeviceTimeSync.h"
#include <StandardDefines.h>

#ifdef ARDUINO

#include <Arduino.h>
#include <time.h>
#if defined(ESP8266)
#include <coredecls.h>
#endif

/**
 * Syncs device time from NTP (pool.ntp.org). Arduino/ESP only.
 * Uses UTC (gmtOffset 0). Blocks up to timeoutMs for NTP response.
 * Requires WiFi to be connected before calling SyncTimeFromNetwork().
 */
/* @Component */
class DeviceTimeSyncNtp : public IDeviceTimeSync {
    Private Static const char* kNtpServer() { return "pool.ntp.org"; }
    Private Static const Int kGmtOffsetSec = 0;
    Private Static const Int kDaylightOffsetSec = 0;
    Private Static const ULong kTimeoutMs = 10000u;

    Public DeviceTimeSyncNtp() = default;

    Public ~DeviceTimeSyncNtp() override = default;

    Public Virtual Bool SyncTimeFromNetwork() override {
        configTime(kGmtOffsetSec, kDaylightOffsetSec, kNtpServer());
        ULong start = (ULong)millis();
        while ((ULong)millis() - start < kTimeoutMs) {
            time_t now = time(nullptr);
            if (now > 0) {
                return true;
            }
            delay(200);
        }
        return false;
    }
};

#else

/**
 * Stub implementation when not on Arduino: no-op, returns false.
 */
/* @Component */
class DeviceTimeSyncNtp : public IDeviceTimeSync {
    Public DeviceTimeSyncNtp() = default;

    Public ~DeviceTimeSyncNtp() override = default;

    Public Virtual Bool SyncTimeFromNetwork() override {
        return false;
    }
};

#endif // ARDUINO

#endif // DEVICETIMESYNCNTP_H
