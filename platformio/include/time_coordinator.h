/* TimeCoordinator - Single Source of Truth for Time
 * Copyright (C) 2026  Mauro Freitas
 * 
 * Centralizes all timezone logic and time conversions.
 * No other module should interpret timezones or apply offsets.
 * 
 * Architecture:
 *   API Response -> Parser -> TimeCoordinator -> UI / PowerManager
 * 
 * Modes:
 *   - MANUAL: API returns UTC, system uses TIMEZONE from config.h via setenv/tzset
 *   - AUTO: API returns local times, RTC syncs with utc_offset_seconds
 */

#ifndef TIME_COORDINATOR_H
#define TIME_COORDINATOR_H

#include <Arduino.h>
#include <time.h>
#include "api_response.h"

// Number of hourly data points from API
#ifndef OWM_NUM_HOURLY
#define OWM_NUM_HOURLY 48
#endif

#ifndef OWM_NUM_DAILY
#define OWM_NUM_DAILY 8
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Data Structures (optimized for embedded - no heap allocation)
// ═══════════════════════════════════════════════════════════════════════════

// Coalesced daily data for cache locality
struct DailyNorm {
    time_t dt;
    time_t sunrise;
    time_t sunset;
    time_t moonrise;
    time_t moonset;
};

// Normalized weather data (timestamps in local time)
struct NormalizedWeather {
    DailyNorm daily[OWM_NUM_DAILY];
    time_t hourlyDt[OWM_NUM_HOURLY];
    time_t currentDt;
    time_t currentSunrise;
    time_t currentSunset;
    int apiOffsetSeconds;
};

// Display-ready data (all strings pre-formatted)
struct TimeDisplayData {
    char updateTime[6];                    // "HH:MM\0"
    char displayDate[32];                  // "Day, DD Mon YYYY\0"
    char hourlyLabels[OWM_NUM_HOURLY][6];  // "14h\0", "15h\0"...
    int forecastDayOfWeek[OWM_NUM_DAILY];  // 0=Sun, 1=Mon...
    uint64_t sleepDurationSeconds;         // Ready for esp_sleep_enable_timer_wakeup
    // Current conditions - pre-formatted times
    char sunriseTime[12];                  // Sunrise time
    char sunsetTime[12];                   // Sunset time
    char moonriseTime[12];                 // Moonrise time
    char moonsetTime[12];                  // Moonset time
    char rainTime[6];                      // "HH:MM\0" for next rain event
    int hourlyStartIndex;                  // Starting index for hourly graphs
};

enum TimeMode {
    TIME_MODE_MANUAL,
    TIME_MODE_AUTO
};

// ═══════════════════════════════════════════════════════════════════════════
// TimeCoordinator Class
// ═══════════════════════════════════════════════════════════════════════════

class TimeCoordinator {
public:
    // Initialize - detects mode and configures timezone
    void begin();
    
    // Main entry point: processes API data and returns display-ready info
    // Does NOT mutate apiData - creates normalized copy internally
    TimeDisplayData process(const owm_resp_onecall_t& apiData, 
                           unsigned long startTimeMillis);
    
    // Check current mode
    bool isAutoMode() const { return mode_ == TIME_MODE_AUTO; }
    
    // Get API offset (for diagnostics)
    int getApiOffsetSeconds() const { return apiOffsetSeconds_; }
    
private:
    TimeMode mode_;
    int apiOffsetSeconds_;
    
    // RTC-persistent flag: survives deep sleep but NOT power loss
    static RTC_DATA_ATTR bool rtcSynced_;
    
    // Helper methods
    void configureTimezoneFromString_(const char* tzString);
    void configureTimezoneFromOffset_(int offsetSeconds);
    void normalize_(const owm_resp_onecall_t& src, NormalizedWeather& dst);
    void formatDisplayData_(const owm_resp_onecall_t& apiData,
                           const NormalizedWeather& norm,
                           TimeDisplayData& out,
                           unsigned long startTimeMillis);
    void syncRtcIfNeeded_(int offsetSeconds);
    uint64_t calculateSleep_(const tm* localTime);
};

#endif // TIME_COORDINATOR_H
