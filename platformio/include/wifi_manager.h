/// @file wifi_manager.h
/// @brief WiFi connection management and configuration portal state machine
/// @copyright Copyright (C) 2026 Mauro Freitas
/// @license GNU General Public License v3.0
///
/// @details
/// Implements a non-blocking state machine for WiFi management:
/// - Automatic connection with stored credentials
/// - Captive portal configuration mode on first boot or connection failure
/// - RTC RAM persistence of settings across deep sleep
/// - IP-based geolocation for automatic location detection
/// - Failure tracking with exponential backoff
///
/// @note All RTC RAM variables survive deep sleep but NOT power loss.

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

// Forward declaration for AsyncWebServerRequest to avoid including full header
class AsyncWebServerRequest;

// ═══════════════════════════════════════════════════════════════════════════
// STATE MACHINE
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Firmware operational states
/// @details State machine controls WiFi connection flow and error handling
enum FirmwareState {
  STATE_BOOT,              ///< Initial state: load credentials, begin connection
  STATE_WIFI_CONNECTING,   ///< Attempting to connect to configured WiFi
  STATE_AP_CONFIG_MODE,    ///< Access point mode for configuration
  STATE_NORMAL_MODE,       ///< Connected, ready for weather updates
  STATE_SLEEP_PENDING,     ///< Weather fetched, preparing for deep sleep
  STATE_ERROR              ///< Permanent error state (manual reset required)
};

/// @brief Global firmware state
/// @details State transitions logged to serial for debugging
extern FirmwareState currentState;

/// @brief Configuration parameters for WiFi manager
struct DeviceConfig {
  uint32_t wifiConnectTimeout;   ///< WiFi connection timeout (seconds)
  uint32_t configTimeout;        ///< AP mode timeout (seconds)
};

/// @brief Global device configuration instance
extern DeviceConfig wifiConfig;

/// @brief Runtime state tracking
struct RuntimeState {
  bool apMode;              ///< Currently in AP mode
  bool wifiConnected;       ///< WiFi connection status
  bool portalActive;        ///< Configuration portal is active
  uint32_t wifiStartTime;   ///< WiFi connection attempt start time (ms)
  uint32_t portalStartTime; ///< AP mode start time (ms)
};

/// @brief Global runtime state instance
extern RuntimeState runtime;

// ═══════════════════════════════════════════════════════════════════════════
// TIMEZONE MODE CONSTANTS
// ═══════════════════════════════════════════════════════════════════════════

#define TIMEZONE_MODE_AUTO   0   ///< Use Open-Meteo API timezone detection
#define TIMEZONE_MODE_MANUAL 1   ///< Use user-selected timezone from config

// ═══════════════════════════════════════════════════════════════════════════
// RTC RAM PERSISTENT VARIABLES
// ═══════════════════════════════════════════════════════════════════════════
/// @defgroup rtc_ram RTC RAM Persistent Storage
/// @brief Variables preserved across deep sleep (lost on power cycle)
/// @{

extern char ramSSID[33];       ///< WiFi SSID (32 chars + null terminator)
extern char ramPassword[64];   ///< WiFi password (63 chars + null)
extern char ramCity[64];       ///< City name for display
extern char ramCountry[64];    ///< Country name for display
extern char ramLat[21];        ///< Latitude (20 chars + null, high precision)
extern char ramLon[21];        ///< Longitude (20 chars + null, high precision)
extern char ramTimezone[64];   ///< POSIX timezone string
extern bool ramAutoGeo;        ///< Auto-geolocation enabled flag
extern uint8_t ramTimezoneMode; ///< TIMEZONE_MODE_AUTO or TIMEZONE_MODE_MANUAL
extern bool rtcInitialized;    ///< RTC RAM has been initialized
extern bool isFirstBoot;       ///< First boot flag (shows setup screens)
extern uint8_t connectionFailCycles;  ///< Consecutive WiFi failures
extern bool isErrorState;      ///< Permanent error state flag

/// @}

// ═══════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Initialize WiFi manager
/// @details Sets hostname, prepares for state machine operation
/// @note Call once in setup()
void wifiManagerSetup();

/// @brief Execute state machine iteration
/// @details Non-blocking; call repeatedly in loop()
/// Handles state transitions, timeouts, and error recovery
void wifiManagerLoop();

/// @brief Transition to new firmware state
/// @param newState Target state
/// @details Logs transition to serial for debugging
void setFirmwareState(FirmwareState newState);

/// @brief Perform IP-based geolocation
/// @return true if location successfully determined
/// @details Uses ip-api.com to auto-detect city/coordinates when ramAutoGeo is true
/// @warning Requires active WiFi connection
bool locateByIpAddress();

/// @brief Handle configuration save from web portal
/// @param request AsyncWebServer request containing form data
/// @details Validates all inputs, sanitizes strings, applies settings
void handleConfigSave(AsyncWebServerRequest *request);

// ═══════════════════════════════════════════════════════════════════════════
// VALIDATION HELPERS
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Validate latitude string
/// @param latStr Latitude string to validate
/// @return true if valid decimal degrees (-90 to 90)
bool validateLatitude(const char* latStr);

/// @brief Validate longitude string
/// @param lonStr Longitude string to validate
/// @return true if valid decimal degrees (-180 to 180)
bool validateLongitude(const char* lonStr);

/// @brief Validate timezone string
/// @param tzStr POSIX timezone string to validate
/// @return true if parseable by tzset()
bool validateTimezone(const char* tzStr);

/// @brief Sanitize input string
/// @param input Source string
/// @param output Destination buffer
/// @param outLen Destination buffer size
/// @param allowSpace Allow space characters in output
/// @details Removes control characters and HTML special chars
void sanitizeString(const char* input, char* output, size_t outLen, bool allowSpace);

/// @brief Sanitize SSID string
/// @param input Source SSID
/// @param output Destination buffer
/// @param outLen Destination buffer size
void sanitizeSSID(const char* input, char* output, size_t outLen);

/// @brief Sanitize city/country name
/// @param input Source string
/// @param output Destination buffer
/// @param outLen Destination buffer size
void sanitizeCityCountry(const char* input, char* output, size_t outLen);

/// @brief Safe string copy with null termination
/// @param src Source string
/// @param dest Destination buffer
/// @param destLen Destination buffer size
void safeCopy(const char* src, char* dest, size_t destLen);

/// @brief Apply timezone configuration
/// @param tzStr POSIX timezone string
/// @return true if successfully applied
/// @details Calls setenv("TZ") and tzset()
bool applyTimezone(const char* tzStr);

#endif // WIFI_MANAGER_H
