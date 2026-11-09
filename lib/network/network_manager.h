#pragma once

#include <WiFi.h>
#include <WiFiMulti.h>
#include <functional>
#include <memory>

class NetworkManager {
    std::unique_ptr<WiFiMulti> wiFiMulti_;
    IPAddress localIp_;
    IPAddress gateway_;
    IPAddress subnet_;

public:
    NetworkManager();

    void connectToNetwork(const char *ssidNetwork, const char *passwordNetwork,
                          const std::function<void()> &checkResetButton = {});

    bool createHotspot(const char *ssidNetwork,
                       const char *passwordNetwork,
                       const std::function<void()> &checkResetButton = {}) const;
};
