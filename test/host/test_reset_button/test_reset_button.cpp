#include <unity.h>

#include "../../../lib/reset/reset_button.h"

void testReleasedButtonDoesNothing() {
    ResetButton button;
    int clears = 0;
    int restarts = 0;

    button.check(false, [&]() { ++clears; return true; },
                 [&]() { ++restarts; });

    TEST_ASSERT_EQUAL(0, clears);
    TEST_ASSERT_EQUAL(0, restarts);
}

void testPressClearsConfigurationBeforeRestarting() {
    ResetButton button;
    int state = 0;

    button.check(true, [&]() { state = 1; return true; },
                 [&]() { TEST_ASSERT_EQUAL(1, state); state = 2; });

    TEST_ASSERT_EQUAL(2, state);
}

void testFailedClearDoesNotRestartAndCanBeRetriedAfterRelease() {
    ResetButton button;
    int clears = 0;
    int restarts = 0;
    bool clearSucceeds = false;
    auto clear = [&]() { ++clears; return clearSucceeds; };
    auto restart = [&]() { ++restarts; };

    button.check(true, clear, restart);
    button.check(true, clear, restart);
    TEST_ASSERT_EQUAL(1, clears);
    TEST_ASSERT_EQUAL(0, restarts);

    button.check(false, clear, restart);
    clearSucceeds = true;
    button.check(true, clear, restart);
    TEST_ASSERT_EQUAL(2, clears);
    TEST_ASSERT_EQUAL(1, restarts);
}

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(testReleasedButtonDoesNothing);
    RUN_TEST(testPressClearsConfigurationBeforeRestarting);
    RUN_TEST(testFailedClearDoesNotRestartAndCanBeRetriedAfterRelease);
    return UNITY_END();
}
