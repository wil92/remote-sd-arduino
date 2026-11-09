#include "network_manager.h"

NetworkManager::NetworkManager() {
    localIp_ = IPAddress(192, 168, 1, 1);
    gateway_ = IPAddress(192, 168, 1, 1);
    subnet_ = IPAddress(255, 255, 255, 0);
}

void NetworkManager::connectToNetwork(const char *ssidNetwork,
                                      const char *passwordNetwork) {
    wiFiMulti_.reset(new WiFiMulti());
    wiFiMulti_->addAP(ssidNetwork, passwordNetwork);
    // Serial.print("Connecting to ");
    // Serial.print(ssidNetwork);
    // Serial.println(" ...");

    int i = 0;
    while (wiFiMulti_->run() != WL_CONNECTED) {
        delay(1000);
        // Serial.print('.');
    }

    // Serial.println('\n');
    // Serial.println("Connection established!");
    // Serial.print("IP address:\t");
    // Serial.println(WiFi.localIP());
}

bool NetworkManager::createHotspot(const char *ssidNetwork,
                                   const char *passwordNetwork) const {
    WiFi.softAPConfig(localIp_, gateway_, subnet_);
    int tries = 10;

    // Serial.println("Try to start soft AP");
    while (!WiFi.softAP(ssidNetwork, passwordNetwork) && tries-- > 0) {
        // Serial.print(".");
        delay(1000);
    }
    // Serial.println("");

    if (tries == 0) {
        // Serial.println("Soft AP failed");
        return false;
    }

    // Serial.println("Soft AP ready");
    // Serial.print("Local IP: ");
    // Serial.println(WiFi.softAPIP());
    return true;
}
