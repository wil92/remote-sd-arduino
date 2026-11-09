#pragma once

#ifndef PLATFORM_NATIVE
#include <EEPROM.h>
#else
#include <Arduino.h>
#endif

#define STRINGIGY(v) #v
#define TOSTRING(v) STRINGIGY(v)

#define IS_ENABLE_FLAG 7
#define CONFIG_STATUS_ADDRESS 0
#define CONFIG_DATA_ADDRESS 1

class FlashManager {
    void saveString(const String &value, int *index);

    String loadString(int *index);

public:
    bool isSetup = false;

    const char *ssid = "RemoteSDDevice";
    const char *password = "remotesd";

    String ssidNetwork = TOSTRING(NETWORK_SSID);
    String passwordNetwork = TOSTRING(NETWORK_PASSWORD);

    void loadSetup();

    void saveSetup();

    bool resetSetup();

    void setVariable(const String &key, const String &value);
};
