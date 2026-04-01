/* TimeCoordinator Implementation
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
        configureTimezoneFromString_(TIMEZONE);
    }
    
    formatDisplayData_(norm, out, startTimeMillis);
    
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
    
    // Current weather - store as-is, conversion done in format step
    dst.currentDt = src.current.dt;
    
    // Daily forecast
    for (int i = 0; i < OWM_NUM_DAILY; i++) {
        dst.daily[i].dt = src.daily[i].dt;
        dst.daily[i].sunrise = src.daily[i].sunrise;
        dst.daily[i].sunset = src.daily[i].sunset;
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

void TimeCoordinator::formatDisplayData_(const NormalizedWeather& norm,
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
    
    // Date for header
    strftime(out.displayDate, sizeof(out.displayDate), 
             "%a, %d %b %Y", &tmLocal);
    
    // Forecast - day of week
    // CRITICAL: In AUTO mode, API data is already local, use gmtime() (no TZ conversion)
    // In MANUAL mode, API data is UTC, use localtime() (apply TZ conversion)
    for (int i = 0; i < OWM_NUM_DAILY; i++) {
        tm tmDaily = {};
        if (mode_ == TIME_MODE_AUTO) {
            gmtime_r(&norm.daily[i].dt, &tmDaily);  // No conversion - already local
        } else {
            localtime_r(&norm.daily[i].dt, &tmDaily);  // Convert UTC to local
        }
        out.forecastDayOfWeek[i] = tmDaily.tm_wday;
    }
    
    // Hourly labels
    for (int i = 0; i < OWM_NUM_HOURLY; i++) {
        tm tmHour = {};
        if (mode_ == TIME_MODE_AUTO) {
            gmtime_r(&norm.hourlyDt[i], &tmHour);  // No conversion - already local
        } else {
            localtime_r(&norm.hourlyDt[i], &tmHour);  // Convert UTC to local
        }
        snprintf(out.hourlyLabels[i], sizeof(out.hourlyLabels[0]), 
                 "%02dh", tmHour.tm_hour);
    }
    
    // Sleep duration calculation uses system local time
    out.sleepDurationSeconds = calculateSleep_(&tmLocal);
}

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
