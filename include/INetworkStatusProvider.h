#ifndef INETWORKSTATUSPROVIDER_H
#define INETWORKSTATUSPROVIDER_H

#include <StandardDefines.h>

/**
 * Provides network status (WiFi and Internet) to subscribers.
 * Writing (Set*) is thread-safe; reading (Is*) is lock-free and may see a stale value.
 */
DefineStandardPointers(INetworkStatusProvider)
class INetworkStatusProvider {
    Public Virtual ~INetworkStatusProvider() = default;

    // ---------- WiFi status ----------
    /** Current WiFi connected state (lock-free read; may be stale). */
    Public Virtual Bool IsWiFiConnected() const = 0;
    /** Set WiFi connected state (thread-safe). */
    Public Virtual Void SetWiFiConnected(Bool connected) = 0;

    // ---------- Internet status ----------
    /** Current internet reachability (lock-free read; may be stale). */
    Public Virtual Bool IsInternetConnected() const = 0;
    /** Set internet connected state (thread-safe). */
    Public Virtual Void SetInternetConnected(Bool connected) = 0;

    // ---------- WiFi connection ID ----------
    /** Current WiFi connection ID (lock-free read; may be stale). */
    Public Virtual Int GetWifiConnectionId() const = 0;
    /** Set WiFi connection ID (thread-safe). */
    Public Virtual Void SetWifiConnectionId(Int connectionId) = 0;
};

#endif // INETWORKSTATUSPROVIDER_H
