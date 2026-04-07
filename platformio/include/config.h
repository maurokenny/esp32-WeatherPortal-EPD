/// @file config.h
/// @brief Compile-time configuration and hardware feature selection
/// @copyright Copyright (C) 2022-2025 Luke Marzen, 2026 Mauro Freitas
/// @license GNU General Public License v3.0
///
/// @details
/// This header defines all compile-time configuration options for the ESP32
/// Weather E-Paper Display firmware. Options are organized into categories:
/// - Hardware (display panel, driver board, sensor)
/// - Localization and timezone
/// - Units of measurement
/// - Network security (HTTP/HTTPS)
/// - Display options (wind indicators, widget positions)
/// - Power management (battery monitoring)
/// - Debug and testing modes
///
/// @note All selections are validated at compile time using static assertions.
/// Invalid configurations will produce descriptive #error messages.

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <cstdint>
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
// HARDWARE CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════

/// @defgroup display_config Display Panel Selection
/// @brief E-Paper panel hardware variants (mutually exclusive)
/// @{
/// 7.5" Black/White e-Paper v2 (800x480px) - Standard driver
// #define DISP_BW_V2
/// 7.5" Black/White e-Paper v2 (800x480px) - Alternative for FPC-C001 panels
#define DISP_BW_V2_ALT
/// 7.5" 3-Color e-Paper B (800x480px, Red/Black/White)
// #define DISP_3C_B
/// 7.3" 7-Color ACeP e-Paper F (800x480px)
// #define DISP_7C_F
/// 7.5" Black/White e-Paper v1 (640x384px) - Legacy
// #define DISP_BW_V1
/// @}

/// @defgroup driver_config Driver Board Selection
/// @brief E-Paper driver board hardware (mutually exclusive)
/// @{
/// DESPI-C02 driver board (recommended)
// #define DRIVER_DESPI_C02
/// Waveshare driver board rev2.2/2.3 (deprecated)
#define DRIVER_WAVESHARE
/// @}

/// @defgroup sensor_config Environment Sensor Selection
/// @brief Indoor sensor hardware (mutually exclusive)
/// @{
/// BME280: Temperature, humidity, pressure
#define SENSOR_BME280
/// BME680: Temperature, humidity, pressure, VOC
// #define SENSOR_BME680
/// @}

/// @brief Sensor initialization delay in milliseconds
/// @details Increase if sensor returns invalid data on first read
// #define SENSOR_INIT_DELAY_MS 300

/// @defgroup color_config 3-Color Display Accent Color
/// @brief Third color for multi-color e-paper displays
#if defined(DISP_3C_B) || defined(DISP_7C_F)
  // #define ACCENT_COLOR GxEPD_BLACK
  #define ACCENT_COLOR GxEPD_RED
  // #define ACCENT_COLOR GxEPD_GREEN
  // #define ACCENT_COLOR GxEPD_BLUE
  // #define ACCENT_COLOR GxEPD_YELLOW
  // #define ACCENT_COLOR GxEPD_ORANGE
#endif

// ═══════════════════════════════════════════════════════════════════════════
// LOCALIZATION AND TIMEZONE
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Locale for text strings and formatting
/// @details Available: de_DE, en_GB, en_US, et_EE, fi_FI, fr_FR, it_IT, nl_BE, pt_BR, es_ES
#define LOCALE en_US

/// @defgroup timezone_mode Timezone Handling Mode
/// @brief How timezone conversions are handled
/// @{
/// AUTO: API returns local times with DST applied
#define OPENMETEO_TIMEZONE_MODE_AUTO
/// MANUAL: API returns UTC, ESP32 converts using TIMEZONE string
// #define OPENMETEO_TIMEZONE_MODE_MANUAL
/// @}

#if defined(OPENMETEO_TIMEZONE_MODE_AUTO) && defined(OPENMETEO_TIMEZONE_MODE_MANUAL)
  #error "Only one Open-Meteo timezone mode may be enabled"
#endif

// ═══════════════════════════════════════════════════════════════════════════
// UNITS CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════

/// @defgroup temp_units Temperature Units
/// @{
// #define UNITS_TEMP_KELVIN
#define UNITS_TEMP_CELSIUS
// #define UNITS_TEMP_FAHRENHEIT
/// @}

/// @defgroup wind_units Wind Speed Units
/// @{
// #define UNITS_SPEED_METERSPERSECOND
// #define UNITS_SPEED_FEETPERSECOND
#define UNITS_SPEED_KILOMETERSPERHOUR
// #define UNITS_SPEED_MILESPERHOUR
// #define UNITS_SPEED_KNOTS
// #define UNITS_SPEED_BEAUFORT
/// @}

/// @defgroup pressure_units Pressure Units
/// @{
#define UNITS_PRES_HECTOPASCALS
// #define UNITS_PRES_PASCALS
// #define UNITS_PRES_MILLIMETERSOFMERCURY
// #define UNITS_PRES_INCHESOFMERCURY
// #define UNITS_PRES_MILLIBARS
// #define UNITS_PRES_ATMOSPHERES
// #define UNITS_PRES_GRAMSPERSQUARECENTIMETER
// #define UNITS_PRES_POUNDSPERSQUAREINCH
/// @}

/// @defgroup distance_units Distance Units
/// @{
#define UNITS_DIST_KILOMETERS
// #define UNITS_DIST_MILES
/// @}

/// @defgroup hourly_precip_units Hourly Precipitation Display
/// @{
/// Show Probability of Precipitation (PoP)
#define UNITS_HOURLY_PRECIP_POP
/// Show precipitation volume
// #define UNITS_HOURLY_PRECIP_MILLIMETERS
// #define UNITS_HOURLY_PRECIP_CENTIMETERS
// #define UNITS_HOURLY_PRECIP_INCHES
/// @}

/// @brief Enable precipitation graph in hourly outlook
#define ENABLE_HOURLY_PRECIP_GRAPH
// #define DISABLE_HOURLY_PRECIP_GRAPH

/// @defgroup daily_precip_units Daily Precipitation Display
/// @{
#define UNITS_DAILY_PRECIP_POP
// #define UNITS_DAILY_PRECIP_MILLIMETERS
// #define UNITS_DAILY_PRECIP_CENTIMETERS
// #define UNITS_DAILY_PRECIP_INCHES
/// @}

// ═══════════════════════════════════════════════════════════════════════════
// NETWORK SECURITY
// ═══════════════════════════════════════════════════════════════════════════

/// @defgroup http_mode HTTP/HTTPS Mode
/// @brief Network security level (mutually exclusive)
/// @details
/// - USE_HTTP: No encryption, lower power consumption
/// - USE_HTTPS_NO_CERT_VERIF: Encryption without authentication (vulnerable to MITM)
/// - USE_HTTPS_WITH_CERT_VERIF: Full encryption + certificate verification
/// @{
// #define USE_HTTP
#define USE_HTTPS_NO_CERT_VERIF
/// Current certificate valid until: 2026-04-10 23:59:59+00:00
// #define USE_HTTPS_WITH_CERT_VERIF
/// @}

// ═══════════════════════════════════════════════════════════════════════════
// WIND DIRECTION DISPLAY
// ═══════════════════════════════════════════════════════════════════════════

/// @defgroup wind_indicator Wind Direction Indicator Style
/// @{
#define WIND_INDICATOR_ARROW
// #define WIND_INDICATOR_NUMBER
// #define WIND_INDICATOR_CPN_CARDINAL
// #define WIND_INDICATOR_CPN_INTERCARDINAL
// #define WIND_INDICATOR_CPN_SECONDARY_INTERCARDINAL
// #define WIND_INDICATOR_CPN_TERTIARY_INTERCARDINAL
// #define WIND_INDICATOR_NONE
/// @}

/// @defgroup wind_precision Wind Direction Icon Precision
/// @brief Number of discrete wind direction icons (affects flash usage)
/// @details
/// - CARDINAL: 4 icons, 288B, ±45° error
/// - INTERCARDINAL: 8 icons, 576B, ±22.5° error
/// - SECONDARY_INTERCARDINAL: 16 icons, 1152B, ±11.25° error
/// - TERTIARY_INTERCARDINAL: 32 icons, 2304B, ±5.625° error
/// - 360: 360 icons, ~26KB, ±0.5° error
/// @{
// #define WIND_ICONS_CARDINAL
// #define WIND_ICONS_INTERCARDINAL
#define WIND_ICONS_SECONDARY_INTERCARDINAL
// #define WIND_ICONS_TERTIARY_INTERCARDINAL
// #define WIND_ICONS_360
/// @}

// ═══════════════════════════════════════════════════════════════════════════
// WIDGET POSITIONS (0-9 grid: 2 columns x 5 rows)
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Widget position mapping for current conditions display
/// @details Layout: [0,1] / [2,3] / [4,5] / [6,7] / [8,9]
/// Positions 6-9 unavailable on DISP_BW_V1 (640x384)
#define POS_SUNRISE     0
#define POS_SUNSET      1
#define POS_WIND        2
#define POS_HUMIDITY    3
#define POS_UVI         4
#define POS_PRESSURE    5
#define POS_AIR_QULITY  6
#define POS_VISIBILITY  7
#define POS_INTEMP      8
#define POS_INHUMIDITY  9

/// @defgroup moon_style Moon Phase Icon Style
/// @{
/// Primary: dark color indicates moon surface
// #define MOONPHASE_PRIMARY
/// Alternative: dark color indicates shadow
#define MOONPHASE_ALTERNATIVE
/// @}

// ═══════════════════════════════════════════════════════════════════════════
// FONT AND DISPLAY OPTIONS
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Font header file path
/// @details Project designed around FreeSans spacing; other fonts may need layout adjustments
#define FONT_HEADER "fonts/FreeSans.h"

/// @defgroup temp_order Temperature Display Order
/// @{
/// High | Low
#define TEMP_ORDER_HL
// #define TEMP_ORDER_LH
/// @}

/// @brief Daily precipitation display mode
/// @details 0=Disable, 1=Always show, 2=Smart (only when precipitation forecasted)
#define DISPLAY_DAILY_PRECIP 2

/// @brief Show weather icons on hourly temperature graph
/// @details 0=Disable, 1=Enable
#define DISPLAY_HOURLY_ICONS 1

/// @brief Enable weather alert display
#define DISPLAY_ALERTS 1

// ═══════════════════════════════════════════════════════════════════════════
// STATUS BAR OPTIONS
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Show battery percentage in status bar
#define STATUS_BAR_EXTRAS_BAT_PERCENTAGE 1
/// @brief Show battery voltage in status bar
#define STATUS_BAR_EXTRAS_BAT_VOLTAGE    1
/// @brief Show WiFi signal strength indicator in status bar
#define STATUS_BAR_EXTRAS_WIFI_STRENGTH  1
/// @brief Show WiFi RSSI value in status bar
#define STATUS_BAR_EXTRAS_WIFI_RSSI      1

// ═══════════════════════════════════════════════════════════════════════════
// POWER MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Battery monitoring enable flag
/// @details 0=USB/PSU mode (no monitoring), 1=Battery mode (full protection)
#define BATTERY_MONITORING 0

/// @defgroup retry_policy Retry Policy Configuration
/// @brief Maximum consecutive failures before entering permanent error state
/// @details Set to 0 for infinite retries (device never enters STATE_ERROR)
/// @{
#define MAX_WIFI_FAIL_CYCLES  10  ///< WiFi connection failures
#define MAX_NTP_FAIL_CYCLES   10  ///< NTP synchronization failures
#define MAX_API_FAIL_CYCLES   10  ///< API request failures
/// @}

/// @brief Silent mode - hide loading/status screens
/// @details true=Show only critical errors, false=Show all status screens
#define SILENT_STATUS true

// ═══════════════════════════════════════════════════════════════════════════
// DATA SOURCE SELECTION (mutually exclusive)
// ═══════════════════════════════════════════════════════════════════════════

/// @defgroup data_source Data Source Selection
/// @brief Choose ONE data source for weather information
/// @{
#define USE_MOCKUP_DATA 0      ///< Synthetic data for testing (no WiFi)
#define USE_SAVED_API_DATA 0   ///< Load from saved_api_response.h header
/// Default (both 0): Fetch live data from Open-Meteo API
/// @}

#if USE_MOCKUP_DATA && USE_SAVED_API_DATA
  #error "Cannot enable both USE_MOCKUP_DATA and USE_SAVED_API_DATA. Choose one."
#endif

/// @brief Mockup weather scenario selection
/// @details Used when USE_MOCKUP_DATA is enabled
typedef enum {
  MOCKUP_WEATHER_SUNNY,   ///< 0: Clear sky, 25°C
  MOCKUP_WEATHER_RAINY,   ///< 1: Moderate rain, 15°C, 85% humidity
  MOCKUP_WEATHER_SNOWY,   ///< 2: Light snow, 0°C, 80% humidity
  MOCKUP_WEATHER_CLOUDY,  ///< 3: Overcast, 18°C
  MOCKUP_WEATHER_THUNDER  ///< 4: Thunderstorm, 20°C
} mockup_weather_type_t;

/// @brief Selected mockup weather scenario
#define MOCKUP_CURRENT_WEATHER MOCKUP_WEATHER_RAINY

/// @brief Rain widget test state for mockup mode
typedef enum {
  MOCKUP_RAIN_NO_RAIN,      ///< POP < 30%, X over icon
  MOCKUP_RAIN_COMPACT,      ///< POP 30-70%, shows timing
  MOCKUP_RAIN_TAKE,         ///< POP >= 70%, take umbrella
  MOCKUP_RAIN_GRAPH_TEST    ///< POP ramp 0-100% for graph testing
} mockup_rain_widget_state_t;

/// @brief Selected rain widget test state
#define MOCKUP_RAIN_WIDGET_STATE MOCKUP_RAIN_GRAPH_TEST

/// @brief NVS namespace for persistent storage
#define NVS_NAMESPACE "weather_epd"

// ═══════════════════════════════════════════════════════════════════════════
// DEBUG CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Debug verbosity level
/// @details 0=Basic status, 1=Increased verbosity + heap usage, 2=Full API responses
#define DEBUG_LEVEL 2

// ═══════════════════════════════════════════════════════════════════════════
// BUILD SYSTEM INJECTION (do not modify)
// ═══════════════════════════════════════════════════════════════════════════

#ifndef WIFI_SSID_VALUE
  #error "WIFI_SSID_VALUE is missing! Ensure your .env file exists."
#endif

#ifndef WIFI_PASSWORD_VALUE
  #error "WIFI_PASSWORD_VALUE is missing! Check your build system configuration."
#endif

// ═══════════════════════════════════════════════════════════════════════════
// EXTERNAL CONFIGURATION (defined in config.cpp)
// ═══════════════════════════════════════════════════════════════════════════

extern const uint8_t PIN_BAT_ADC;      ///< Battery ADC input pin
extern const uint8_t PIN_EPD_BUSY;     ///< E-paper busy pin
extern const uint8_t PIN_EPD_CS;       ///< SPI chip select
extern const uint8_t PIN_EPD_RST;      ///< Display reset
extern const uint8_t PIN_EPD_DC;       ///< Data/command select
extern const uint8_t PIN_EPD_SCK;      ///< SPI clock
extern const uint8_t PIN_EPD_MISO;     ///< SPI MISO (unused)
extern const uint8_t PIN_EPD_MOSI;     ///< SPI MOSI
extern const uint8_t PIN_EPD_PWR;      ///< Display power control
extern const uint8_t PIN_BME_SDA;      ///< I2C SDA for BME sensor
extern const uint8_t PIN_BME_SCL;      ///< I2C SCL for BME sensor
extern const uint8_t PIN_BME_PWR;      ///< Sensor power control
extern const uint8_t BME_ADDRESS;      ///< I2C address (0x76 or 0x77)

extern const char *WIFI_SSID;          ///< WiFi network name
extern const char *WIFI_PASSWORD;      ///< WiFi password
extern const unsigned long WIFI_TIMEOUT;           ///< WiFi connection timeout (ms)
extern const unsigned HTTP_CLIENT_TCP_TIMEOUT;     ///< HTTP TCP timeout (ms)

extern const String OPENMETEO_ENDPOINT;            ///< API hostname
extern const uint16_t OPENMETEO_PORT;              ///< API port (80 or 443)
extern const String OPENMETEO_AIRQUALITY_ENDPOINT; ///< Air quality API hostname
extern const String LAT;                           ///< Default latitude
extern const String LON;                           ///< Default longitude
extern const String CITY_STRING;                   ///< Default city name
extern const String COUNTRY_STRING;                ///< Default country name
extern const String GEOLOCATION_ENDPOINT;          ///< IP geolocation service
extern const char *TIMEZONE;                       ///< POSIX timezone string
extern const char *TIME_FORMAT;                    ///< Time format string
extern const char *HOUR_FORMAT;                    ///< Hour format string
extern const char *DATE_FORMAT;                    ///< Date format string
extern const char *REFRESH_TIME_FORMAT;            ///< Status bar time format
extern const char *NTP_SERVER_1;                   ///< Primary NTP server
extern const char *NTP_SERVER_2;                   ///< Secondary NTP server
extern const unsigned long NTP_TIMEOUT;            ///< NTP sync timeout (ms)
extern const int SLEEP_DURATION;                   ///< Normal sleep interval (min)
extern const int BED_TIME;                         ///< Sleep mode start hour
extern const int WAKE_TIME;                        ///< Sleep mode end hour
extern const int HOURLY_GRAPH_MAX;                 ///< Max hours in hourly graph

// Battery voltage thresholds (mV)
extern const uint32_t WARN_BATTERY_VOLTAGE;
extern const uint32_t LOW_BATTERY_VOLTAGE;
extern const uint32_t VERY_LOW_BATTERY_VOLTAGE;
extern const uint32_t CRIT_LOW_BATTERY_VOLTAGE;
extern const uint32_t MAX_BATTERY_VOLTAGE;
extern const uint32_t MIN_BATTERY_VOLTAGE;

extern const unsigned long LOW_BATTERY_SLEEP_INTERVAL;      ///< Sleep duration when low battery
extern const unsigned long VERY_LOW_BATTERY_SLEEP_INTERVAL; ///< Sleep duration when very low

// ═══════════════════════════════════════════════════════════════════════════
// COMPILE-TIME VALIDATION (do not modify)
// ═══════════════════════════════════════════════════════════════════════════

#if !(defined(DISP_BW_V2) ^ defined(DISP_BW_V2_ALT) ^ defined(DISP_3C_B) ^ defined(DISP_7C_F) ^ defined(DISP_BW_V1))
  #error "Invalid configuration. Exactly one display panel must be selected."
#endif
#if !(defined(DRIVER_WAVESHARE) ^ defined(DRIVER_DESPI_C02))
  #error "Invalid configuration. Exactly one driver board must be selected."
#endif
#if !(defined(SENSOR_BME280) ^ defined(SENSOR_BME680))
  #error "Invalid configuration. Exactly one sensor must be selected."
#endif
#if !(defined(LOCALE))
  #error "Invalid configuration. Locale not selected."
#endif
#if !(defined(UNITS_TEMP_KELVIN) ^ defined(UNITS_TEMP_CELSIUS) ^ defined(UNITS_TEMP_FAHRENHEIT))
  #error "Invalid configuration. Exactly one temperature unit must be selected."
#endif
#if !(defined(UNITS_SPEED_METERSPERSECOND) ^ defined(UNITS_SPEED_FEETPERSECOND) ^ defined(UNITS_SPEED_KILOMETERSPERHOUR) ^ defined(UNITS_SPEED_MILESPERHOUR) ^ defined(UNITS_SPEED_KNOTS) ^ defined(UNITS_SPEED_BEAUFORT))
  #error "Invalid configuration. Exactly one wind speed unit must be selected."
#endif
#if !(defined(UNITS_PRES_HECTOPASCALS) ^ defined(UNITS_PRES_PASCALS) ^ defined(UNITS_PRES_MILLIMETERSOFMERCURY) ^ defined(UNITS_PRES_INCHESOFMERCURY) ^ defined(UNITS_PRES_MILLIBARS) ^ defined(UNITS_PRES_ATMOSPHERES) ^ defined(UNITS_PRES_GRAMSPERSQUARECENTIMETER) ^ defined(UNITS_PRES_POUNDSPERSQUAREINCH))
  #error "Invalid configuration. Exactly one pressure unit must be selected."
#endif
#if !(defined(UNITS_DIST_KILOMETERS) ^ defined(UNITS_DIST_MILES))
  #error "Invalid configuration. Exactly one distance unit must be selected."
#endif
#if !(defined(USE_HTTP) ^ defined(USE_HTTPS_NO_CERT_VERIF) ^ defined(USE_HTTPS_WITH_CERT_VERIF))
  #error "Invalid configuration. Exactly one HTTP mode must be selected."
#endif
#if !(defined(MOONPHASE_PRIMARY) ^ defined(MOONPHASE_ALTERNATIVE))
  #error "Invalid configuration. Exactly one moon phase style must be selected."
#endif
#if !(defined(TEMP_ORDER_HL) ^ defined(TEMP_ORDER_LH))
  #error "Invalid configuration. Exactly one temperature order must be selected."
#endif
#if !(defined(UNITS_HOURLY_PRECIP_POP) ^ defined(UNITS_HOURLY_PRECIP_MILLIMETERS) ^ defined(UNITS_HOURLY_PRECIP_CENTIMETERS) ^ defined(UNITS_HOURLY_PRECIP_INCHES))
  #error "Invalid configuration. Exactly one hourly precipitation unit must be selected."
#endif
#if !(defined(UNITS_DAILY_PRECIP_POP) ^ defined(UNITS_DAILY_PRECIP_MILLIMETERS) ^ defined(UNITS_DAILY_PRECIP_CENTIMETERS) ^ defined(UNITS_DAILY_PRECIP_INCHES))
  #error "Invalid configuration. Exactly one daily precipitation unit must be selected."
#endif
#if defined(WIND_INDICATOR_ARROW) && !(defined(WIND_ICONS_CARDINAL) ^ defined(WIND_ICONS_INTERCARDINAL) ^ defined(WIND_ICONS_SECONDARY_INTERCARDINAL) ^ defined(WIND_ICONS_TERTIARY_INTERCARDINAL) ^ defined(WIND_ICONS_360))
  #error "Invalid configuration. Exactly one wind direction icon precision must be selected."
#endif

#endif // __CONFIG_H__
