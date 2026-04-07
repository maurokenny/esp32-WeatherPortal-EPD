/**
 * @file state_decision.cpp
 * @brief Pure function for state machine decisions. 
 * @details No hardware, no WiFi, no Arduino dependencies.
 */

#include "state_decision.h"
#include "_strftime.h"

DecisionOutput decideTransition(State current, const DecisionInput& input) {
    DecisionOutput output = {};
    output.nextState = current; // Default stay in current state
    
    // Clear side effects by default
    output.updateFirstBoot = false;
    output.resetWifiFail = false;
    output.incWifiFail = false;
    output.resetNtpFail = false;
    output.incNtpFail = false;
    output.resetApiFail = false;
    output.incApiFail = false;
    output.setErrorFlag = false;

    switch (current) {
        case STATE_BOOT:
            if (input.hasCredentials) {
                output.nextState = STATE_WIFI_CONNECTING;
            } else {
                output.nextState = STATE_AP_CONFIG_MODE;
            }
            break;

        case STATE_WIFI_CONNECTING:
            if (input.wifiConnected) {
                output.nextState = STATE_NORMAL_MODE;
                output.resetWifiFail = true;
                if (input.isFirstBoot) {
                    output.updateFirstBoot = true;
                }
            } else if (input.wifiTimeout) {
                if (input.isFirstBoot) {
                    output.nextState = STATE_AP_CONFIG_MODE;
                } else {
                    // Logic to retry or go to error
                    output.incWifiFail = true;
                    uint8_t nextFailCount = input.wifiFailCycles + 1;
                    
                    if (input.maxWifiFail == 0) {
                        // Infinite retry
                        output.nextState = STATE_SLEEP_PENDING;
                    } else if (nextFailCount < input.maxWifiFail) {
                        output.nextState = STATE_SLEEP_PENDING;
                    } else {
                        output.nextState = STATE_ERROR;
                        output.setErrorFlag = true;
                    }
                }
            }
            break;

        case STATE_AP_CONFIG_MODE:
            if (input.configSaved) {
                output.nextState = STATE_BOOT;
                output.resetWifiFail = true;
            } else if (input.portalTimeout) {
                output.nextState = STATE_ERROR;
                output.setErrorFlag = true;
            }
            break;

        case STATE_NORMAL_MODE:
            // This is usually reached after success or handled by individual failures
            output.nextState = STATE_SLEEP_PENDING;
            
            if (input.ntpSuccess) {
                output.resetNtpFail = true;
            } else {
                output.incNtpFail = true;
                if (input.maxNtpFail > 0 && (input.ntpFailCycles + 1) >= input.maxNtpFail) {
                    output.nextState = STATE_ERROR;
                    output.setErrorFlag = true;
                }
            }
            
            if (input.apiSuccess) {
                output.resetApiFail = true;
            } else {
                output.incApiFail = true;
                if (input.maxApiFail > 0 && (input.apiFailCycles + 1) >= input.maxApiFail) {
                    output.nextState = STATE_ERROR;
                    output.setErrorFlag = true;
                }
            }
            break;

        case STATE_SLEEP_PENDING:
            // Stay in this state until hardware reset/deep sleep restart
            output.nextState = STATE_SLEEP_PENDING;
            break;

        case STATE_ERROR:
            // Terminal state
            output.nextState = STATE_ERROR;
            break;
    }

    return output;
}

uint32_t calculateSleepDuration(int curHour, int curMin, int curSec, 
                                int bedtime, int wakeTime, int sleepDurationMin) {
    if (bedtime == wakeTime) {
        // Sleep for standard interval if no bedtime is defined
        int relMinute = curMin % sleepDurationMin;
        int sleepMin = sleepDurationMin - relMinute;
        if (sleepMin < sleepDurationMin / 2) {
            sleepMin += sleepDurationMin;
        }
        return (uint32_t)((sleepMin * 60 - curSec + 3) * 1.0015f);
    }

    // Translate hours to a scale relative to WAKE_TIME (0 = wake time)
    int relHour = (curHour - wakeTime + 24) % 24;
    int curMinuteTotal = relHour * 60 + curMin;
    
    // Bedtime in the same relative scale
    int bedtimeRelHour = (bedtime - wakeTime + 24) % 24;
    
    // Standard sleep interval
    int sleepMinutes = sleepDurationMin - (curMinuteTotal % sleepDurationMin);
    if (sleepMinutes < 5) { // Ensure at least 5 mins of sleep
        sleepMinutes += sleepDurationMin;
    }
    
    int predictedWakeRelHour = ((curMinuteTotal + sleepMinutes) / 60) % 24;
    
    uint32_t sleepDurationSec;
    // Decision: standard sleep or sleep until wakeTime
    if (predictedWakeRelHour < bedtimeRelHour) {
        // Daytime: next wake is before bedtime
        sleepDurationSec = sleepMinutes * 60LL - curSec;
    } else {
        // Nighttime: next wake would be during/after bedtime
        int hoursUntilWake = 24 - relHour;
        sleepDurationSec = hoursUntilWake * 3600ULL - (curMin * 60ULL + curSec);
    }
    
    // Apply ESP32 drift correction (3s buffer + 0.15% clock correction)
    return (uint32_t)((sleepDurationSec + 3) * 1.0015f);
}

int getForecastWeekday(int todayWday, int forecastIndex) {
    return (todayWday + forecastIndex) % 7;
}

void getForecastDayName(int todayWday, int forecastIndex, char* buf, size_t size) {
    if (buf == nullptr || size == 0) return;
    
    int targetWday = getForecastWeekday(todayWday, forecastIndex);
    
    struct tm tmpTm = {};
    tmpTm.tm_wday = targetWday;
    
    // Use the project's custom strftime for locale support
    _strftime(buf, size, "%a", &tmpTm);
}

