/**
 * @file test_state_machine.cpp
 * @brief Unit tests for the state machine decision logic.
 * @details Runs on native host (Linux/Mac/PC) using Unity test framework.
 */

#include <unity.h>
#include "state_decision.h"

// Set up Unity test suite
void setUp(void) {
    // This is run before each test
}

void tearDown(void) {
    // This is run after each test
}

// ═══════════════════════════════════════════════════════════════════════════
// BOOT TESTS
// ═══════════════════════════════════════════════════════════════════════════

void test_boot_no_credentials_goes_to_ap_mode(void) {
    DecisionInput input = {};
    input.hasCredentials = false;
    
    DecisionOutput output = decideTransition(STATE_BOOT, input);
    
    TEST_ASSERT_EQUAL(STATE_AP_CONFIG_MODE, output.nextState);
}

void test_boot_with_credentials_goes_to_wifi_connecting(void) {
    DecisionInput input = {};
    input.hasCredentials = true;
    
    DecisionOutput output = decideTransition(STATE_BOOT, input);
    
    TEST_ASSERT_EQUAL(STATE_WIFI_CONNECTING, output.nextState);
}

// ═══════════════════════════════════════════════════════════════════════════
// WIFI CONNECTING TESTS
// ═══════════════════════════════════════════════════════════════════════════

void test_wifi_success_goes_to_normal_mode(void) {
    DecisionInput input = {};
    input.wifiConnected = true;
    input.isFirstBoot = true;
    
    DecisionOutput output = decideTransition(STATE_WIFI_CONNECTING, input);
    
    TEST_ASSERT_EQUAL(STATE_NORMAL_MODE, output.nextState);
    TEST_ASSERT_TRUE(output.resetWifiFail);
    TEST_ASSERT_TRUE(output.updateFirstBoot); // isFirstBoot becomes false
}

void test_first_boot_wifi_timeout_goes_to_ap_mode(void) {
    DecisionInput input = {};
    input.isFirstBoot = true;
    input.wifiConnected = false;
    input.wifiTimeout = true;
    
    DecisionOutput output = decideTransition(STATE_WIFI_CONNECTING, input);
    
    TEST_ASSERT_EQUAL(STATE_AP_CONFIG_MODE, output.nextState);
    TEST_ASSERT_FALSE(output.incWifiFail); // Counter doesn't increment on first boot
}

void test_subsequent_boot_wifi_timeout_goes_to_sleep_pending(void) {
    DecisionInput input = {};
    input.isFirstBoot = false; // Post-first boot
    input.wifiConnected = false;
    input.wifiTimeout = true;
    input.wifiFailCycles = 0;
    input.maxWifiFail = 10;
    
    DecisionOutput output = decideTransition(STATE_WIFI_CONNECTING, input);
    
    TEST_ASSERT_EQUAL(STATE_SLEEP_PENDING, output.nextState);
    TEST_ASSERT_TRUE(output.incWifiFail);
}

void test_wifi_max_failures_reaches_error_state(void) {
    DecisionInput input = {};
    input.isFirstBoot = false;
    input.wifiConnected = false;
    input.wifiTimeout = true;
    input.wifiFailCycles = 9;   // Current is 9
    input.maxWifiFail = 10;    // Limit is 10
    
    DecisionOutput output = decideTransition(STATE_WIFI_CONNECTING, input);
    
    TEST_ASSERT_EQUAL(STATE_ERROR, output.nextState);
    TEST_ASSERT_TRUE(output.incWifiFail);
    TEST_ASSERT_TRUE(output.setErrorFlag);
}

void test_wifi_infinite_retry_when_max_is_zero(void) {
    DecisionInput input = {};
    input.isFirstBoot = false;
    input.wifiConnected = false;
    input.wifiTimeout = true;
    input.wifiFailCycles = 99; // Very high count
    input.maxWifiFail = 0;    // Infinite retries
    
    DecisionOutput output = decideTransition(STATE_WIFI_CONNECTING, input);
    
    TEST_ASSERT_EQUAL(STATE_SLEEP_PENDING, output.nextState);
    TEST_ASSERT_TRUE(output.incWifiFail);
}

// ═══════════════════════════════════════════════════════════════════════════
// AP_CONFIG_MODE TESTS
// ═══════════════════════════════════════════════════════════════════════════

void test_ap_mode_config_saved_goes_to_boot(void) {
    DecisionInput input = {};
    input.configSaved = true;
    
    DecisionOutput output = decideTransition(STATE_AP_CONFIG_MODE, input);
    
    TEST_ASSERT_EQUAL(STATE_BOOT, output.nextState);
    TEST_ASSERT_TRUE(output.resetWifiFail);
}

void test_ap_mode_timeout_goes_to_error(void) {
    DecisionInput input = {};
    input.portalTimeout = true;
    
    DecisionOutput output = decideTransition(STATE_AP_CONFIG_MODE, input);
    
    TEST_ASSERT_EQUAL(STATE_ERROR, output.nextState);
    TEST_ASSERT_TRUE(output.setErrorFlag);
}

// ═══════════════════════════════════════════════════════════════════════════
// NORMAL_MODE TESTS
// ═══════════════════════════════════════════════════════════════════════════

void test_normal_mode_success_goes_to_sleep_pending(void) {
    DecisionInput input = {};
    input.ntpSuccess = true;
    input.apiSuccess = true;
    
    DecisionOutput output = decideTransition(STATE_NORMAL_MODE, input);
    
    TEST_ASSERT_EQUAL(STATE_SLEEP_PENDING, output.nextState);
    TEST_ASSERT_TRUE(output.resetNtpFail);
    TEST_ASSERT_TRUE(output.resetApiFail);
}

void test_api_failure_leads_to_error_if_max_reached(void) {
    DecisionInput input = {};
    input.ntpSuccess = true;
    input.apiSuccess = false;
    input.apiFailCycles = 2; // Current is 2
    input.maxApiFail = 3;    // Limit is 3
    
    DecisionOutput output = decideTransition(STATE_NORMAL_MODE, input);
    
    TEST_ASSERT_EQUAL(STATE_ERROR, output.nextState);
    TEST_ASSERT_TRUE(output.incApiFail);
    TEST_ASSERT_TRUE(output.setErrorFlag);
}

// ═══════════════════════════════════════════════════════════════════════════
// EDGE CASES
// ═══════════════════════════════════════════════════════════════════════════

void test_max_is_one_immediate_error_on_second_attempt(void) {
    DecisionInput input = {};
    input.isFirstBoot = false;
    input.wifiConnected = false;
    input.wifiTimeout = true;
    input.wifiFailCycles = 0; // Current is 0
    input.maxWifiFail = 1;    // Error after 1 failure
    
    DecisionOutput output = decideTransition(STATE_WIFI_CONNECTING, input);
    
    TEST_ASSERT_EQUAL(STATE_ERROR, output.nextState);
    TEST_ASSERT_TRUE(output.incWifiFail);
}

void test_is_first_boot_reset_only_once(void) {
    DecisionInput input = {};
    input.wifiConnected = true;
    input.isFirstBoot = false; // Already second boot
    
    DecisionOutput output = decideTransition(STATE_WIFI_CONNECTING, input);
    
    TEST_ASSERT_FALSE(output.updateFirstBoot); // Should not update if already false
}

// ═══════════════════════════════════════════════════════════════════════════
// SLEEP DURATION TESTS
// ═══════════════════════════════════════════════════════════════════════════

void test_sleep_duration_daytime(void) {
    // 14:00:00 (H:M:S), Bed: 22h, Wake: 07h, Interval: 30min
    uint32_t sleep = calculateSleepDuration(14, 0, 0, 22, 7, 30);
    // Standard interval is 30m (1800s) + 3s buffer * drift
    // 1803 * 1.0015 = 1805.7
    TEST_ASSERT_INT_WITHIN(5, 1806, sleep);
}

void test_sleep_duration_bedtime_trigger(void) {
    // 22:05:00 (Night), Bed: 22h, Wake: 07h, Interval: 30min
    uint32_t sleep = calculateSleepDuration(22, 5, 0, 22, 7, 30);
    // Should sleep until 07:00 (8h 55m = 32100s)
    // 32103 * 1.0015 = 32151.15
    TEST_ASSERT_INT_WITHIN(60, 32151, sleep);
}

void test_sleep_duration_midnight_cross(void) {
    // 01:00:00 (Night), Bed: 22h, Wake: 07h, Interval: 30min
    uint32_t sleep = calculateSleepDuration(1, 0, 0, 22, 7, 30);
    // Should sleep until 07:00 (6h = 21600s)
    // 21603 * 1.0015 = 21635.4
    TEST_ASSERT_INT_WITHIN(60, 21635, sleep);
}

void test_sleep_duration_no_bedtime_loop(void) {
    // Bedtime == WakeTime means infinite loop of standard intervals
    uint32_t sleep = calculateSleepDuration(23, 0, 0, 0, 0, 30);
    TEST_ASSERT_INT_WITHIN(5, 1806, sleep);
}

void test_wake_at_7am_wifi_fail_goes_to_sleep_not_ap_mode(void) {
    // Scenario: User configured WiFi 2 days ago. Device wakes up at 07:00 AM.
    // Router is slow to respond, WiFi connection times out.
    DecisionInput input = {};
    input.isFirstBoot = false;     // Already connected in the past!
    input.hasCredentials = true; 
    input.wifiConnected = false;
    input.wifiTimeout = true;      // Failed to connect this time
    
    DecisionOutput output = decideTransition(STATE_WIFI_CONNECTING, input);
    
    // MUST go to SLEEP_PENDING (to retry later), NOT AP_CONFIG_MODE
    TEST_ASSERT_EQUAL(STATE_SLEEP_PENDING, output.nextState);
    TEST_ASSERT_TRUE(output.incWifiFail);
}

void test_forecast_weekday_cycling(void) {
    // Basic cycling
    TEST_ASSERT_EQUAL(1, getForecastWeekday(0, 1)); // Sun -> Mon
    TEST_ASSERT_EQUAL(2, getForecastWeekday(0, 2)); // Sun -> Tue
    
    // Rollover
    TEST_ASSERT_EQUAL(0, getForecastWeekday(6, 1)); // Sat -> Sun
    TEST_ASSERT_EQUAL(1, getForecastWeekday(6, 2)); // Sat -> Mon
    TEST_ASSERT_EQUAL(6, getForecastWeekday(2, 4)); // Tue -> Sat
}

void test_forecast_day_names_en(void) {
    char buf[8];
    
    // Testing English names (en_US set in platformio.ini)
    getForecastDayName(0, 0, buf, sizeof(buf)); // Sunday
    TEST_ASSERT_EQUAL_STRING("Sun", buf);
    
    getForecastDayName(0, 1, buf, sizeof(buf)); // Monday
    TEST_ASSERT_EQUAL_STRING("Mon", buf);
    
    getForecastDayName(6, 1, buf, sizeof(buf)); // Saturday -> Sunday
    TEST_ASSERT_EQUAL_STRING("Sun", buf);
}

// Main runner for native environment
int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Boot
    RUN_TEST(test_boot_no_credentials_goes_to_ap_mode);
    RUN_TEST(test_boot_with_credentials_goes_to_wifi_connecting);
    
    // WiFi Connecting
    RUN_TEST(test_wifi_success_goes_to_normal_mode);
    RUN_TEST(test_first_boot_wifi_timeout_goes_to_ap_mode);
    RUN_TEST(test_subsequent_boot_wifi_timeout_goes_to_sleep_pending);
    RUN_TEST(test_wifi_max_failures_reaches_error_state);
    RUN_TEST(test_wifi_infinite_retry_when_max_is_zero);
    
    // AP mode
    RUN_TEST(test_ap_mode_config_saved_goes_to_boot);
    RUN_TEST(test_ap_mode_timeout_goes_to_error);
    
    // Normal mode
    RUN_TEST(test_normal_mode_success_goes_to_sleep_pending);
    RUN_TEST(test_api_failure_leads_to_error_if_max_reached);
    
    // Edge cases
    RUN_TEST(test_max_is_one_immediate_error_on_second_attempt);
    RUN_TEST(test_is_first_boot_reset_only_once);

    // Sleep duration
    RUN_TEST(test_sleep_duration_daytime);
    RUN_TEST(test_sleep_duration_bedtime_trigger);
    RUN_TEST(test_sleep_duration_midnight_cross);
    RUN_TEST(test_sleep_duration_no_bedtime_loop);

    // Forecast Days
    RUN_TEST(test_forecast_weekday_cycling);
    RUN_TEST(test_forecast_day_names_en);
    
    // Specific real-world scenario
    RUN_TEST(test_wake_at_7am_wifi_fail_goes_to_sleep_not_ap_mode);
    
    return UNITY_END();
}

