#include "flash_manager.h"

void FlashManager::loadSetup() {
    isSetup = EEPROM.read(CONFIG_STATUS_ADDRESS) == IS_ENABLE_FLAG;
    if (!isSetup) {
        return;
    }

    int index = CONFIG_DATA_ADDRESS;
    ssidNetwork = loadString(&index);
    passwordNetwork = loadString(&index);
}

bool FlashManager::resetSetup() {
    EEPROM.write(CONFIG_STATUS_ADDRESS, 0);
#ifdef ARDUINO_ARCH_ESP32
    if (!EEPROM.commit()) {
        return false;
    }
#endif
    isSetup = false;
    return true;
}

void FlashManager::saveSetup() {
    EEPROM.write(CONFIG_STATUS_ADDRESS, IS_ENABLE_FLAG);

    int index = CONFIG_DATA_ADDRESS;

    // network
    saveString(ssidNetwork, &index);
    saveString(passwordNetwork, &index);

#ifdef ARDUINO_ARCH_ESP32
    EEPROM.commit();
#endif
    // Serial.println("Data saved in flash memory");
}

void FlashManager::saveString(const String &value, int *index) {
    EEPROM.write(*index, value.length());
    (*index)++;
    for (int i = 0; i < static_cast<int>(value.length()); i++) {
        EEPROM.write(*index, value[i]);
        (*index)++;
    }
}

String FlashManager::loadString(int *index) {
    const int len = EEPROM.read((*index)++);
    String result = "";
    for (int i = 0; i < len; i++) {
        result += static_cast<char>(EEPROM.read((*index)++));
    }
    return result;
}

void FlashManager::setVariable(const String &key, const String &value) {
    if (key == "ssid") {
        ssidNetwork = value.c_str();
    } else if (key == "password") {
        passwordNetwork = value.c_str();
    }
}
