/// @file display_utils.h
/// @brief Display helper utilities and bitmap selection
/// @copyright Copyright (C) 2022-2025 Luke Marzen, 2026 Mauro Freitas
/// @license GNU General Public License v3.0
///
/// @details
/// Provides utility functions for the e-paper display:
/// - Battery voltage reading and percentage calculation
/// - Bitmap selection based on weather conditions
/// - Alert categorization and filtering
/// - WiFi signal strength indicators
/// - String formatting utilities

#ifndef __DISPLAY_UTILS_H__
#define __DISPLAY_UTILS_H__

#include <vector>
#include <time.h>
#include "api_response.h"

/// @brief Weather alert categories for icon selection
/// @details Used to select appropriate alert icon based on event text
enum alert_category {
  NOT_FOUND = -1,          ///< No matching category
  SMOG,                    ///< Air quality / smog
  SMOKE,                   ///< Smoke conditions
  FOG,                     ///< Fog advisories
  METEOR,                  ///< Meteorological events
  NUCLEAR,                 ///< Nuclear/radiation hazards
  BIOHAZARD,               ///< Biological hazards
  EARTHQUAKE,              ///< Seismic events
  FIRE,                    ///< Fire warnings
  HEAT,                    ///< Excessive heat
  WINTER,                  ///< Winter weather
  TSUNAMI,                 ///< Tsunami warnings
  LIGHTNING,               ///< Lightning/thunderstorms
  SANDSTORM,               ///< Sand/dust storms
  FLOOD,                   ///< Flood warnings
  VOLCANO,                 ///< Volcanic activity
  AIR_QUALITY,             ///< Air quality alerts
  TORNADO,                 ///< Tornado warnings
  SMALL_CRAFT_ADVISORY,    ///< Marine small craft
  GALE_WARNING,            ///< Marine gale
  STORM_WARNING,           ///< Marine storm
  HURRICANE_WARNING,       ///< Hurricane warnings
  HURRICANE,               ///< Hurricane tracking
  DUST,                    ///< Dust advisories
  STRONG_WIND              ///< High wind warnings
};

/// @defgroup battery_funcs Battery Management
/// @{

/// @brief Read battery voltage from ADC
/// @return Battery voltage in millivolts (mV)
/// @details Uses ESP32 ADC with calibrated voltage divider
/// @note Voltage divider assumed 1M+1M (multiply ADC reading by 2)
uint32_t readBatteryVoltage();

/// @brief Calculate battery percentage using sigmoidal approximation
/// @param v Current voltage (mV)
/// @param minv Minimum voltage (empty)
/// @param maxv Maximum voltage (full)
/// @return Battery percentage (0-100)
/// @details Uses curve-fitted sigmoid for LiPo discharge curve
uint32_t calcBatPercent(uint32_t v, uint32_t minv, uint32_t maxv);

/// @brief Get battery level bitmap
/// @param batPercent Battery percentage (0-100)
/// @return Pointer to 24x24 bitmap
const uint8_t *getBatBitmap24(uint32_t batPercent);

/// @}

/// @defgroup string_funcs String Formatting
/// @{

/// @brief Format date string for display
/// @param s Output string
/// @param timeInfo Time structure
/// @param tzOffsetSeconds Timezone offset for adjustment (0=use as-is)
void getDateStr(String &s, tm *timeInfo, int tzOffsetSeconds = 0);

/// @brief Format refresh time string
/// @param s Output string
/// @param timeSuccess Whether time sync was successful
/// @param timeInfo Time structure
/// @param tzOffsetSeconds Timezone offset in seconds
void getRefreshTimeStr(String &s, bool timeSuccess, tm *timeInfo, int tzOffsetSeconds = 0);

/// @brief Convert string to Title Case
/// @param text String to modify in-place
/// @details Capitalizes first letter of each word
void toTitleCase(String &text);

/// @brief Truncate alert text at punctuation
/// @param text Alert event string to modify
/// @details Removes everything after first comma, period, or open paren
void truncateExtraAlertInfo(String &text);

/// @}

/// @defgroup alert_funcs Alert Processing
/// @{

/// @brief Filter and prioritize weather alerts
/// @param resp Vector of alerts from API
/// @param ignore_list Array to mark ignored alerts (0=show, 1=ignore)
/// @details Implements deduplication and urgency ranking
void filterAlerts(std::vector<owm_alerts_t> &resp, int *ignore_list);

/// @brief Get alert category from event text
/// @param alert Alert structure
/// @return Alert category enum value
enum alert_category getAlertCategory(const owm_alerts_t &alert);

/// @}

/// @defgroup weather_bitmap_funcs Weather Icon Selection
/// @{

/// @brief Get hourly forecast icon (32x32)
/// @param hourly Hourly forecast data point
/// @param today Today's daily forecast (for moon phase)
/// @return Pointer to 32x32 bitmap
const uint8_t *getHourlyForecastBitmap32(const owm_hourly_t &hourly,
                                         const owm_daily_t  &today);

/// @brief Get daily forecast icon (64x64)
/// @param daily Daily forecast data
/// @return Pointer to 64x64 bitmap
const uint8_t *getDailyForecastBitmap64(const owm_daily_t &daily);

/// @brief Get current conditions icon (196x196)
/// @param current Current conditions
/// @param today Today's daily forecast
/// @return Pointer to 196x196 bitmap
const uint8_t *getCurrentConditionsBitmap196(const owm_current_t &current,
                                             const owm_daily_t   &today);

/// @brief Get current conditions icon (96x96)
/// @param current Current conditions
/// @param today Today's daily forecast
/// @return Pointer to 96x96 bitmap
const uint8_t *getCurrentConditionsBitmap96(const owm_current_t &current,
                                            const owm_daily_t   &today);

/// @brief Get alert icon (32x32)
/// @param alert Alert data
/// @return Pointer to 32x32 bitmap
const uint8_t *getAlertBitmap32(const owm_alerts_t &alert);

/// @brief Get alert icon (48x48)
/// @param alert Alert data
/// @return Pointer to 48x48 bitmap
const uint8_t *getAlertBitmap48(const owm_alerts_t &alert);

/// @brief Get moon phase icon (48x48)
/// @param daily Daily forecast containing moon phase
/// @return Pointer to 48x48 bitmap
const uint8_t *getMoonPhaseBitmap48(const owm_daily_t &daily);

/// @brief Get moon phase description string
/// @param daily Daily forecast
/// @return Pointer to localized phase name
const char *getMoonPhaseStr(const owm_daily_t &daily);

/// @}

/// @defgroup wifi_bitmap_funcs WiFi Status
/// @{

/// @brief Get WiFi signal description
/// @param rssi Signal strength in dBm
/// @return Localized description string
const char *getWiFidesc(int rssi);

/// @brief Get WiFi status bitmap (16x16)
/// @param rssi Signal strength in dBm
/// @return Pointer to 16x16 bitmap
const uint8_t *getWiFiBitmap16(int rssi);

/// @}

/// @defgroup wind_funcs Wind Display
/// @{

/// @brief Get wind direction icon (24x24)
/// @param windDeg Wind direction in degrees
/// @return Pointer to 24x24 bitmap
const uint8_t *getWindBitmap24(int windDeg);

/// @brief Get compass point notation string
/// @param windDeg Wind direction in degrees
/// @return Cardinal/intercardinal direction (N, NE, NNE, etc.)
const char *getCompassPointNotation(int windDeg);

/// @}

/// @defgroup utility_funcs Air Quality & Utilities
/// @{

/// @brief Get UV index description
/// @param uvi UV index value
/// @return Localized description (Low, Moderate, High, etc.)
const char *getUVIdesc(unsigned int uvi);

/// @brief Calculate average pollutant concentration
/// @param pollutant Array of hourly concentrations
/// @param hours Number of hours to average
/// @return Average concentration
float getAvgConc(const float pollutant[], int hours);

/// @brief Calculate Air Quality Index
/// @param p Air pollution data structure
/// @return AQI value (scale depends on locale)
int getAQI(const owm_resp_air_pollution_t &p);

/// @brief Get AQI description
/// @param aqi AQI value
/// @return Localized quality description
const char *getAQIdesc(int aqi);

/// @brief Get HTTP response phrase
/// @param code HTTP status code
/// @return Human-readable description
const char *getHttpResponsePhrase(int code);

/// @brief Get WiFi status phrase
/// @param status WiFi connection status
/// @return Human-readable description
const char *getWifiStatusPhrase(wl_status_t status);

/// @brief Disable built-in LED
/// @details Turns off LED to save power
void disableBuiltinLED();

/// @}

/// @defgroup screen_funcs Screen Drawing Utilities
/// @{

/// @brief Update status message on display
/// @param msg Status message to display
void updateEinkStatus(const char* msg);

/// @brief Draw loading screen
/// @param bitmap_196x196 Icon bitmap to display
/// @param msg Primary message
/// @param submsg Secondary message (optional)
void drawLoading(const uint8_t *bitmap_196x196, const char *msg, const char *submsg = nullptr);

/// @brief Draw AP mode configuration screen
/// @param ssid AP network name
/// @param timeoutMinutes Configuration timeout in minutes
void drawAPModeScreen(const char* ssid, uint32_t timeoutMinutes);

/// @brief Draw timeout screen
void drawTimeoutScreen();

/// @brief Draw error screen
/// @param title Error title
/// @param message Error description
/// @param action Recommended action
void drawErrorScreen(const char* title, const char* message, const char* action);

/// @}

#endif // __DISPLAY_UTILS_H__
