/// @file renderer.h
/// @brief E-paper display rendering functions and display instance
/// @copyright Copyright (C) 2022-2025 Luke Marzen, 2026 Mauro Freitas
/// @license GNU General Public License v3.0
///
/// @details
/// Provides high-level rendering functions for the weather display:
/// - Display initialization and power management
/// - Text rendering with alignment
/// - Weather widget drawing (current, forecast, graphs)
/// - Status bar and error screens
///
/// @note Display instance is conditionally compiled based on DISP_* macros.
/// Different panel types use different GxEPD2 template classes.

#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <vector>
#include <Arduino.h>
#include <time.h>
#include "api_response.h"
#include "config.h"

// ═══════════════════════════════════════════════════════════════════════════
// DISPLAY INSTANCE CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Display width in pixels
/// @note Set by DISP_* macros: 800 for most panels, 640 for DISP_BW_V1
#define DISP_WIDTH 800

/// @brief Display height in pixels
/// @note Set by DISP_* macros: 480 for most panels, 384 for DISP_BW_V1
#define DISP_HEIGHT 480

#ifdef DISP_BW_V2
  #include <GxEPD2_BW.h>
  /// @brief Display instance for 7.5" BW v2 panel
  extern GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display;
#endif

#ifdef DISP_BW_V2_ALT
  #include <GxEPD2_BW.h>
  /// @brief Display instance for 7.5" BW v2 alt panel (FPC-C001)
  extern GxEPD2_BW<GxEPD2_750_GDEY075T7, GxEPD2_750_GDEY075T7::HEIGHT> display;
#endif

#ifdef DISP_3C_B
  #include <GxEPD2_3C.h>
  /// @brief Display instance for 7.5" 3-color panel
  /// @note Uses partial buffer (HEIGHT/2) due to color data size
  extern GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT / 2> display;
#endif

#ifdef DISP_7C_F
  #include <GxEPD2_7C.h>
  /// @brief Display instance for 7.3" 7-color panel
  /// @note Uses partial buffer (HEIGHT/4) due to color data size
  extern GxEPD2_7C<GxEPD2_730c_GDEY073D46, GxEPD2_730c_GDEY073D46::HEIGHT / 4> display;
#endif

#ifdef DISP_BW_V1
  #undef DISP_WIDTH
  #undef DISP_HEIGHT
  #define DISP_WIDTH 640
  #define DISP_HEIGHT 384
  #include <GxEPD2_BW.h>
  /// @brief Display instance for 7.5" BW v1 panel (legacy, 640x384)
  extern GxEPD2_BW<GxEPD2_750, GxEPD2_750::HEIGHT> display;
#endif

// ═══════════════════════════════════════════════════════════════════════════
// TEXT ALIGNMENT
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Text alignment options for drawString()
typedef enum alignment {
  LEFT,     ///< Align text left to anchor point
  RIGHT,    ///< Align text right to anchor point
  CENTER    ///< Center text on anchor point
} alignment_t;

// ═══════════════════════════════════════════════════════════════════════════
// TEXT RENDERING
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Calculate string width in pixels
/// @param text String to measure
/// @return Width in pixels
uint16_t getStringWidth(const String &text);

/// @brief Calculate string height in pixels
/// @param text String to measure
/// @return Height in pixels
uint16_t getStringHeight(const String &text);

/// @brief Draw string with specified alignment
/// @param x Horizontal anchor position
/// @param y Vertical anchor position (baseline)
/// @param text String to draw
/// @param alignment Alignment relative to anchor
/// @param color Text color (GxEPD_BLACK, GxEPD_WHITE, or ACCENT_COLOR)
void drawString(int16_t x, int16_t y, const String &text, alignment_t alignment,
                uint16_t color = GxEPD_BLACK);

/// @brief Draw multi-line string with word wrapping
/// @param x Horizontal anchor position
/// @param y Vertical anchor position
/// @param text String to draw
/// @param alignment Alignment
/// @param max_width Maximum line width in pixels
/// @param max_lines Maximum number of lines
/// @param line_spacing Vertical space between lines (pixels)
/// @param color Text color
/// @details Breaks at spaces and dashes. Adds ellipsis on last line if truncated.
void drawMultiLnString(int16_t x, int16_t y, const String &text,
                       alignment_t alignment, uint16_t max_width,
                       uint16_t max_lines, int16_t line_spacing,
                       uint16_t color = GxEPD_BLACK);

// ═══════════════════════════════════════════════════════════════════════════
// DISPLAY CONTROL
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Initialize e-paper display
/// @details Powers on display, initializes SPI, sets rotation and fonts
/// @warning Must call powerOffDisplay() before deep sleep
void initDisplay();

/// @brief Power off e-paper display
/// @details Hibernates display controller and cuts power
void powerOffDisplay();

// ═══════════════════════════════════════════════════════════════════════════
// WEATHER WIDGETS
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Draw umbrella widget with rain recommendation
/// @param x Top-left X position
/// @param y Top-left Y position
/// @param hourly Hourly forecast array
/// @param hours Number of hours in array
/// @param current_dt Current timestamp (Unix epoch)
/// @param rainTimeStr Pre-formatted rain time string
void drawUmbrellaWidget(int x, int y, const owm_hourly_t *hourly, int hours,
                        int64_t current_dt, const char* rainTimeStr);

/// @brief Draw current conditions panel
/// @param current Current weather data
/// @param today Today's daily forecast
/// @param owm_air_pollution Air quality data
/// @param inTemp Indoor temperature (NAN if unavailable)
/// @param inHumidity Indoor humidity % (NAN if unavailable)
/// @param hourly Hourly forecast array (for umbrella widget)
/// @param sunriseTimeStr Formatted sunrise time
/// @param sunsetTimeStr Formatted sunset time
/// @param moonriseTimeStr Formatted moonrise time
/// @param moonsetTimeStr Formatted moonset time
/// @param rainTimeStr Formatted next rain time
void drawCurrentConditions(const owm_current_t &current,
                           const owm_daily_t &today,
                           const owm_resp_air_pollution_t &owm_air_pollution,
                           float inTemp, float inHumidity,
                           const owm_hourly_t *hourly,
                           const char* sunriseTimeStr,
                           const char* sunsetTimeStr,
                           const char* moonriseTimeStr,
                           const char* moonsetTimeStr,
                           const char* rainTimeStr);

/// @brief Draw daily forecast row
/// @param daily Array of daily forecasts
/// @param todayDayOfWeek Current day of week (0=Sunday)
/// @details Shows 5 days of forecast with icons and temperatures
void drawForecast(const owm_daily_t *daily, int todayDayOfWeek);

/// @brief Draw weather alerts
/// @param alerts Vector of active alerts
/// @param city Location city name
/// @param date Current date string
void drawAlerts(std::vector<owm_alerts_t> &alerts,
                const String &city, const String &date);

/// @brief Draw location and date header
/// @param city City name
/// @param date Formatted date string
void drawLocationDate(const String &city, const String &date);

/// @brief Draw hourly outlook temperature/precipitation graph
/// @param hourly Hourly forecast array
/// @param daily Daily forecast array
/// @param hourlyLabels Array of hour labels (e.g., "14h")
/// @param startIndex Starting hour index for graph
void drawOutlookGraph(const owm_hourly_t *hourly, const owm_daily_t *daily,
                      const char hourlyLabels[][6], int startIndex);

/// @brief Draw status bar
/// @param statusStr Status message
/// @param refreshTimeStr Last update time
/// @param rssi WiFi signal strength (dBm)
/// @param batVoltage Battery voltage (mV)
void drawStatusBar(const String &statusStr, const String &refreshTimeStr,
                   int rssi, uint32_t batVoltage);

// ═══════════════════════════════════════════════════════════════════════════
// INDIVIDUAL CURRENT CONDITION WIDGETS (by position)
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Draw sunrise time widget
/// @param sunriseTimeStr Formatted sunrise time
/// @note Only compiled if POS_SUNRISE defined
#ifdef POS_SUNRISE
void drawCurrentSunrise(const char* sunriseTimeStr);
#endif

/// @brief Draw sunset time widget
/// @param sunsetTimeStr Formatted sunset time
/// @note Only compiled if POS_SUNSET defined
#ifdef POS_SUNSET
void drawCurrentSunset(const char* sunsetTimeStr);
#endif

/// @brief Draw indoor temperature widget
/// @param inTemp Indoor temperature (Celsius)
/// @note Only compiled if POS_INTEMP defined
#ifdef POS_INTEMP
void drawCurrentInTemp(float inTemp);
#endif

/// @brief Draw indoor humidity widget
/// @param inHumidity Indoor humidity percentage
/// @note Only compiled if POS_INHUMIDITY defined
#ifdef POS_INHUMIDITY
void drawCurrentInHumidity(float inHumidity);
#endif

/// @brief Draw moonrise time widget
/// @param moonriseTimeStr Formatted moonrise time
/// @note Only compiled if POS_MOONRISE defined
#ifdef POS_MOONRISE
void drawCurrentMoonrise(const char* moonriseTimeStr);
#endif

/// @brief Draw moonset time widget
/// @param moonsetTimeStr Formatted moonset time
/// @note Only compiled if POS_MOONSET defined
#ifdef POS_MOONSET
void drawCurrentMoonset(const char* moonsetTimeStr);
#endif

/// @brief Draw wind speed/direction widget
/// @param current Current weather data
/// @note Only compiled if POS_WIND defined
#ifdef POS_WIND
void drawCurrentWind(const owm_current_t &current);
#endif

/// @brief Draw humidity widget
/// @param current Current weather data
/// @note Only compiled if POS_HUMIDITY defined
#ifdef POS_HUMIDITY
void drawCurrentHumidity(const owm_current_t &current);
#endif

/// @brief Draw UV index widget
/// @param current Current weather data
/// @note Only compiled if POS_UVI defined
#ifdef POS_UVI
void drawCurrentUVI(const owm_current_t &current);
#endif

/// @brief Draw pressure widget
/// @param current Current weather data
/// @note Only compiled if POS_PRESSURE defined
#ifdef POS_PRESSURE
void drawCurrentPressure(const owm_current_t &current);
#endif

/// @brief Draw visibility widget
/// @param current Current weather data
/// @note Only compiled if POS_VISIBILITY defined
#ifdef POS_VISIBILITY
void drawCurrentVisibility(const owm_current_t &current);
#endif

/// @brief Draw air quality widget
/// @param owm_air_pollution Air quality data
/// @note Only compiled if POS_AIR_QULITY defined
#ifdef POS_AIR_QULITY
void drawCurrentAirQuality(const owm_resp_air_pollution_t &owm_air_pollution);
#endif

/// @brief Draw moon phase widget
/// @param today Today's daily forecast
/// @note Only compiled if POS_MOONPHASE defined
#ifdef POS_MOONPHASE
void drawCurrentMoonphase(const owm_daily_t &today);
#endif

/// @brief Draw dew point widget
/// @param current Current weather data
/// @note Only compiled if POS_DEWPOINT defined
#ifdef POS_DEWPOINT
void drawCurrentDewpoint(const owm_current_t &current);
#endif

// ═══════════════════════════════════════════════════════════════════════════
// UTILITY SCREENS
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Draw error screen with icon
/// @param bitmap_196x196 Error icon (196x196 pixels)
/// @param errMsgLn1 First line of error message
/// @param errMsgLn2 Second line (optional)
void drawError(const uint8_t *bitmap_196x196,
               const String &errMsgLn1, const String &errMsgLn2 = "");

/// @brief Draw loading screen with icon
/// @param bitmap_196x196 Loading icon (196x196 pixels)
/// @param msgLn1 First line of message
/// @param msgLn2 Second line (optional)
void drawLoading(const uint8_t *bitmap_196x196,
                 const String &msgLn1, const String &msgLn2 = "");

#endif // __RENDERER_H__
