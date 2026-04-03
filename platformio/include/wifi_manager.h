#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

// Forward declaration for AsyncWebServerRequest to avoid including full header
class AsyncWebServerRequest;

// State Machine States
enum FirmwareState {
    STATE_BOOT,
    STATE_WIFI_CONNECTING,
    STATE_AP_CONFIG_MODE,
    STATE_NORMAL_MODE,
    STATE_SLEEP_PENDING,
    STATE_ERROR
};

extern FirmwareState currentState;

// Configuration Structure
struct DeviceConfig {
    uint32_t wifiConnectTimeout;   // seconds
    uint32_t configTimeout;        // seconds
};

extern DeviceConfig wifiConfig;

// Runtime State
struct RuntimeState {
    bool apMode;
    bool wifiConnected;
    bool portalActive;
    uint32_t wifiStartTime;
    uint32_t portalStartTime;
};

extern RuntimeState runtime;

// Timezone Mode Constants (Runtime values - correspond to config.h compile-time flags)
#define TIMEZONE_MODE_AUTO   0   // Use Open-Meteo API timezone
#define TIMEZONE_MODE_MANUAL 1   // Use user-selected timezone

// RAM Variables for connection and location
// Buffer sizes follow: Character Limit + 1 for null-terminator
extern char ramSSID[33];       // Wi-Fi SSID limit: 32 chars + \0
extern char ramPassword[64];   // WPA2 password limit: 63 chars + \0
extern char ramCity[64];       // Standard limit: 63 chars + \0
extern char ramCountry[64];    // Standard limit: 63 chars + \0
extern char ramLat[21];        // High-precision lat/lon: 20 chars + \0
extern char ramLon[21];        // High-precision lat/lon: 20 chars + \0
extern char ramTimezone[64];   // Standard limit: 63 chars + \0
extern bool ramAutoGeo;
extern uint8_t ramTimezoneMode;  // OPENMETEO_TIMEZONE_MODE_AUTO or OPENMETEO_TIMEZONE_MODE_MANUAL
extern bool rtcInitialized;
extern bool isFirstBoot;
extern uint8_t connectionFailCycles;  // Consecutive WiFi connection failure cycles
extern bool isErrorState;             // Permanent error state flag (survives deep sleep)

// Functions
void wifiManagerSetup();
void wifiManagerLoop();

// Helper to transition state cleanly
void setFirmwareState(FirmwareState newState);
bool locateByIpAddress();

// Configuration save handler with validation
void handleConfigSave(AsyncWebServerRequest *request);

// Validation helper functions
bool validateLatitude(const char* latStr);
bool validateLongitude(const char* lonStr);
bool validateTimezone(const char* tzStr);
void sanitizeString(const char* input, char* output, size_t outLen, bool allowSpace);
void sanitizeSSID(const char* input, char* output, size_t outLen);
void sanitizeCityCountry(const char* input, char* output, size_t outLen);
void safeCopy(const char* src, char* dest, size_t destLen);
bool applyTimezone(const char* tzStr);

#endif
