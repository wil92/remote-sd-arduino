#include <ArduinoFake.h>
#include <EEPROM.h>
#include <cstring>
#include <string>
#include <unity.h>
#include <vector>

#include "../../../lib/flash/flash_manager.h"

using namespace fakeit;

std::vector<std::string> serialMessages;

void testInitialCredentialsMatchNetworkConfiguration() {
    FlashManager flashManager;

    TEST_ASSERT_EQUAL_STRING(NETWORK_SSID, flashManager.ssidNetwork.c_str());
    TEST_ASSERT_EQUAL_STRING(NETWORK_PASSWORD,
                             flashManager.passwordNetwork.c_str());
    TEST_ASSERT_EQUAL_STRING("Remote SD", flashManager.deviceName.c_str());
}

void testLoadSetupClearsSetupStateWhenFlagIsNotSet() {
    FlashManager flashManager;
    flashManager.isSetup = true;
    When(Method(ArduinoFake(EEPROM), read)).Return(0);

    flashManager.loadSetup();

    TEST_ASSERT_FALSE(flashManager.isSetup);
    TEST_ASSERT_TRUE(serialMessages.empty());
    Verify(Method(ArduinoFake(EEPROM), read).Using(CONFIG_STATUS_ADDRESS))
        .Once();
}

void testLoadSetupRestoresSavedCredentialsWhenFlagIsSet() {
    FlashManager flashManager;
    When(Method(ArduinoFake(EEPROM), read))
        .Return(IS_ENABLE_FLAG, 3, 'n', 'e', 't', 3, 'p', 'w', 'd');

    flashManager.loadSetup();

    TEST_ASSERT_TRUE(flashManager.isSetup);
    TEST_ASSERT_EQUAL_STRING("net", flashManager.ssidNetwork.c_str());
    TEST_ASSERT_EQUAL_STRING("pwd", flashManager.passwordNetwork.c_str());
    TEST_ASSERT_EQUAL_STRING("Remote SD", flashManager.deviceName.c_str());
    TEST_ASSERT_TRUE(serialMessages.empty());
}

void testNameAndCredentialsSurviveReload() {
    uint8_t storage[CONFIG_STORAGE_SIZE];
    std::memset(storage, 0xff, sizeof(storage));
    When(Method(ArduinoFake(EEPROM), write))
        .AlwaysDo([&](int index, uint8_t value) { storage[index] = value; });
    When(Method(ArduinoFake(EEPROM), read))
        .AlwaysDo([&](int index) { return storage[index]; });

    FlashManager first;
    first.setVariable("ssid", "network");
    first.setVariable("password", "password");
    first.setVariable("name", "Printer \"A\" \xc3\xa9");
    TEST_ASSERT_TRUE(first.saveSetup());
    TEST_ASSERT_EQUAL(IS_ENABLE_WITH_NAME_FLAG, storage[0]);

    FlashManager restored;
    restored.loadSetup();
    TEST_ASSERT_TRUE(restored.isSetup);
    TEST_ASSERT_EQUAL_STRING("network", restored.ssidNetwork.c_str());
    TEST_ASSERT_EQUAL_STRING("password", restored.passwordNetwork.c_str());
    TEST_ASSERT_EQUAL_STRING("Printer \"A\" \xc3\xa9", restored.deviceName.c_str());
    TEST_ASSERT_TRUE(serialMessages.empty());
}

void testNameValidationAndStorageBounds() {
    FlashManager manager;
    manager.deviceName = "";
    TEST_ASSERT_FALSE(manager.canSaveSetup());
    manager.deviceName = String(std::string(DEVICE_NAME_MAX_BYTES, 'x').c_str());
    TEST_ASSERT_TRUE(manager.canSaveSetup());
    manager.deviceName += "x";
    TEST_ASSERT_FALSE(manager.canSaveSetup());
    manager.deviceName = "bad\nname";
    TEST_ASSERT_FALSE(manager.canSaveSetup());
    manager.deviceName = "\xc0\xaf";
    TEST_ASSERT_FALSE(manager.canSaveSetup());
    manager.deviceName = "\xed\xa0\x80";
    TEST_ASSERT_FALSE(manager.canSaveSetup());
    manager.deviceName = "\xf4\x90\x80\x80";
    TEST_ASSERT_FALSE(manager.canSaveSetup());
    manager.deviceName = "\xf0\x9f\x96\xa8";
    TEST_ASSERT_TRUE(manager.canSaveSetup());
    manager.ssidNetwork = std::string(255, 's').c_str();
    manager.passwordNetwork = std::string(255, 'p').c_str();
    TEST_ASSERT_FALSE(manager.canSaveSetup());
    manager.deviceName = std::string(64, 'x').c_str();
    manager.passwordNetwork = std::string(189, 'p').c_str();
    TEST_ASSERT_TRUE(manager.canSaveSetup());
    manager.passwordNetwork += "p";
    TEST_ASSERT_FALSE(manager.canSaveSetup());
}

void testResetSetupClearsPersistedFlagAndSetupState() {
    FlashManager flashManager;
    flashManager.isSetup = true;
    When(Method(ArduinoFake(EEPROM), write)).AlwaysReturn();

    TEST_ASSERT_TRUE(flashManager.resetSetup());

    TEST_ASSERT_FALSE(flashManager.isSetup);
    Verify(Method(ArduinoFake(EEPROM), write).Using(CONFIG_STATUS_ADDRESS, 0))
        .Once();
}

void setUp(void) {
    ArduinoFakeReset();
    serialMessages.clear();
    When(OverloadedMethod(ArduinoFake(Serial), println, size_t(const String &)))
        .AlwaysDo([](const String &message) {
            serialMessages.push_back(message.c_str());
            return static_cast<size_t>(0);
        });
}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(testInitialCredentialsMatchNetworkConfiguration);
    RUN_TEST(testLoadSetupClearsSetupStateWhenFlagIsNotSet);
    RUN_TEST(testLoadSetupRestoresSavedCredentialsWhenFlagIsSet);
    RUN_TEST(testNameAndCredentialsSurviveReload);
    RUN_TEST(testNameValidationAndStorageBounds);
    RUN_TEST(testResetSetupClearsPersistedFlagAndSetupState);
    return UNITY_END();
}
