#pragma once

#ifdef ARDUINO_ARCH_ESP32
#include <EEPROM.h>
#else
#include <Arduino.h>
#endif

#if __has_include("../../include/network_config.h")
#include "../../include/network_config.h"
#else
#include "../../include/network_config.example.h"
#endif

#define IS_ENABLE_FLAG 7
#define IS_ENABLE_WITH_NAME_FLAG 8
#define CONFIG_STATUS_ADDRESS 0
#define CONFIG_DATA_ADDRESS 1
#define CONFIG_STORAGE_SIZE 512
#define DEVICE_NAME_MAX_BYTES 64
#define DEFAULT_DEVICE_NAME "Remote SD"

class FlashManager {
    void saveString(const String &value, int *index);

    String loadString(int *index);

public:
    bool isSetup = false;

    String ssidNetwork = NETWORK_SSID;
    String passwordNetwork = NETWORK_PASSWORD;
    String deviceName = DEFAULT_DEVICE_NAME;

    void loadSetup();

    bool canSaveSetup() const;

    bool saveSetup();

    bool resetSetup();

    void setVariable(const String &key, const String &value);
};
