#pragma once

#include <WiFi.h>
#include <WiFiMulti.h>
#include <memory>

class NetworkManager {
    std::unique_ptr<WiFiMulti> wiFiMulti_;
    IPAddress localIp_;
    IPAddress gateway_;
    IPAddress subnet_;

public:
    NetworkManager();

    void connectToNetwork(const char *ssidNetwork, const char *passwordNetwork);

    bool createHotspot(const char *ssidNetwork,
                       const char *passwordNetwork) const;
};
