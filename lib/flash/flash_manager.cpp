#include "flash_manager.h"

#include "../text_utils.h"

namespace {
bool validDeviceName(const String &name) {
    return name.length() > 0 && name.length() <= DEVICE_NAME_MAX_BYTES &&
           isValidUtf8(name, true);
}
}

void FlashManager::loadSetup() {
    const int status = EEPROM.read(CONFIG_STATUS_ADDRESS);
    isSetup = status == IS_ENABLE_FLAG || status == IS_ENABLE_WITH_NAME_FLAG;
    deviceName = DEFAULT_DEVICE_NAME;
    if (!isSetup) {
        return;
    }

    int index = CONFIG_DATA_ADDRESS;
    ssidNetwork = loadString(&index);
    passwordNetwork = loadString(&index);
    if (status == IS_ENABLE_WITH_NAME_FLAG) {
        String savedName = loadString(&index);
        if (validDeviceName(savedName)) {
            deviceName = savedName;
        } else {
            Serial.println("Invalid saved device name; using default");
        }
    }
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

bool FlashManager::canSaveSetup() const {
    return validDeviceName(deviceName) && ssidNetwork.length() <= 255 &&
           passwordNetwork.length() <= 255 &&
           CONFIG_DATA_ADDRESS + 3 + ssidNetwork.length() +
                   passwordNetwork.length() + deviceName.length() <=
               CONFIG_STORAGE_SIZE;
}

bool FlashManager::saveSetup() {
    if (!canSaveSetup()) {
        Serial.println("Invalid configuration; not saved");
        return false;
    }
    EEPROM.write(CONFIG_STATUS_ADDRESS, IS_ENABLE_WITH_NAME_FLAG);

    int index = CONFIG_DATA_ADDRESS;

    saveString(ssidNetwork, &index);
    saveString(passwordNetwork, &index);
    saveString(deviceName, &index);

#ifdef ARDUINO_ARCH_ESP32
    if (!EEPROM.commit()) {
        Serial.println("Failed to save configuration");
        return false;
    }
#endif
    return true;
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
    if (*index >= CONFIG_STORAGE_SIZE) {
        Serial.println("Invalid saved configuration length");
        return "";
    }
    const int len = EEPROM.read((*index)++);
    if (len > CONFIG_STORAGE_SIZE - *index) {
        Serial.println("Invalid saved configuration length");
        *index = CONFIG_STORAGE_SIZE;
        return "";
    }
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
    } else if (key == "name") {
        deviceName = value;
    }
}
