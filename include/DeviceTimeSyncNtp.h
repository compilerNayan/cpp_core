#ifndef DEVICETIMESYNCNTP_H
#define DEVICETIMESYNCNTP_H

#include "IDeviceTime.h"
#include <StandardDefines.h>
#include <ILogger.h>

#ifdef ARDUINO

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <time.h>
#include <sys/time.h>

/**
 * Syncs device time from NTP via our own UDP request/response, then settimeofday().
 * Does not rely on configTime() / built-in SNTP. Tries multiple servers (IPs first).
 * Requires WiFi to be connected before calling SyncTimeFromNetwork().
 */
/* @Component */
class DeviceTimeSyncNtp : public IDeviceTime {
    /* @Autowired */
    Private ILoggerPtr logger;

    Private Static const ULong kNtpPort = 123;
    Private Static const ULong kNtpTimeoutMs = 8000u;
    Private Static const ULong kNtpEpochOffsetSec = 2208988800ULL;  // 1900-01-01 to 1970-01-01
    Private Static const time_t kMinValidEpochSec = 1000000000;     // sanity: >= 2001

    Private Static const char* const* GetNtpServers() {
        static const char* const list[] = {
            "129.6.15.28",    // time.nist.gov
            "162.159.200.1",  // time.cloudflare.com
            "216.239.35.0",   // time.google.com
            "pool.ntp.org",
        };
        return list;
    }
    Private Static const Int kNtpServerCount = 4;

    /** Send NTP request to serverIp:123, read response, return Unix seconds. 0 = failure. */
    Private time_t FetchTimeFromNtp(WiFiUDP& udp, IPAddress& serverIp) const {
        byte packet[48];
        memset(packet, 0, sizeof(packet));
        packet[0] = 0x23;  // LI=0, VN=4, Mode=3 (client)

        if (!udp.beginPacket(serverIp, (unsigned int)kNtpPort)) return 0;
        udp.write(packet, sizeof(packet));
        if (!udp.endPacket()) return 0;

        ULong start = (ULong)millis();
        int size = 0;
        while ((ULong)millis() - start < kNtpTimeoutMs) {
            size = udp.parsePacket();
            if (size >= 48) break;
            delay(10);
        }
        if (size < 48) return 0;
        if (udp.read(packet, 48) != 48) return 0;
        udp.stop();

        ULong ntpSec = (ULong)packet[40] << 24 | (ULong)packet[41] << 16
                     | (ULong)packet[42] << 8  | (ULong)packet[43];
        ULong unixSec = ntpSec - kNtpEpochOffsetSec;
        if (unixSec < (ULong)kMinValidEpochSec) return 0;
        return (time_t)unixSec;
    }

    Private Bool SetDeviceTime(time_t unixSec) const {
        struct timeval tv;
        tv.tv_sec = unixSec;
        tv.tv_usec = 0;
        return settimeofday(&tv, nullptr) == 0;
    }

    Public DeviceTimeSyncNtp() = default;

    Public ~DeviceTimeSyncNtp() override = default;

    Public Virtual Bool SyncTimeFromNetwork() override {
        logger->Info(Tag::Untagged, StdString("[DeviceTimeSyncNtp] Syncing time from NTP (UDP + settimeofday)..."));
        WiFiUDP udp;
        if (!udp.begin(8888)) {
            logger->Error(Tag::Untagged, StdString("[DeviceTimeSyncNtp] UDP begin(8888) failed."));
            return false;
        }

        const char* const* servers = GetNtpServers();
        for (Int i = 0; i < kNtpServerCount; i++) {
            const char* host = servers[i];
            IPAddress ip;
            if (!WiFi.hostByName(host, ip)) {
                logger->Warning(Tag::Untagged, StdString("[DeviceTimeSyncNtp] DNS failed for ") + host);
                continue;
            }
            logger->Info(Tag::Untagged, StdString("[DeviceTimeSyncNtp] Trying ") + host + "...");
            time_t unixSec = FetchTimeFromNtp(udp, ip);
            if (unixSec == 0) {
                logger->Warning(Tag::Untagged, StdString("[DeviceTimeSyncNtp] No reply from ") + host);
                continue;
            }
            udp.stop();
            if (!SetDeviceTime(unixSec)) {
                logger->Error(Tag::Untagged, StdString("[DeviceTimeSyncNtp] settimeofday() failed."));
                return false;
            }
            logger->Info(Tag::Untagged, StdString("[DeviceTimeSyncNtp] Time synced from ") + host + " (UTC).");
            return true;
        }
        udp.stop();
        logger->Error(Tag::Untagged, StdString("[DeviceTimeSyncNtp] Time sync failed (tried all servers)."));
        return false;
    }

    Public Virtual ULongLong GetCurrentTimeMsFromEpoch() override {
        time_t sec = time(nullptr);
        return sec > 0 ? (ULongLong)sec * 1000ULL : 0ULL;
    }
};

#else

/**
 * Stub implementation when not on Arduino: no-op, returns false.
 */
/* @Component */
class DeviceTimeSyncNtp : public IDeviceTime {
    Public DeviceTimeSyncNtp() = default;

    Public ~DeviceTimeSyncNtp() override = default;

    Public Virtual Bool SyncTimeFromNetwork() override {
        return false;
    }

    Public Virtual ULongLong GetCurrentTimeMsFromEpoch() override {
        return 0ULL;
    }
};

#endif // ARDUINO

#endif // DEVICETIMESYNCNTP_H
