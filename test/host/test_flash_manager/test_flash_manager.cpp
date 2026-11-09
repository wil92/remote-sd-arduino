#include <ArduinoFake.h>
#include <EEPROM.h>
#include <unity.h>

#include "../../../lib/flash/flash_manager.h"

using namespace fakeit;

void testLoadSetupClearsSetupStateWhenFlagIsNotSet() {
    FlashManager flashManager;
    flashManager.isSetup = true;
    When(Method(ArduinoFake(EEPROM), read)).Return(0);

    flashManager.loadSetup();

    TEST_ASSERT_FALSE(flashManager.isSetup);
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

void setUp(void) { ArduinoFakeReset(); }

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(testLoadSetupClearsSetupStateWhenFlagIsNotSet);
    RUN_TEST(testLoadSetupRestoresSavedCredentialsWhenFlagIsSet);
    RUN_TEST(testResetSetupClearsPersistedFlagAndSetupState);
    return UNITY_END();
}
