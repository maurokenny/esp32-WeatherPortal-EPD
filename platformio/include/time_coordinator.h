/* TimeCoordinator - Single Source of Truth for Time
 * 
 * Centralizes all timezone logic and time conversions.
 * No other module should interpret timezones or apply offsets.
 * 
 * Architecture:
 *   API Response → Parser → TimeCoordinator → UI / PowerManager
 * 
 * Modes:
 *   - MANUAL: API returns UTC, system uses TIMEZONE from config.h
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
};

// Normalized weather data (timestamps in local time)
struct NormalizedWeather {
    DailyNorm daily[OWM_NUM_DAILY];
    time_t hourlyDt[OWM_NUM_HOURLY];
    time_t currentDt;
    int apiOffsetSeconds;
};

// Display-ready data (all strings pre-formatted)
struct TimeDisplayData {
    char updateTime[6];                    // "HH:MM\0"
    char displayDate[32];                  // "Dia, DD Mmm AAAA\0"
    char hourlyLabels[OWM_NUM_HOURLY][6];  // "14h\0", "15h\0"...
    int forecastDayOfWeek[OWM_NUM_DAILY];  // 0=Dom, 1=Seg...
    uint64_t sleepDurationSeconds;         // Ready for esp_sleep_enable_timer_wakeup
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
    // Initialize - detects mode and prepares timezone offset
    void begin();
    
    // Main entry point: processes API data and returns display-ready info
    // Does NOT mutate apiData - creates normalized copy internally
    TimeDisplayData process(const owm_resp_onecall_t& apiData, 
                           unsigned long startTimeMillis);
    
    // Check current mode
    bool isAutoMode() const { return mode_ == TIME_MODE_AUTO; }
    
    // Get offset used (for diagnostics)
    int getOffsetSeconds() const { 
        return (mode_ == TIME_MODE_AUTO) ? 0 : manualOffsetSeconds_; 
    }
    
private:
    TimeMode mode_;
    int manualOffsetSeconds_;  // Parsed from TIMEZONE string (MANUAL mode)
    
    // RTC-persistent flag: survives deep sleep but NOT power loss
    static RTC_DATA_ATTR bool rtcSynced_;
    
    // Helper methods
    void normalize_(const owm_resp_onecall_t& src, NormalizedWeather& dst);
    void formatDisplayData_(const NormalizedWeather& norm, 
                           TimeDisplayData& out,
                           time_t nowLocal,
                           unsigned long startTimeMillis);
    void syncRtcIfNeeded_(int offsetSeconds);
    
    // Pure arithmetic UTC to local conversion (deterministic, no global TZ state)
    time_t utcToLocal_(time_t utc) const {
        return utc + manualOffsetSeconds_;
    }
    
    // Parse TIMEZONE string from config.h (e.g., "CET-1CEST,M3.5.0,M10.5.0/3")
    int parseTimezoneString_(const char* tzString);
    
    // Calculate sleep duration based on BED_TIME/WAKE_TIME
    uint64_t calculateSleep_(time_t nowLocal);
};

#endif // TIME_COORDINATOR_H
