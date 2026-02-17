#ifndef NETWORKSTATUSPROVIDER_H
#define NETWORKSTATUSPROVIDER_H

#include "INetworkStatusProvider.h"
#include <atomic>

/**
 * Implementation of INetworkStatusProvider using std::atomic<bool>.
 * Writes are thread-safe; reads are lock-free (subscribers may see a stale value).
 */
/* @Component */
class NetworkStatusProvider : public INetworkStatusProvider {
    Private std::atomic<bool> wifiConnected_{false};
    Private std::atomic<bool> internetConnected_{false};
    Private std::atomic<int> wifiConnectionId_{0};

    Public NetworkStatusProvider() = default;

    Public ~NetworkStatusProvider() override = default;

    Public Virtual Bool IsWiFiConnected() const override {
        return wifiConnected_.load(std::memory_order_relaxed);
    }

    Public Virtual Void SetWiFiConnected(Bool connected) override {
        wifiConnected_.store(connected ? true : false, std::memory_order_release);
    }

    Public Virtual Bool IsInternetConnected() const override {
        return internetConnected_.load(std::memory_order_relaxed);
    }

    Public Virtual Void SetInternetConnected(Bool connected) override {
        internetConnected_.store(connected ? true : false, std::memory_order_release);
    }

    Public Virtual Int GetWifiConnectionId() const override {
        return wifiConnectionId_.load(std::memory_order_relaxed);
    }

    Public Virtual Void SetWifiConnectionId(Int connectionId) override {
        wifiConnectionId_.store(connectionId, std::memory_order_release);
    }
};

#endif // NETWORKSTATUSPROVIDER_H
