#include "network_manager.h"

#include <ESPmDNS.h>

namespace {
void waitWithResetButtonChecks(const std::function<void()> &checkResetButton) {
    for (int i = 0; i < 10; ++i) {
        if (checkResetButton) {
            checkResetButton();
        }
        delay(100);
    }
}
}

NetworkManager::NetworkManager() {
    localIp_ = IPAddress(192, 168, 1, 1);
    gateway_ = IPAddress(192, 168, 1, 1);
    subnet_ = IPAddress(255, 255, 255, 0);
}

bool NetworkManager::connectToNetwork(
    const char *ssidNetwork, const char *passwordNetwork,
    const std::function<void()> &checkResetButton) {
    if (!ssidNetwork || !ssidNetwork[0] || !passwordNetwork) {
        Serial.println("Cannot connect: Wi-Fi configuration is invalid");
        return false;
    }

    WiFi.persistent(false);
    if (!WiFi.mode(WIFI_STA)) {
        Serial.println("Cannot connect: failed to enable Wi-Fi station mode");
        return false;
    }
    if (!WiFi.setAutoReconnect(true)) {
        Serial.println("Cannot connect: failed to enable Wi-Fi auto-reconnect");
        return false;
    }
    if (WiFi.begin(ssidNetwork, passwordNetwork) == WL_CONNECT_FAILED) {
        Serial.println("Cannot start Wi-Fi connection");
        return false;
    }

    int lastStatus = -1;
    while (true) {
        const wl_status_t status = WiFi.status();
        if (status == WL_CONNECTED) {
            break;
        }
        if (static_cast<int>(status) != lastStatus) {
            lastStatus = static_cast<int>(status);
            Serial.print("Wi-Fi connection status: ");
            Serial.println(lastStatus);
        }
        waitWithResetButtonChecks(checkResetButton);
    }
    Serial.println("Connected. IP address: " + WiFi.localIP().toString());
    return true;
}

bool NetworkManager::startDiscovery(const String &hostname) const {
    if (!MDNS.begin(hostname)) {
        Serial.println("Failed to start mDNS");
        return false;
    }
    if (!MDNS.addService("remote-sd", "tcp", 80)) {
        Serial.println("Failed to register mDNS discovery service");
        MDNS.end();
        return false;
    }
    return true;
}

bool NetworkManager::startScan() const {
    WiFi.scanDelete();
    return WiFi.scanNetworks(true) != WIFI_SCAN_FAILED;
}

int NetworkManager::scanComplete() const {
    const int result = WiFi.scanComplete();
    if (result == WIFI_SCAN_RUNNING) return NETWORK_SCAN_RUNNING;
    return result < 0 ? NETWORK_SCAN_FAILED : result;
}

String NetworkManager::scanSsid(int index) const { return WiFi.SSID(index); }

bool NetworkManager::createHotspot(
    const char *ssidNetwork, const char *passwordNetwork,
    const std::function<void()> &checkResetButton) const {
    if (!WiFi.mode(WIFI_AP_STA) ||
        !WiFi.softAPConfig(localIp_, gateway_, subnet_)) {
        Serial.println("Failed to configure setup access point");
        return false;
    }
    int tries = 10;

    while (!WiFi.softAP(ssidNetwork, passwordNetwork) && tries-- > 0) {
        waitWithResetButtonChecks(checkResetButton);
    }

    if (tries == 0) {
        return false;
    }

    return true;
}
