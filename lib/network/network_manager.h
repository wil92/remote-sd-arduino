#pragma once

#include <WiFi.h>
#include <functional>

#include "../scan_status.h"

class NetworkManager {
    IPAddress localIp_;
    IPAddress gateway_;
    IPAddress subnet_;

public:
    NetworkManager();

    bool connectToNetwork(const char *ssidNetwork, const char *passwordNetwork,
                          const std::function<void()> &checkResetButton = {});

    bool startDiscovery(const String &hostname) const;

    bool startScan() const;

    int scanComplete() const;

    String scanSsid(int index) const;

    bool createHotspot(const char *ssidNetwork,
                       const char *passwordNetwork,
                       const std::function<void()> &checkResetButton = {}) const;
};
