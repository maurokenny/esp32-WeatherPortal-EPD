/// @file wifi_manager.h
/// @brief WiFi connection management and configuration portal state machine
/// @copyright Copyright (C) 2026 Mauro Freitas
/// @license GNU General Public License v3.0
///
/// @details
/// Implements a non-blocking state machine for WiFi management:
/// - Automatic connection with NVS-stored credentials
/// - Captive portal configuration mode for explicit provisioning only
/// - Configuration button support (GPIO0) to force AP mode after boot
/// - IP-based geolocation for automatic location detection
/// - Failure tracking with configurable retry limits
///
/// @note WiFi credentials and location settings are persisted in NVS flash.
///       RTC RAM is used only for failure counters across deep sleep.

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "state_decision.h"

// Forward declaration for AsyncWebServerRequest to avoid including full header
class AsyncWebServerRequest;

// ═══════════════════════════════════════════════════════════════════════════
// CONFIGURATION STORE (NVS)
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Persistent configuration storage using ESP32 NVS
/// @details Stores WiFi credentials and location settings in flash.
///          Survives deep sleep, reboots, watchdog resets, and power cycles.
class ConfigStore {
public:
    static constexpr const char* STORE_NAMESPACE = "device";
    static constexpr const char* KEY_SSID = "ssid";
    static constexpr const char* KEY_PASSWORD = "pass";
    static constexpr const char* KEY_LAT = "lat";
    static constexpr const char* KEY_LON = "lon";
    static constexpr const char* KEY_CITY = "city";
    static constexpr const char* KEY_COUNTRY = "country";
    static constexpr const char* KEY_TZ = "tz";
    static constexpr const char* KEY_AUTO_GEO = "autoGeo";
    static constexpr const char* KEY_TZ_MODE = "tzMode";

    ConfigStore();

    /// @brief Open NVS namespace
    /// @param readOnly If false, allows write operations
    /// @return true on success
    bool begin(bool readOnly = false);

    /// @brief Close NVS namespace
    void end();

    /// @brief Load all values from NVS into RAM buffers
    /// @return true if NVS was accessible (even if empty)
    bool loadFromNVS();

    /// @brief Save all RAM buffers to NVS
    /// @return true on success
    bool saveToNVS();

    /// @brief Clear all configuration from NVS
    void clear();

    /// @brief Check if stored WiFi config is valid
    /// @return true if SSID is non-empty (password may be empty for open networks)
    bool hasValidWifiConfig() const;

    /// @brief Check if a complete location set is stored
    /// @return true if lat, lon, city, country, and timezone are non-empty
    bool hasCompleteLocation() const;

    /// @brief Migrate config from legacy RTC RAM to NVS (one-time)
    /// @return true if migration was performed
    bool migrateFromRtcIfNeeded();

    // Setters
    void setWifiConfig(const char* ssid, const char* password);
    void setLocation(const char* lat, const char* lon,
                     const char* city, const char* country,
                     const char* tz);
    void setTimezoneMode(uint8_t mode);
    void setAutoGeo(bool enabled);

    // Getters
    const char* ssid() const { return ssid_; }
    const char* password() const { return password_; }
    const char* lat() const { return lat_; }
    const char* lon() const { return lon_; }
    const char* city() const { return city_; }
    const char* country() const { return country_; }
    const char* timezone() const { return timezone_; }
    uint8_t timezoneMode() const { return timezoneMode_; }
    bool autoGeo() const { return autoGeo_; }

private:
    Preferences prefs_;
    char ssid_[33];
    char password_[64];
    char lat_[21];
    char lon_[21];
    char city_[64];
    char country_[64];
    char timezone_[64];
    bool autoGeo_;
    uint8_t timezoneMode_;
};

/// @brief Global configuration store instance
extern ConfigStore configStore;

// ═══════════════════════════════════════════════════════════════════════════
// CONFIGURATION BUTTON
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Read the configuration button after boot stabilization
/// @return true if the button was held for AP_MODE_HOLD_MS
/// @details GPIO0 must be released during reset to avoid bootloader mode.
///          The button is sampled after a short debounce delay.
bool readConfigButton();

// ═══════════════════════════════════════════════════════════════════════════
// STATE MACHINE
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Firmware operational states
/// @details Alias for State defined in state_decision.h for backward compatibility
using FirmwareState = State;

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
/// @details Only failure counters remain in RTC RAM. Configuration is in NVS.
/// @{

extern uint8_t connectionFailCycles;  ///< Consecutive WiFi failures
extern uint8_t ntpFailCycles;         ///< Consecutive NTP failures
extern uint8_t apiFailCycles;         ///< Consecutive API failures
extern bool isErrorState;             ///< Permanent error state flag

/// @}

// ═══════════════════════════════════════════════════════════════════════════
// LEGACY RTC RAM CONFIGURATION VARIABLES (DEPRECATED)
// ═══════════════════════════════════════════════════════════════════════════
/// @deprecated Kept only for one-time migration to NVS. Will be removed.
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

/// @}

// ═══════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Initialize WiFi manager and load configuration from NVS
/// @details Sets hostname, opens ConfigStore, migrates legacy RTC config
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

/// @brief Bootstrap NVS from compile-time .env defaults
/// @return true if .env had valid WiFi config and was written to NVS
/// @details Only runs when ALLOW_ENV_BOOTSTRAP_TO_NVS is 1
bool bootstrapFromEnv();

/// @brief Perform IP-based geolocation
/// @return true if location successfully determined
/// @details Uses ip-api.com to auto-detect city/coordinates when autoGeo is true
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
