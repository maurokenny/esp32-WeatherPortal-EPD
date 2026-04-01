/* TimeCoordinator Implementation
 * 
 * Single Source of Truth for all time-related operations.
 * Uses system timezone support (setenv/tzset) for robust TZ handling.
 */

#include "time_coordinator.h"
#include "config.h"
#include "wifi_manager.h"  // For ramTimezoneMode

// ═══════════════════════════════════════════════════════════════════════════
// Static Members
// ═══════════════════════════════════════════════════════════════════════════

RTC_DATA_ATTR bool TimeCoordinator::rtcSynced_ = false;

// ═══════════════════════════════════════════════════════════════════════════
// Public Methods
// ═══════════════════════════════════════════════════════════════════════════

void TimeCoordinator::begin() {
    mode_ = (ramTimezoneMode == TIMEZONE_MODE_AUTO) ? 
            TIME_MODE_AUTO : TIME_MODE_MANUAL;
    apiOffsetSeconds_ = 0;
    
    if (mode_ == TIME_MODE_MANUAL) {
        // Configure system timezone using POSIX TZ string from config.h
        configureTimezone_();
    }
    
    Serial.printf("[TimeCoordinator] Mode: %s\n", 
                  mode_ == TIME_MODE_AUTO ? "AUTO" : "MANUAL");
}

TimeDisplayData TimeCoordinator::process(const owm_resp_onecall_t& apiData,
                                         unsigned long startTimeMillis) {
    TimeDisplayData out = {};  // Zero-initialize
    
    // STEP 1: Normalize API data (convert timestamps if needed)
    NormalizedWeather norm;
    normalize_(apiData, norm);
    apiOffsetSeconds_ = norm.apiOffsetSeconds;
    
    // STEP 2: For AUTO mode, sync RTC with API offset
    if (mode_ == TIME_MODE_AUTO) {
        syncRtcIfNeeded_(norm.apiOffsetSeconds);
    }
    
    // STEP 3: Format everything for UI
    // localtime() will automatically apply the configured timezone
    formatDisplayData_(norm, out, startTimeMillis);
    
    return out;
}

// ═══════════════════════════════════════════════════════════════════════════
// Private Methods
// ═══════════════════════════════════════════════════════════════════════════

void TimeCoordinator::configureTimezone_() {
    // Set system timezone using POSIX TZ string from config.h
    // Example: "CET-1CEST,M3.5.0,M10.5.0/3" or "EST5EDT,M3.2.0,M11.1.0"
    setenv("TZ", TIMEZONE, 1);
    tzset();
    
    Serial.printf("[TimeCoordinator] Timezone set to: %s\n", TIMEZONE);
}

void TimeCoordinator::normalize_(const owm_resp_onecall_t& src, 
                                 NormalizedWeather& dst) {
    dst.apiOffsetSeconds = src.timezone_offset;
    
    // Current weather
    if (mode_ == TIME_MODE_MANUAL) {
        // In MANUAL mode, API returns UTC, localtime() will convert
        dst.currentDt = src.current.dt;
    } else {
        // In AUTO mode, API returns local time
        dst.currentDt = src.current.dt;
    }
    
    // Daily forecast (coalesced struct for cache locality)
    for (int i = 0; i < OWM_NUM_DAILY; i++) {
        // Store as-is; localtime() will handle conversion in MANUAL mode
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
    // Check 1: Already synced this session?
    if (rtcSynced_) {
        return;
    }
    
    // Check 2: Did RTC survive deep sleep? (reasonable epoch)
    time_t currentTime = time(nullptr);
    if (currentTime > 1000000000) {  // > 2001-09-09
        rtcSynced_ = true;
        return;
    }
    
    // Need to sync - configure with API offset
    configTime(offsetSeconds, 0, "pool.ntp.org");
    delay(100);
    
    time_t after = time(nullptr);
    if (after > 1000000000) {
        rtcSynced_ = true;
    }
}

void TimeCoordinator::formatDisplayData_(const NormalizedWeather& norm,
                                        TimeDisplayData& out,
                                        unsigned long startTimeMillis) {
    // Get current time and convert to local using system timezone
    time_t now = time(nullptr);
    tm tmLocal = {};
    localtime_r(&now, &tmLocal);
    
    // Time for footer (e.g., "14:32")
    snprintf(out.updateTime, sizeof(out.updateTime), 
             "%02d:%02d", tmLocal.tm_hour, tmLocal.tm_min);
    
    // Date for header
    strftime(out.displayDate, sizeof(out.displayDate), 
             "%a, %d %b %Y", &tmLocal);
    
    // Forecast - day of week for each day
    for (int i = 0; i < OWM_NUM_DAILY; i++) {
        tm tmDaily = {};
        localtime_r(&norm.daily[i].dt, &tmDaily);
        out.forecastDayOfWeek[i] = tmDaily.tm_wday;
    }
    
    // Hourly labels for outlook graph
    for (int i = 0; i < OWM_NUM_HOURLY; i++) {
        tm tmHour = {};
        localtime_r(&norm.hourlyDt[i], &tmHour);
        snprintf(out.hourlyLabels[i], sizeof(out.hourlyLabels[0]), 
                 "%02dh", tmHour.tm_hour);
    }
    
    // Sleep duration
    out.sleepDurationSeconds = calculateSleep_(&tmLocal);
}

uint64_t TimeCoordinator::calculateSleep_(const tm* localTime) {
    int curHour = localTime->tm_hour;
    int curMin = localTime->tm_min;
    int curSec = localTime->tm_sec;
    
    // Calculate time relative to WAKE_TIME
    int bedtimeHour = INT_MAX;
    if (BED_TIME != WAKE_TIME) {
        bedtimeHour = (BED_TIME - WAKE_TIME + 24) % 24;
    }
    
    int relHour = (curHour - WAKE_TIME + 24) % 24;
    int curMinute = relHour * 60 + curMin;
    
    // Calculate minutes until next update
    int sleepMinutes = SLEEP_DURATION - (curMinute % SLEEP_DURATION);
    if (sleepMinutes < SLEEP_DURATION / 2) {
        sleepMinutes += SLEEP_DURATION;
    }
    
    // Check if wake time falls in sleep period
    int predictedWakeHour = ((curMinute + sleepMinutes) / 60) % 24;
    
    uint64_t sleepDuration;
    if (predictedWakeHour < bedtimeHour) {
        sleepDuration = sleepMinutes * 60 - curSec;
    } else {
        int hoursUntilWake = 24 - relHour;
        sleepDuration = hoursUntilWake * 3600ULL - (curMin * 60ULL + curSec);
    }
    
    // Compensation for fast RTCs
    sleepDuration += 3ULL;
    sleepDuration *= 1.0015f;
    
    return sleepDuration;
}
