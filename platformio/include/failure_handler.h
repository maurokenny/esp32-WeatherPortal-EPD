/// @file failure_handler.h
/// @brief System failure handling and retry management
/// @copyright Copyright (C) 2026 Mauro Freitas
/// @license GNU General Public License v3.0
///
/// @details
/// Implements a failure handling system with retry counting and escalation:
/// - Tracks consecutive failures per subsystem (WiFi, NTP, API, Battery)
/// - Shows error screens (unless in silent mode)
/// - Enters deep sleep between retry attempts
/// - Enters permanent error state after MAX_*_FAIL_CYCLES exceeded
///
/// Failure counters are stored in RTC RAM and survive deep sleep.

#ifndef __FAILURE_HANDLER_H__
#define __FAILURE_HANDLER_H__

#include <Arduino.h>

/// @brief Failure type categories
/// @details Used to track failures per subsystem
enum FailureType {
  FAILURE_WIFI,     ///< WiFi connection failure
  FAILURE_NTP,      ///< NTP time synchronization failure
  FAILURE_API,      ///< Weather API request failure
  FAILURE_BATTERY,  ///< Battery voltage critical
  FAILURE_AP_TIMEOUT ///< Configuration portal timeout
};

/// @brief WiFi connection failure counter
/// @details RTC RAM variable, persists across deep sleep
extern uint8_t connectionFailCycles;

/// @brief NTP synchronization failure counter
/// @details RTC RAM variable, persists across deep sleep
extern uint8_t ntpFailCycles;

/// @brief API request failure counter
/// @details RTC RAM variable, persists across deep sleep
extern uint8_t apiFailCycles;

/// @brief Handle system failure with retry logic
/// @param type Failure category
/// @param line1 Primary error message (displayed on screen)
/// @param line2 Secondary error message (optional, displayed below)
/// @details
/// Increments failure counter for the specified type. If counter exceeds
/// configured maximum, transitions to STATE_ERROR (permanent sleep).
/// Otherwise, shows error screen (unless SILENT_STATUS) and schedules
/// retry via deep sleep.
///
/// Retry intervals:
/// - First 3 failures: short sleep (configured interval)
/// - 4-6 failures: longer sleep (10 minutes)
/// - 7+ failures: maximum sleep (30 minutes)
///
/// @note This function does not return if entering STATE_ERROR
void handleFailure(FailureType type,
                   const String& line1,
                   const String& line2 = "");

#endif // __FAILURE_HANDLER_H__
