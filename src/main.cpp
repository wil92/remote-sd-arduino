#include "main.h"

#include <Arduino.h>

#include "../lib/flash/flash_manager.h"
#include "../lib/network/access_point_config.h"
#include "../lib/network/network_manager.h"
#include "../lib/reset/reset_button.h"
#include "../lib/webserver/web_server_manager.h"

#define RESET_CONFIG_PIN GPIO_NUM_20
#define SWITCH_SD_PIN GPIO_NUM_21

NetworkManager networkManager;
FlashManager flashManager;
WebServerManager webServer;
SDCardManager sdCardManager;
ResetButton resetButton;

void resetConfigWhenButtonPress() {
    resetButton.check(
        digitalRead(RESET_CONFIG_PIN) == LOW,
        []() {
            Serial.println("Reset configuration requested");
            if (!flashManager.resetSetup()) {
                Serial.println("Failed to clear saved configuration");
                return false;
            }
            return true;
        },
        []() { ESP.restart(); });
}

void enableSD(bool status) { digitalWrite(SWITCH_SD_PIN, status ? LOW : HIGH); }

void setup() {
    Serial.begin(9600);

    EEPROM.begin(512);

    flashManager.loadSetup();

    pinMode(RESET_CONFIG_PIN, INPUT);
    pinMode(SWITCH_SD_PIN, OUTPUT);

    resetConfigWhenButtonPress();

    enableSD(false);
    webServer.checkResetButton = resetConfigWhenButtonPress;

    if (flashManager.isSetup) {
        networkManager.connectToNetwork(flashManager.ssidNetwork.c_str(),
                                        flashManager.passwordNetwork.c_str(),
                                        resetConfigWhenButtonPress);

        webServer.flashManager = &flashManager;
        webServer.sdCardManager = &sdCardManager;
        webServer.rf = []() { ESP.restart(); };
        webServer.sf = enableSD;
        webServer.setup();
        webServer.run();
    } else {
        if (!networkManager.createHotspot(AP_SSID, AP_PASSWORD,
                                          resetConfigWhenButtonPress)) {
            ESP.restart();
        }
        delay(100);

        webServer.flashManager = &flashManager;
        webServer.rf = []() { ESP.restart(); };
        webServer.setup();
        webServer.run();
    }
}

void loop() {
    resetConfigWhenButtonPress();
    webServer.loop();
}
