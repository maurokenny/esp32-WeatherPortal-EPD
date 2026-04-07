/// @file time_coordinator.h
/// @brief Centralized time and timezone management
/// @copyright Copyright (C) 2026 Mauro Freitas
/// @license GNU General Public License v3.0
///
/// @details
/// TimeCoordinator is the Single Source of Truth for all time-related operations
/// in the ESP32 Weather Display. It centralizes timezone logic and ensures
/// consistent time display across all UI components.
///
/// Architecture flow:
/// ```
/// API Response -> Parser -> TimeCoordinator -> UI / PowerManager
/// ```
///
/// Operating modes:
/// - **AUTO**: API returns local times; system uses utc_offset_seconds for RTC sync
/// - **MANUAL**: API returns UTC; system uses TIMEZONE from config.h via setenv/tzset
///
/// @note No other module should interpret timezones or apply offsets directly.
/// All time calculations must go through TimeCoordinator.

#ifndef TIME_COORDINATOR_H
#define TIME_COORDINATOR_H

#include <Arduino.h>
#include <time.h>
#include "api_response.h"

/// @brief Number of hourly data points from API
#ifndef OWM_NUM_HOURLY
#define OWM_NUM_HOURLY 48
#endif

/// @brief Number of daily data points from API
#ifndef OWM_NUM_DAILY
#define OWM_NUM_DAILY 8
#endif

// ═══════════════════════════════════════════════════════════════════════════
// DATA STRUCTURES (optimized for embedded - no heap allocation)
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Coalesced daily data for cache locality
struct DailyNorm {
  time_t dt;        ///< Day timestamp (Unix epoch)
  time_t sunrise;   ///< Sunrise timestamp
  time_t sunset;    ///< Sunset timestamp
  time_t moonrise;  ///< Moonrise timestamp
  time_t moonset;   ///< Moonset timestamp
};

/// @brief Normalized weather data with processed timestamps
/// @details All timestamps stored in local time after timezone conversion
struct NormalizedWeather {
  DailyNorm daily[OWM_NUM_DAILY];      ///< Daily forecast data
  time_t hourlyDt[OWM_NUM_HOURLY];     ///< Hourly timestamps
  time_t currentDt;                    ///< Current observation time
  time_t currentSunrise;               ///< Today's sunrise
  time_t currentSunset;                ///< Today's sunset
  int apiOffsetSeconds;                ///< API timezone offset from UTC
};

/// @brief Display-ready time data (all strings pre-formatted)
/// @details Structure contains all time information needed for UI rendering
struct TimeDisplayData {
  char updateTime[6];                    ///< Update time "HH:MM\0"
  char displayDate[32];                  ///< Formatted date "Day, DD Mon YYYY\0"
  char hourlyLabels[OWM_NUM_HOURLY][6];  ///< Hour labels "14h\0", "15h\0", ...
  int todayDayOfWeek;                    ///< Current day (0=Sun, 1=Mon, ...)
  uint64_t sleepDurationSeconds;         ///< Calculated deep sleep duration
  char sunriseTime[12];                  ///< Formatted sunrise time
  char sunsetTime[12];                   ///< Formatted sunset time
  char moonriseTime[12];                 ///< Formatted moonrise time
  char moonsetTime[12];                  ///< Formatted moonset time
  char rainTime[6];                      ///< Next rain time "HH:MM\0"
  int hourlyStartIndex;                  ///< Starting index for hourly graphs
};

/// @brief Time coordination mode
enum TimeMode {
  TIME_MODE_MANUAL,   ///< User-specified timezone (API returns UTC)
  TIME_MODE_AUTO      ///< API-detected timezone (API returns local time)
};

// ═══════════════════════════════════════════════════════════════════════════
// TIME COORDINATOR CLASS
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Centralized time and timezone management
///
/// This class is responsible for:
/// - Detecting timezone mode from configuration
/// - Configuring system timezone (setenv/tzset)
/// - Normalizing API timestamps to local time
/// - Pre-formatting all display strings
/// - Calculating sleep duration based on wake/sleep schedule
class TimeCoordinator {
public:
  /// @brief Initialize TimeCoordinator
  /// @details Detects mode from ramTimezoneMode and configures timezone
  void begin();

  /// @brief Process API data and generate display-ready time information
  /// @param apiData Raw weather API response structure
  /// @param startTimeMillis Device startup time from millis()
  /// @return TimeDisplayData structure with all pre-formatted time strings
  /// @note Does not modify apiData - creates normalized copy internally
  TimeDisplayData process(const owm_resp_onecall_t& apiData,
                         unsigned long startTimeMillis);

  /// @brief Check if operating in auto timezone mode
  /// @return true if API timezone auto-detection is active
  bool isAutoMode() const { return mode_ == TIME_MODE_AUTO; }

  /// @brief Get API timezone offset
  /// @return Offset from UTC in seconds (positive=east of GMT)
  int getApiOffsetSeconds() const { return apiOffsetSeconds_; }

private:
  TimeMode mode_;              ///< Current timezone mode
  int apiOffsetSeconds_;       ///< Cached API offset for diagnostics

  /// @brief RTC-persistent sync flag (survives deep sleep)
  static RTC_DATA_ATTR bool rtcSynced_;

  // Internal helper methods
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
