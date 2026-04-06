/* TimeCoordinator Implementation
 * Copyright (C) 2026  Mauro Freitas
 * 
 * Single Source of Truth for all time-related operations.
 * Uses system timezone support (setenv/tzset) for robust TZ handling.
 * 
 * Critical design:
 * - System time (time(nullptr)): always UTC, converted via localtime()
 * - API data in AUTO mode: already local, use gmtime() (no conversion!)
 * - API data in MANUAL mode: UTC, converted via localtime()
 */

#include "time_coordinator.h"
#include "config.h"
#include "wifi_manager.h"

RTC_DATA_ATTR bool TimeCoordinator::rtcSynced_ = false;
static constexpr time_t MIN_VALID_EPOCH = 1609459200; // 2021-01-01 00:00:00 UTC

void TimeCoordinator::begin() {
    mode_ = (ramTimezoneMode == TIMEZONE_MODE_AUTO) ? 
            TIME_MODE_AUTO : TIME_MODE_MANUAL;
    apiOffsetSeconds_ = 0;
    
    Serial.printf("[TimeCoordinator] Mode: %s\n", 
                  mode_ == TIME_MODE_AUTO ? "AUTO" : "MANUAL");
}

TimeDisplayData TimeCoordinator::process(const owm_resp_onecall_t& apiData,
                                         unsigned long startTimeMillis) {
    TimeDisplayData out = {};
    
    NormalizedWeather norm;
    normalize_(apiData, norm);
    apiOffsetSeconds_ = norm.apiOffsetSeconds;
    
    // Configure system timezone so localtime() works correctly
    // This affects ONLY time(nullptr) conversions, not API data in AUTO mode
    if (mode_ == TIME_MODE_AUTO) {
        configureTimezoneFromOffset_(norm.apiOffsetSeconds);
        syncRtcIfNeeded_(norm.apiOffsetSeconds);
    } else {
        const char *tzString = (ramTimezone[0] != '\0') ? ramTimezone : TIMEZONE;
        configureTimezoneFromString_(tzString);
    }
    
    formatDisplayData_(apiData, norm, out, startTimeMillis);
    
    return out;
}

void TimeCoordinator::configureTimezoneFromString_(const char* tzString) {
    setenv("TZ", tzString, 1);
    tzset();
}

void TimeCoordinator::configureTimezoneFromOffset_(int offsetSeconds) {
    // Convert seconds offset to POSIX TZ format
    // POSIX sign is inverted: UTC-2 means GMT+2 (east of Greenwich)
    int hours = offsetSeconds / 3600;
    int mins = (abs(offsetSeconds) % 3600) / 60;
    
    char tzBuf[16];
    if (mins == 0) {
        snprintf(tzBuf, sizeof(tzBuf), "UTC%+d", -hours);
    } else {
        snprintf(tzBuf, sizeof(tzBuf), "UTC%+d:%02d", -hours, mins);
    }
    
    configureTimezoneFromString_(tzBuf);
    Serial.printf("[TimeCoordinator] TZ set: %s (UTC%+d)\n", tzBuf, offsetSeconds);
}

void TimeCoordinator::normalize_(const owm_resp_onecall_t& src, 
                                 NormalizedWeather& dst) {
    dst.apiOffsetSeconds = src.timezone_offset;
    
    // Current weather - store as raw UTC epoch values from parser
    dst.currentDt = src.current.dt;
    dst.currentSunrise = src.current.sunrise;
    dst.currentSunset = src.current.sunset;
    
    // Daily forecast
    for (int i = 0; i < OWM_NUM_DAILY; i++) {
        dst.daily[i].dt = src.daily[i].dt;
        dst.daily[i].sunrise = src.daily[i].sunrise;
        dst.daily[i].sunset = src.daily[i].sunset;
        dst.daily[i].moonrise = src.daily[i].moonrise;
        dst.daily[i].moonset = src.daily[i].moonset;
    }
    
    // Hourly forecast
    for (int i = 0; i < OWM_NUM_HOURLY; i++) {
        dst.hourlyDt[i] = src.hourly[i].dt;
    }
}

void TimeCoordinator::syncRtcIfNeeded_(int offsetSeconds) {
    if (rtcSynced_) return;
    
    time_t currentTime = time(nullptr);
    if (currentTime > 1000000000) {
        rtcSynced_ = true;
        return;
    }
    
    configTime(offsetSeconds, 0, "pool.ntp.org");
    delay(100);
    
    if (time(nullptr) > 1000000000) {
        rtcSynced_ = true;
    }
}

void TimeCoordinator::formatDisplayData_(const owm_resp_onecall_t& apiData,
                                        const NormalizedWeather& norm,
                                        TimeDisplayData& out,
                                        unsigned long startTimeMillis) {
    // Get current system time - always UTC from time(nullptr)
    // localtime() converts to local using configured TZ
    time_t now = time(nullptr);
    tm tmLocal = {};
    localtime_r(&now, &tmLocal);
    
    // Update time for footer
    snprintf(out.updateTime, sizeof(out.updateTime), 
             "%02d:%02d", tmLocal.tm_hour, tmLocal.tm_min);
    out.rainTime[0] = '\0';
    
    // Find next rain event and pre-format the time string
    for (int i = 0; i < OWM_NUM_HOURLY; i++) {
        if (apiData.hourly[i].dt >= now && apiData.hourly[i].pop >= 0.20f) {
            tm tmRain = {};
            time_t rainTimestamp = static_cast<time_t>(apiData.hourly[i].dt);
            localtime_r(&rainTimestamp, &tmRain);
            strftime(out.rainTime, sizeof(out.rainTime), TIME_FORMAT, &tmRain);
            break;
        }
    }
    
    // Date for header
    strftime(out.displayDate, sizeof(out.displayDate), 
             "%a, %d %b %Y", &tmLocal);
    
    // Forecast - day of week
    // Calculate based on actual API timestamps, not sequential from today
    // This handles cases where API daily[] doesn't start from today (e.g., late night)
    for (int i = 0; i < OWM_NUM_DAILY; i++) {
        tm tmDaily = {};
        time_t dailyDt = norm.daily[i].dt;
        if (dailyDt > MIN_VALID_EPOCH) {
            // Use localtime to get correct weekday from the actual timestamp
            localtime_r(&dailyDt, &tmDaily);
            out.forecastDayOfWeek[i] = tmDaily.tm_wday;
        } else {
            // Fallback: sequential calculation for invalid timestamps
            out.forecastDayOfWeek[i] = (tmLocal.tm_wday + i) % 7;
        }
    }
    
    // Hourly labels
    for (int i = 0; i < OWM_NUM_HOURLY; i++) {
        tm tmHour = {};
        localtime_r(&norm.hourlyDt[i], &tmHour);
        snprintf(out.hourlyLabels[i], sizeof(out.hourlyLabels[0]), 
                 "%02dh", tmHour.tm_hour);
    }
    
    // Sunrise and sunset
    tm tmSunrise = {};
    localtime_r(&norm.currentSunrise, &tmSunrise);
    strftime(out.sunriseTime, sizeof(out.sunriseTime), TIME_FORMAT, &tmSunrise);
    
    tm tmSunset = {};
    localtime_r(&norm.currentSunset, &tmSunset);
    strftime(out.sunsetTime, sizeof(out.sunsetTime), TIME_FORMAT, &tmSunset);

    if (norm.daily[0].moonrise != 0) {
        tm tmMoonrise = {};
        localtime_r(&norm.daily[0].moonrise, &tmMoonrise);
        strftime(out.moonriseTime, sizeof(out.moonriseTime), TIME_FORMAT, &tmMoonrise);
    } else {
        out.moonriseTime[0] = '\0';
    }
    if (norm.daily[0].moonset != 0) {
        tm tmMoonset = {};
        localtime_r(&norm.daily[0].moonset, &tmMoonset);
        strftime(out.moonsetTime, sizeof(out.moonsetTime), TIME_FORMAT, &tmMoonset);
    } else {
        out.moonsetTime[0] = '\0';
    }

    // Hourly start index for graphs and related UI
    out.hourlyStartIndex = 0;
    for (int i = 0; i < OWM_NUM_HOURLY; i++) {
        if (norm.hourlyDt[i] >= now) {
            out.hourlyStartIndex = i;
            break;
        }
    }

    // Sleep duration calculation uses system local time
    out.sleepDurationSeconds = calculateSleep_(&tmLocal);
}

// Break down a timestamp that is already in local time
// This does NOT apply any timezone conversion - just extracts year/month/day/hour/etc
uint64_t TimeCoordinator::calculateSleep_(const tm* localTime) {
    int curHour = localTime->tm_hour;
    int curMin = localTime->tm_min;
    int curSec = localTime->tm_sec;
    
    int bedtimeHour = INT_MAX;
    if (BED_TIME != WAKE_TIME) {
        bedtimeHour = (BED_TIME - WAKE_TIME + 24) % 24;
    }
    
    int relHour = (curHour - WAKE_TIME + 24) % 24;
    int curMinute = relHour * 60 + curMin;
    
    int sleepMinutes = SLEEP_DURATION - (curMinute % SLEEP_DURATION);
    if (sleepMinutes < SLEEP_DURATION / 2) {
        sleepMinutes += SLEEP_DURATION;
    }
    
    int predictedWakeHour = ((curMinute + sleepMinutes) / 60) % 24;
    
    uint64_t sleepDuration;
    if (predictedWakeHour < bedtimeHour) {
        sleepDuration = sleepMinutes * 60 - curSec;
    } else {
        int hoursUntilWake = 24 - relHour;
        sleepDuration = hoursUntilWake * 3600ULL - (curMin * 60ULL + curSec);
    }
    
    sleepDuration += 3ULL;
    sleepDuration *= 1.0015f;
    
    return sleepDuration;
}
