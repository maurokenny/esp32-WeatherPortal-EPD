/// @file time_coordinator.cpp
/// @brief TimeCoordinator implementation - centralized time management
/// @copyright Copyright (C) 2026 Mauro Freitas
/// @license GNU General Public License v3.0
///
/// @details
/// Single Source of Truth for all time-related operations.
/// Uses system timezone support (setenv/tzset) for robust TZ handling.
///
/// Critical design principles:
/// - System time (time(nullptr)): always UTC, converted via localtime()
/// - API data in AUTO mode: already local, use gmtime() (no conversion!)
/// - API data in MANUAL mode: UTC, converted via localtime()
///
/// Architecture: API Response -> Parser -> TimeCoordinator -> UI / PowerManager

#include "time_coordinator.h"
#include "config.h"
#include "wifi_manager.h"

RTC_DATA_ATTR bool TimeCoordinator::rtcSynced_ = false;

/// @brief Initialize TimeCoordinator
/// @details Detects mode from ramTimezoneMode and configures system timezone
void TimeCoordinator::begin() {
    mode_ = (ramTimezoneMode == TIMEZONE_MODE_AUTO) ? 
            TIME_MODE_AUTO : TIME_MODE_MANUAL;
    apiOffsetSeconds_ = 0;
    
    Serial.printf("[TimeCoordinator] Mode: %s\n", 
                  mode_ == TIME_MODE_AUTO ? "AUTO" : "MANUAL");
}

/// @brief Process API data and generate display-ready time information
/// @param apiData Raw weather API response
/// @param startTimeMillis Device startup time from millis()
/// @return TimeDisplayData with all pre-formatted time strings
/// @note Does not modify apiData - creates normalized copy internally
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

/// @brief Configure system timezone from POSIX string
/// @param tzString POSIX timezone string (e.g., "EST5EDT,M3.2.0,M11.1.0")
/// @details Calls setenv("TZ") and tzset() to configure libc timezone
void TimeCoordinator::configureTimezoneFromString_(const char* tzString) {
    setenv("TZ", tzString, 1);
    tzset();
}

/// @brief Configure system timezone from UTC offset
/// @param offsetSeconds Offset from UTC in seconds (positive=east)
/// @details Generates POSIX TZ string and configures system timezone
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

/// @brief Normalize API response timestamps
/// @param src Source API response
/// @param dst Normalized output structure
/// @details Copies and converts timestamps to local time as needed
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

/// @brief Synchronize system RTC with NTP if needed
/// @param offsetSeconds Local timezone offset for RTC configuration
/// @details Only syncs once per boot cycle (tracked via rtcSynced_)
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

/// @brief Format all display strings from normalized data
/// @param apiData Raw API response (for rain lookup)
/// @param norm Normalized timestamps
/// @param out Output display data structure
/// @param startTimeMillis Device startup time
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
    
    // Forecast - base day of week
    // Use the current local system weekday and render following days sequentially.
    out.todayDayOfWeek = tmLocal.tm_wday;
    
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

/// @brief Calculate deep sleep duration from local time
/// @param localTime Current local time structure
/// @return Sleep duration in seconds
/// @details Implements wake/sleep schedule logic based on BED_TIME/WAKE_TIME.
/// Does NOT apply timezone conversion - assumes localTime is already localized.
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
