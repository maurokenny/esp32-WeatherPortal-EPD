/// @file state_decision.h
/// @brief Pure deterministic state machine logic for ESP32 firmware
/// @details Separates decision logic from hardware (millis, WiFi) for unit testing.

#ifndef STATE_DECISION_H
#define STATE_DECISION_H

#include <stdint.h>
#include <stddef.h>

/// @brief Firmware operational states (Reused from wifi_manager.h)
enum State {
    STATE_BOOT,
    STATE_WIFI_CONNECTING,
    STATE_AP_CONFIG_MODE,
    STATE_NORMAL_MODE,
    STATE_SLEEP_PENDING,
    STATE_ERROR
};

/// @brief Input variables for the state transition decision
struct DecisionInput {
    bool hasCredentials;      // Credentials exist in RTC RAM
    bool isFirstBoot;         // Device has never successfully completed a cycle
    bool wifiConnected;       // result of WiFi.status() == WL_CONNECTED
    bool wifiTimeout;         // result of timer comparison
    bool ntpSuccess;          // result of NTP sync attempt
    bool apiSuccess;          // result of API fetch attempt
    bool configSaved;         // result of web portal /save action
    bool portalTimeout;       // result of AP mode timer comparison

    uint8_t wifiFailCycles;   // connectionFailCycles
    uint8_t ntpFailCycles;    // ntpFailCycles
    uint8_t apiFailCycles;    // apiFailCycles

    uint8_t maxWifiFail;      // MAX_WIFI_FAIL_CYCLES
    uint8_t maxNtpFail;       // MAX_NTP_FAIL_CYCLES
    uint8_t maxApiFail;       // MAX_API_FAIL_CYCLES
};

/// @brief Desired side effects and next state
struct DecisionOutput {
    State nextState;
    
    // Commands to be executed by the caller
    bool updateFirstBoot;      // Set isFirstBoot = false
    bool resetWifiFail;        // Set connectionFailCycles = 0
    bool incWifiFail;          // Increment connectionFailCycles
    bool resetNtpFail;         // Set ntpFailCycles = 0
    bool incNtpFail;           // Increment ntpFailCycles
    bool resetApiFail;         // Set apiFailCycles = 0
    bool incApiFail;           // Increment apiFailCycles
    bool setErrorFlag;         // Set isErrorState = true (and sleep forever)
};

DecisionOutput decideTransition(State current, const DecisionInput& input);

/**
 * @brief Pure function to calculate deep sleep duration.
 * @param curHour Current local hour (0-23)
 * @param curMin Current local minute (0-59)
 * @param curSec Current local second (0-59)
 * @param bedtime Hour to start extended sleep (e.g., 22)
 * @param wakeTime Hour to wake up (e.g., 7)
 * @param sleepDurationMin Standard sleep interval in minutes (e.g., 30)
 * @return Calculated sleep duration in seconds for the ESP32 timer
 */
uint32_t calculateSleepDuration(int curHour, int curMin, int curSec, 
                                int bedtime, int wakeTime, int sleepDurationMin);

/**
 * @brief Calculate the weekday for a forecast day based on its index.
 * @param todayWday Weekday for the current day (0=Sun, 1=Mon, etc.)
 * @param forecastIndex index of the forecast day (0=today, 1=tomorrow, etc.)
 * @return Weekday for the indexed day (0-6)
 */
int getForecastWeekday(int todayWday, int forecastIndex);

/**
 * @brief Generate the abbreviated localized day name for a forecast day.
 * @param todayWday Weekday for the current day
 * @param forecastIndex index of the forecast day
 * @param buf Output buffer for the name
 * @param size Buffer size
 */
void getForecastDayName(int todayWday, int forecastIndex, char* buf, size_t size);

#endif // STATE_DECISION_H
