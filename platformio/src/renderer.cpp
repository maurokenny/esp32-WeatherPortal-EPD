/* Renderer for esp32-weather-epd.
 * Copyright (C) 2022-2025  Luke Marzen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "_locale.h"
#include "_strftime.h"
#include "renderer.h"
#include "api_response.h"
#include "config.h"
#include "conversions.h"
#include "display_utils.h"
#include "umbrella_parser.h"

/* Data validation utilities for API values - returns true if valid */
namespace {
  inline bool isValidWindSpeed(float windSpeed) {
    return !std::isnan(windSpeed) && windSpeed >= 0.0f;
  }
  inline bool isValidHumidity(int humidity) {
    return humidity >= 0 && humidity <= 100;
  }
  inline bool isValidPressure(float pressure) {
    return !std::isnan(pressure) && pressure > 0.0f;
  }
  inline bool isValidVisibility(int visibility) {
    return visibility > 0;
  }
  inline bool isValidUVI(float uvi) {
    return !std::isnan(uvi) && uvi >= 0.0f;
  }
}

// fonts
#include FONT_HEADER

// icon header files
#include "icons/icons_16x16.h"
#include "icons/icons_24x24.h"
#include "icons/icons_32x32.h"
#include "icons/icons_48x48.h"
#include "icons/icons_64x64.h"
#include "icons/icons_96x96.h"
#include "icons/icons_128x128.h"
#include "icons/icons_160x160.h"
#include "icons/icons_196x196.h"

#ifdef DISP_BW_V2
  GxEPD2_BW<GxEPD2_750_T7,
            GxEPD2_750_T7::HEIGHT> display(
    GxEPD2_750_T7(PIN_EPD_CS,
                  PIN_EPD_DC,
                  PIN_EPD_RST,
                  PIN_EPD_BUSY));
#endif
#ifdef DISP_BW_V2_ALT
  GxEPD2_BW<GxEPD2_750_GDEY075T7,
            GxEPD2_750_GDEY075T7::HEIGHT> display(
    GxEPD2_750_GDEY075T7(PIN_EPD_CS,
                  PIN_EPD_DC,
                  PIN_EPD_RST,
                  PIN_EPD_BUSY));
#endif
#ifdef DISP_3C_B
  GxEPD2_3C<GxEPD2_750c_Z08,
            GxEPD2_750c_Z08::HEIGHT / 2> display(
    GxEPD2_750c_Z08(PIN_EPD_CS,
                    PIN_EPD_DC,
                    PIN_EPD_RST,
                    PIN_EPD_BUSY));
#endif
#ifdef DISP_7C_F
  GxEPD2_7C<GxEPD2_730c_GDEY073D46,
            GxEPD2_730c_GDEY073D46::HEIGHT / 4> display(
    GxEPD2_730c_GDEY073D46(PIN_EPD_CS,
                           PIN_EPD_DC,
                           PIN_EPD_RST,
                           PIN_EPD_BUSY));
#endif
#ifdef DISP_BW_V1
  GxEPD2_BW<GxEPD2_750,
            GxEPD2_750::HEIGHT> display(
    GxEPD2_750(PIN_EPD_CS,
               PIN_EPD_DC,
               PIN_EPD_RST,
               PIN_EPD_BUSY));
#endif

#ifndef ACCENT_COLOR
  #define ACCENT_COLOR GxEPD_BLACK
#endif

/* Returns the string width in pixels
 */
uint16_t getStringWidth(const String &text)
{
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  return w;
}

/* Returns the string height in pixels
 */
uint16_t getStringHeight(const String &text)
{
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  return h;
}

/* Draws a string with alignment
 */
void drawString(int16_t x, int16_t y, const String &text, alignment_t alignment,
                uint16_t color)
{
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextColor(color);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (alignment == RIGHT)
  {
    x = x - w;
  }
  if (alignment == CENTER)
  {
    x = x - w / 2;
  }
  display.setCursor(x, y);
  display.print(text);
  return;
} // end drawString

/* Draws a string that will flow into the next line when max_width is reached.
 * If a string exceeds max_lines an ellipsis (...) will terminate the last word.
 * Lines will break at spaces(' ') and dashes('-').
 *
 * Note: max_width should be big enough to accommodate the largest word that
 *       will be displayed. If an unbroken string of characters longer than
 *       max_width exist in text, then the string will be printed beyond
 *       max_width.
 */
void drawMultiLnString(int16_t x, int16_t y, const String &text,
                       alignment_t alignment, uint16_t max_width,
                       uint16_t max_lines, int16_t line_spacing,
                       uint16_t color)
{
  uint16_t current_line = 0;
  String textRemaining = text;
  // print until we reach max_lines or no more text remains
  while (current_line < max_lines && !textRemaining.isEmpty())
  {
    int16_t  x1, y1;
    uint16_t w, h;

    display.getTextBounds(textRemaining, 0, 0, &x1, &y1, &w, &h);

    int endIndex = textRemaining.length();
    // check if remaining text is to wide, if it is then print what we can
    String subStr = textRemaining;
    int splitAt = 0;
    int keepLastChar = 0;
    while (w > max_width && splitAt != -1)
    {
      if (keepLastChar)
      {
        // if we kept the last character during the last iteration of this while
        // loop, remove it now so we don't get stuck in an infinite loop.
        subStr.remove(subStr.length() - 1);
      }

      // find the last place in the string that we can break it.
      if (current_line < max_lines - 1)
      {
        splitAt = std::max(subStr.lastIndexOf(" "),
                           subStr.lastIndexOf("-"));
      }
      else
      {
        // this is the last line, only break at spaces so we can add ellipsis
        splitAt = subStr.lastIndexOf(" ");
      }

      // if splitAt == -1 then there is an unbroken set of characters that is
      // longer than max_width. Otherwise if splitAt != -1 then we can continue
      // the loop until the string is <= max_width
      if (splitAt != -1)
      {
        endIndex = splitAt;
        subStr = subStr.substring(0, endIndex + 1);

        char lastChar = subStr.charAt(endIndex);
        if (lastChar == ' ')
        {
          // remove this char now so it is not counted towards line width
          keepLastChar = 0;
          subStr.remove(endIndex);
          --endIndex;
        }
        else if (lastChar == '-')
        {
          // this char will be printed on this line and removed next iteration
          keepLastChar = 1;
        }

        if (current_line < max_lines - 1)
        {
          // this is not the last line
          display.getTextBounds(subStr, 0, 0, &x1, &y1, &w, &h);
        }
        else
        {
          // this is the last line, we need to make sure there is space for
          // ellipsis
          display.getTextBounds(subStr + "...", 0, 0, &x1, &y1, &w, &h);
          if (w <= max_width)
          {
            // ellipsis fit, add them to subStr
            subStr = subStr + "...";
          }
        }

      } // end if (splitAt != -1)
    } // end inner while

    drawString(x, y + (current_line * line_spacing), subStr, alignment, color);

    // update textRemaining to no longer include what was printed
    // +1 for exclusive bounds, +1 to get passed space/dash
    textRemaining = textRemaining.substring(endIndex + 2 - keepLastChar);

    ++current_line;
  } // end outer while

  return;
} // end drawMultiLnString

/* Initialize e-paper display
 */
void initDisplay()
{
  pinMode(PIN_EPD_PWR, OUTPUT);
  digitalWrite(PIN_EPD_PWR, HIGH);
#ifdef DRIVER_WAVESHARE
  display.init(115200, true, 2, false);
#endif
#ifdef DRIVER_DESPI_C02
  display.init(115200, true, 10, false);
#endif
  // remap spi
  SPI.end();
  SPI.begin(PIN_EPD_SCK,
            PIN_EPD_MISO,
            PIN_EPD_MOSI,
            PIN_EPD_CS);

  display.setRotation(0);
  display.setTextSize(1);
  display.setTextColor(GxEPD_BLACK);
  display.setTextWrap(false);
  // display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
  display.firstPage(); // use paged drawing mode, sets fillScreen(GxEPD_WHITE)
  return;
} // end initDisplay

/* Power-off e-paper display
 */
void powerOffDisplay()
{
  display.hibernate(); // turns powerOff() and sets controller to deep sleep for
                       // minimum power use
  digitalWrite(PIN_EPD_PWR, LOW);
  return;
} // end initDisplay


/* These functions are responsible for drawing the current conditions and
 * associated icons on the left panel.
 */

// drawCurrentSunrise
#ifdef POS_SUNRISE
void drawCurrentSunrise(const char* sunriseTimeStr)
{
  int PosX = POS_SUNRISE % 2;
  int PosY = static_cast<int>(POS_SUNRISE / 2);
    // icons
  display.drawInvertedBitmap(162 * PosX, 204 + (48 + 8) * PosY,
                             wi_sunrise_48x48, 48, 48, GxEPD_BLACK);

  // labels
  display.setFont(&FONT_7pt8b);
  drawString(48 + (162 * PosX), 204 + 10 + (48 + 8) * PosY, TXT_SUNRISE, LEFT);

  // sunrise - pre-formatted by TimeCoordinator
  display.setFont(&FONT_12pt8b);
  drawString(48 + (162 * PosX), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2, sunriseTimeStr, LEFT);

  return;
}
# endif
// end drawCurrentSunrise

// drawCurrentWind
#ifdef POS_WIND
void drawCurrentWind(const owm_current_t &current)
{
  String dataStr, unitStr;
  int PosX = (POS_WIND % 2);
  int PosY = static_cast<int>(POS_WIND / 2);

  // icons
  display.drawInvertedBitmap(162 * PosX, 204 + (48 + 8) * PosY,
                             wi_strong_wind_48x48, 48, 48, GxEPD_BLACK);

  // labels
  display.setFont(&FONT_7pt8b);
  drawString(48 + (162 * PosX), 204 + 10 + (48 + 8) * PosY, TXT_WIND, LEFT);

  // wind
  display.setFont(&FONT_12pt8b);
  
  // Set unit string based on configuration (always shown)
#ifdef UNITS_SPEED_METERSPERSECOND
  unitStr = String(" ") + TXT_UNITS_SPEED_METERSPERSECOND;
#endif
#ifdef UNITS_SPEED_FEETPERSECOND
  unitStr = String(" ") + TXT_UNITS_SPEED_FEETPERSECOND;
#endif
#ifdef UNITS_SPEED_KILOMETERSPERHOUR
  unitStr = String(" ") + TXT_UNITS_SPEED_KILOMETERSPERHOUR;
#endif
#ifdef UNITS_SPEED_MILESPERHOUR
  unitStr = String(" ") + TXT_UNITS_SPEED_MILESPERHOUR;
#endif
#ifdef UNITS_SPEED_KNOTS
  unitStr = String(" ") + TXT_UNITS_SPEED_KNOTS;
#endif
#ifdef UNITS_SPEED_BEAUFORT
  unitStr = String(" ") + TXT_UNITS_SPEED_BEAUFORT;
#endif
  
  // Validate wind data before displaying
  if (isValidWindSpeed(current.wind_speed)) {
#ifdef WIND_INDICATOR_ARROW
    display.drawInvertedBitmap(48 + (162 * PosX), 204 + 24 / 2 + (48 + 8) * PosY,
                               getWindBitmap24(current.wind_deg),
                               24, 24, GxEPD_BLACK);
#endif
#ifdef UNITS_SPEED_METERSPERSECOND
    dataStr = String(static_cast<int>(std::round(current.wind_speed)));
#endif
#ifdef UNITS_SPEED_FEETPERSECOND
    dataStr = String(static_cast<int>(std::round(
                     meterspersecond_to_feetpersecond(current.wind_speed) )));
#endif
#ifdef UNITS_SPEED_KILOMETERSPERHOUR
    dataStr = String(static_cast<int>(std::round(
                     meterspersecond_to_kilometersperhour(current.wind_speed) )));
#endif
#ifdef UNITS_SPEED_MILESPERHOUR
    dataStr = String(static_cast<int>(std::round(
                     meterspersecond_to_milesperhour(current.wind_speed) )));
#endif
#ifdef UNITS_SPEED_KNOTS
    dataStr = String(static_cast<int>(std::round(
                     meterspersecond_to_knots(current.wind_speed) )));
#endif
#ifdef UNITS_SPEED_BEAUFORT
    dataStr = String(meterspersecond_to_beaufort(current.wind_speed));
#endif
  } else {
    // Invalid wind data - show "--"
    dataStr = "--";
  }

#ifdef WIND_INDICATOR_ARROW
  drawString( (48 + 24)+ (162 * PosX), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2, dataStr, LEFT);
#else
  drawString(48    + (162 * PosX) , 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2, dataStr, LEFT);
#endif
  display.setFont(&FONT_8pt8b);
  drawString(display.getCursorX(), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2,
             unitStr, LEFT);

#if defined(WIND_INDICATOR_NUMBER)
  dataStr = String(current.wind_deg) + "\260";
  display.setFont(&FONT_12pt8b);
  drawString(display.getCursorX() + 6, 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2,
             dataStr, LEFT);
#endif
#if defined(WIND_INDICATOR_CPN_CARDINAL)                \
 || defined(WIND_INDICATOR_CPN_INTERCARDINAL)           \
 || defined(WIND_INDICATOR_CPN_SECONDARY_INTERCARDINAL) \
 || defined(WIND_INDICATOR_CPN_TERTIARY_INTERCARDINAL)
  dataStr = getCompassPointNotation(current.wind_deg);
  display.setFont(&FONT_12pt8b);
  drawString(display.getCursorX() + 6, 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2,
             dataStr, LEFT);
#endif

  return;
}
#endif
// end drawCurrentWind

// drawCurrentUVI
#ifdef POS_UVI
void drawCurrentUVI(const owm_current_t &current)
{
  String dataStr, unitStr;
  int PosX = (POS_UVI % 2);
  int PosY = static_cast<int>(POS_UVI / 2);

  // icons
  display.drawInvertedBitmap(162 * PosX, 204 + (48 + 8) * PosY,
                             wi_day_sunny_48x48, 48, 48, GxEPD_BLACK);

  // labels
  display.setFont(&FONT_7pt8b);
  drawString(48 + (162 * PosX), 204 + 10 + (48 + 8) * PosY, TXT_UV_INDEX, LEFT);

  // spacing between end of index value and start of descriptor text
  const int sp = 8;

  // uv index
  display.setFont(&FONT_12pt8b);
  
  // Validate UVI data before displaying
  if (isValidUVI(current.uvi)) {
    unsigned int uvi = static_cast<unsigned int>(
                                  std::max(std::round(current.uvi), 0.0f));
    dataStr = String(uvi);
    drawString(48 + (162 * PosX), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2, dataStr, LEFT);
    display.setFont(&FONT_7pt8b);
    dataStr = String(getUVIdesc(uvi));
    int max_w = (162 + (PosX * 162) - sp) - (display.getCursorX() + sp);
    if (getStringWidth(dataStr) <= max_w)
    { // Fits on a single line, draw along bottom
      drawString(display.getCursorX() + sp, 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2,
                 dataStr, LEFT);
    }
    else
    { // use smaller font
      display.setFont(&FONT_5pt8b);
      if (getStringWidth(dataStr) <= max_w)
      { // Fits on a single line with smaller font, draw along bottom
        drawString(display.getCursorX() + sp,
                   204 + 17 / 2 + (48 + 8) * PosY + 48 / 2,
                   dataStr, LEFT);
      }
      else
      { // Does not fit on a single line, draw higher to allow room for 2nd line
        drawMultiLnString(display.getCursorX() + sp,
                          204 + 17 / 2 + (48 + 8) * PosY + 48 / 2 - 10,
                          dataStr, LEFT, max_w, 2, 10);
      }
    }
  } else {
    // Invalid UVI data - show "--"
    dataStr = "--";
    drawString(48 + (162 * PosX), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2, dataStr, LEFT);
  }
  return;
}
#endif
// end drawCurrentUVI

// drawCurrentAirQuality
#ifdef POS_AIR_QULITY
void drawCurrentAirQuality(const owm_resp_air_pollution_t &owm_air_pollution)
{
  String dataStr, unitStr;
  int PosX = (POS_AIR_QULITY % 2);
  int PosY = static_cast<int>(POS_AIR_QULITY / 2);

  // icons
  display.drawInvertedBitmap(162 * PosX, 204 + (48 + 8) * PosY,
                             air_filter_48x48, 48, 48, GxEPD_BLACK);

  // labels
  display.setFont(&FONT_7pt8b);

  const char *air_quality_index_label;
  if (aqi_desc_type(AQI_SCALE) == AIR_QUALITY_DESC)
  {
    air_quality_index_label = TXT_AIR_QUALITY;
  }
  else // (aqi_desc_type(AQI_SCALE) == AIR_POLLUTION_DESC)
  {
    air_quality_index_label = TXT_AIR_POLLUTION;
  }
  drawString(48 + (162 * PosX), 204 + 10 + (48 + 8) * PosY, air_quality_index_label, LEFT);

  // spacing between end of index value and start of descriptor text
  const int sp = 8;

  // air quality index
  display.setFont(&FONT_12pt8b);
  const owm_components_t &c = owm_air_pollution.components;
  // OpenWeatherMap does not provide pb (lead) conentrations, so we pass NULL.
  int aqi = calc_aqi(AQI_SCALE, c.co, c.nh3, c.no, c.no2, c.o3, NULL, c.so2,
                                c.pm10, c.pm2_5);
  int aqi_max = aqi_scale_max(AQI_SCALE);
  if (aqi > aqi_max)
  {
    dataStr = "> " + String(aqi_max);
  }
  else
  {
    dataStr = String(aqi);
  }
  drawString(48 + (162 * PosX), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2, dataStr, LEFT);
  display.setFont(&FONT_7pt8b);
  dataStr = String(aqi_desc(AQI_SCALE, aqi));
  int max_w = (162 + (PosX * 162) - sp) - (display.getCursorX() + sp);
  if (getStringWidth(dataStr) <= max_w)
  { // Fits on a single line, draw along bottom
    drawString(display.getCursorX() + sp, 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2,
               dataStr, LEFT);
  }
  else
  { // use smaller font
    display.setFont(&FONT_5pt8b);
    if (getStringWidth(dataStr) <= max_w)
    { // Fits on a single line with smaller font, draw along bottom
      drawString(display.getCursorX() + sp,
                 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2,
                 dataStr, LEFT);
    }
    else
    { // Does not fit on a single line, draw higher to allow room for 2nd line
      drawMultiLnString(display.getCursorX() + sp,
                        204 + 17 / 2 + (48 + 8) * PosY + 48 / 2 - 10,
                        dataStr, LEFT, max_w, 2, 10);
    }
  }

  return;
}
#endif
// end drawCurrentAirQuality

// drawCurrentInTemp
#ifdef POS_INTEMP
void drawCurrentInTemp(float inTemp)
{
  String dataStr, unitStr;
  int PosX = (POS_INTEMP % 2);
  int PosY = static_cast<int>(POS_INTEMP / 2);

  // icons
  display.drawInvertedBitmap(162 * PosX, 204 + (48 + 8) * PosY,
                             house_thermometer_48x48, 48, 48, GxEPD_BLACK);

  // labels
  display.setFont(&FONT_7pt8b);
  drawString(48 + (162 * PosX), 204 + 10 + (48 + 8) * PosY, TXT_INDOOR_TEMPERATURE, LEFT);

  // indoor temperature
  display.setFont(&FONT_12pt8b);
  if (!std::isnan(inTemp))
  {
#ifdef UNITS_TEMP_KELVIN
    dataStr = String(std::round(celsius_to_kelvin(inTemp) * 10) / 10.0f, 1);
#endif
#ifdef UNITS_TEMP_CELSIUS
    dataStr = String(std::round(inTemp * 10) / 10.0f, 1);
#endif
#ifdef UNITS_TEMP_FAHRENHEIT
    dataStr = String(static_cast<int>(
              std::round(celsius_to_fahrenheit(inTemp))));
#endif
  }
  else
  {
    dataStr = "--";
  }
#if defined(UNITS_TEMP_CELSIUS) || defined(UNITS_TEMP_FAHRENHEIT)
  dataStr += "\260";
#endif
  drawString(48 + (162 * PosX), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2, dataStr, LEFT);
  return;
}
#endif
// end drawCurrentInTemp

// drawCurrentSunset
#ifdef POS_SUNSET
void drawCurrentSunset(const char* sunsetTimeStr)
{
  int PosX = (POS_SUNSET % 2);
  int PosY = static_cast<int>(POS_SUNSET / 2);
  // icons
  display.drawInvertedBitmap(162 * PosX, 204 + (48 + 8) * PosY,
                             wi_sunset_48x48, 48, 48, GxEPD_BLACK);

  // labels
  display.setFont(&FONT_7pt8b);
  drawString(48 + (162 * PosX), 204 + 10 + (48 + 8) * PosY, TXT_SUNSET, LEFT);

  // sunset - pre-formatted by TimeCoordinator
  display.setFont(&FONT_12pt8b);
  drawString(48 + (162 * PosX), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2, sunsetTimeStr, LEFT);

  return;
}
#endif
// end drawCurrentSunset

// drawCurrentHumidity
#ifdef POS_HUMIDITY
void drawCurrentHumidity(const owm_current_t &current)
{
  String dataStr, unitStr;
  int PosX = (POS_HUMIDITY % 2);
  int PosY = static_cast<int>(POS_HUMIDITY / 2);

  // icons
  display.drawInvertedBitmap(162 * PosX, 204 + (48 + 8) * PosY,
                             wi_humidity_48x48, 48, 48, GxEPD_BLACK);

  // labels
  display.setFont(&FONT_7pt8b);
  drawString(48 + (162 * PosX), 204 + 10 + (48 + 8) * PosY, TXT_HUMIDITY, LEFT);

  // humidity
  display.setFont(&FONT_12pt8b);
  if (isValidHumidity(current.humidity)) {
    dataStr = String(current.humidity);
  } else {
    dataStr = "--";
  }
  drawString(48 + (162 * PosX), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2, dataStr, LEFT);
  display.setFont(&FONT_8pt8b);
  drawString(display.getCursorX(), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2,
             "%", LEFT);
  return;
}
#endif
// end drawCurrentHumidity

// drawCurrentPressure
#ifdef POS_PRESSURE
void drawCurrentPressure(const owm_current_t &current)
{
  String dataStr, unitStr;
  int PosX = (POS_PRESSURE % 2);
  int PosY = static_cast<int>(POS_PRESSURE / 2);
  //  icons
  display.drawInvertedBitmap(162 * PosX, 204 + (48 + 8) * PosY,
                             wi_barometer_48x48, 48, 48, GxEPD_BLACK);

  //  labels
  display.setFont(&FONT_7pt8b);
  drawString(48 + (162 * PosX), 204 + 10 + (48 + 8) * PosY, TXT_PRESSURE, LEFT);

  // pressure
  // Set unit string based on configuration (always shown)
#ifdef UNITS_PRES_HECTOPASCALS
  unitStr = String(" ") + TXT_UNITS_PRES_HECTOPASCALS;
#endif
#ifdef UNITS_PRES_PASCALS
  unitStr = String(" ") + TXT_UNITS_PRES_PASCALS;
#endif
#ifdef UNITS_PRES_MILLIMETERSOFMERCURY
  unitStr = String(" ") + TXT_UNITS_PRES_MILLIMETERSOFMERCURY;
#endif
#ifdef UNITS_PRES_INCHESOFMERCURY
  unitStr = String(" ") + TXT_UNITS_PRES_INCHESOFMERCURY;
#endif
#ifdef UNITS_PRES_MILLIBARS
  unitStr = String(" ") + TXT_UNITS_PRES_MILLIBARS;
#endif
#ifdef UNITS_PRES_ATMOSPHERES
  unitStr = String(" ") + TXT_UNITS_PRES_ATMOSPHERES;
#endif
#ifdef UNITS_PRES_GRAMSPERSQUARECENTIMETER
  unitStr = String(" ") + TXT_UNITS_PRES_GRAMSPERSQUARECENTIMETER;
#endif
#ifdef UNITS_PRES_POUNDSPERSQUAREINCH
  unitStr = String(" ") + TXT_UNITS_PRES_POUNDSPERSQUAREINCH;
#endif

  // Validate pressure data before displaying
  display.setFont(&FONT_12pt8b);
  if (isValidPressure(static_cast<float>(current.pressure))) {
#ifdef UNITS_PRES_HECTOPASCALS
    dataStr = String(current.pressure);
#endif
#ifdef UNITS_PRES_PASCALS
    dataStr = String(static_cast<int>(std::round(
                     hectopascals_to_pascals(current.pressure) )));
#endif
#ifdef UNITS_PRES_MILLIMETERSOFMERCURY
    dataStr = String(static_cast<int>(std::round(
                     hectopascals_to_millimetersofmercury(current.pressure) )));
#endif
#ifdef UNITS_PRES_INCHESOFMERCURY
    dataStr = String(std::round(1e1f *
                     hectopascals_to_inchesofmercury(current.pressure)
                     ) / 1e1f, 1);
#endif
#ifdef UNITS_PRES_MILLIBARS
    dataStr = String(static_cast<int>(std::round(
                     hectopascals_to_millibars(current.pressure) )));
#endif
#ifdef UNITS_PRES_ATMOSPHERES
    dataStr = String(std::round(1e3f *
                     hectopascals_to_atmospheres(current.pressure) )
                     / 1e3f, 3);
#endif
#ifdef UNITS_PRES_GRAMSPERSQUARECENTIMETER
    dataStr = String(static_cast<int>(std::round(
                     hectopascals_to_gramspersquarecentimeter(current.pressure)
                     )));
#endif
#ifdef UNITS_PRES_POUNDSPERSQUAREINCH
    dataStr = String(std::round(1e2f *
                     hectopascals_to_poundspersquareinch(current.pressure)
                     ) / 1e2f, 2);
#endif
  } else {
    dataStr = "--";
  }
  
  drawString(48 + (162 * PosX), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2, dataStr, LEFT);
  display.setFont(&FONT_8pt8b);
  drawString(display.getCursorX(), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2,
             unitStr, LEFT);

  return;
}
#endif
// end drawCurrentPressure

// drawCurrentVisibility
#ifdef POS_VISIBILITY
void drawCurrentVisibility(const owm_current_t &current)
{
  String dataStr, unitStr;
  int PosX = (POS_VISIBILITY % 2);
  int PosY = static_cast<int>(POS_VISIBILITY / 2);

  // icons
  display.drawInvertedBitmap(162 * PosX, 204 + (48 + 8) * PosY,
                             visibility_icon_48x48, 48, 48, GxEPD_BLACK);

  // labels
  display.setFont(&FONT_7pt8b);
  drawString(48 + (162 * PosX), 204 + 10 + (48 + 8) * PosY, TXT_VISIBILITY, LEFT);

  // visibility
  display.setFont(&FONT_12pt8b);
  
  // Set unit string based on configuration (always shown)
#ifdef UNITS_DIST_KILOMETERS
  unitStr = String(" ") + TXT_UNITS_DIST_KILOMETERS;
#endif
#ifdef UNITS_DIST_MILES
  unitStr = String(" ") + TXT_UNITS_DIST_MILES;
#endif

  // Calculate visibility value (used for validation)
#ifdef UNITS_DIST_KILOMETERS
  float vis = meters_to_kilometers(current.visibility);
#endif
#ifdef UNITS_DIST_MILES
  float vis = meters_to_miles(current.visibility);
#endif

  // Validate visibility data before displaying
  if (isValidVisibility(current.visibility)) {
    // if visibility is less than 1.95, round to 1 decimal place
    // else round to int
    if (vis < 1.95)
    {
      dataStr = String(std::round(10 * vis) / 10.0, 1);
    }
    else
    {
      dataStr = String(static_cast<int>(std::round(vis)));
    }
#ifdef UNITS_DIST_KILOMETERS
    if (vis >= 10)
    {
      dataStr = "> " + dataStr;
    }
#endif
#ifdef UNITS_DIST_MILES
    if (vis >= 6)
    {
      dataStr = "> " + dataStr;
    }
#endif
  } else {
    dataStr = "--";
  }
  
  drawString(48 + (162 * PosX), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2, dataStr, LEFT);
  display.setFont(&FONT_8pt8b);
  drawString(display.getCursorX(), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2,
             unitStr, LEFT);

  return;
}
#endif
// end drawCurrentVisibility

// drawCurrentInHumidit
#ifdef POS_INHUMIDITY
void drawCurrentInHumidity(float inHumidity)
{
  String dataStr, unitStr;
  int PosX = (POS_INHUMIDITY % 2);
  int PosY = static_cast<int>(POS_INHUMIDITY / 2);

  // current weather data icons
  display.drawInvertedBitmap(162 * PosX, 204 + (48 + 8) * PosY,
                             house_humidity_48x48, 48, 48, GxEPD_BLACK);

  // current weather data labels
  display.setFont(&FONT_7pt8b);
  drawString(48 + (162 * PosX), 204 + 10 + (48 + 8) * PosY, TXT_INDOOR_HUMIDITY, LEFT);

  // indoor humidity
  display.setFont(&FONT_12pt8b);
  if (!std::isnan(inHumidity))
  {
    dataStr = String(static_cast<int>(std::round(inHumidity)));
  }
  else
  {
    dataStr = "--";
  }
  drawString(48 + (162 * PosX), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2, dataStr, LEFT);
  display.setFont(&FONT_8pt8b);
  drawString(display.getCursorX(), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2,
             "%", LEFT);
  return;
}
#endif
// end drawCurrentInHumidity

// drawCurrentMoonrise
#ifdef POS_MOONRISE
void drawCurrentMoonrise(const char* moonriseTimeStr)
{
  int PosX = POS_MOONRISE % 2;
  int PosY = static_cast<int>(POS_MOONRISE / 2);

  // icons
  display.drawInvertedBitmap(162 * PosX, 204 + (48 + 8) * PosY,
                             wi_moonrise_48x48, 48, 48, GxEPD_BLACK);

  // labels
  display.setFont(&FONT_7pt8b);
  drawString(48 + (162 * PosX), 204 + 10 + (48 + 8) * PosY, TXT_MOONRISE, LEFT);

  // moonrise
  display.setFont(&FONT_12pt8b);
  drawString(48 + (162 * PosX), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2,
             moonriseTimeStr, LEFT);

  return;
}
# endif
// end drawCurrentMoonrise

// drawCurrentMoonset
#ifdef POS_MOONSET
void drawCurrentMoonset(const char* moonsetTimeStr)
{
  int PosX = (POS_MOONSET % 2);
  int PosY = static_cast<int>(POS_MOONSET / 2);
  // icons
  display.drawInvertedBitmap(162 * PosX, 204 + (48 + 8) * PosY,
                             wi_moonset_48x48, 48, 48, GxEPD_BLACK);

  // labels
  display.setFont(&FONT_7pt8b);
  drawString(48 + (162 * PosX), 204 + 10 + (48 + 8) * PosY, TXT_MOONSET, LEFT);

  // moonset
  display.setFont(&FONT_12pt8b);
  drawString(48 + (162 * PosX), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2,
             moonsetTimeStr, LEFT);

  return;
}
#endif
// end drawCurrentMoonset

// drawCurrentMoonphase
#ifdef POS_MOONPHASE
void drawCurrentMoonphase(const owm_daily_t &daily)
{
  String dataStr, unitStr;
  int PosX = (POS_MOONPHASE % 2);
  int PosY = static_cast<int>(POS_MOONPHASE / 2);

  // icons
  display.drawInvertedBitmap(162 * PosX, 204 + (48 + 8) * PosY,
                             getMoonPhaseBitmap48(daily), 48, 48, GxEPD_BLACK);

  // labels
  display.setFont(&FONT_7pt8b);
  drawString(48 + (162 * PosX), 204 + 10 + (48 + 8) * PosY, TXT_MOONPHASE, LEFT);

  // moonphase
  const int sp = 8;
  dataStr = String(getMoonPhaseStr(daily));
  int max_w = (162 + (PosX * 162) - sp) - (48 + (PosX * 162));
  if (getStringWidth(dataStr) <= max_w)
  { // Fits on a single line, draw along bottom
    drawString(48 + (162 * PosX), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2,
               dataStr, LEFT);
  }
  else
  { // use smaller font
    display.setFont(&FONT_5pt8b);
    if (getStringWidth(dataStr) <= max_w)
    { // Fits on a single line with smaller font, draw along bottom
      drawString(48 + (162 * PosX),
                 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2,
                 dataStr, LEFT);
    }
    else
    { // Does not fit on a single line, draw higher to allow room for 2nd line
      drawMultiLnString(48 + (162 * PosX),
                        204 + 17 / 2 + (48 + 8) * PosY + 48 / 2 - 10,
                        dataStr, LEFT, max_w, 2, 10);
    }
  }

  return;
}
#endif
// end drawCurrentMoonphase

// drawCurrentDewpoint
#ifdef POS_DEWPOINT
void drawCurrentDewpoint(const owm_current_t &current)
{
  String dataStr, unitStr;
  int PosX = (POS_DEWPOINT % 2);
  int PosY = static_cast<int>(POS_DEWPOINT / 2);
  
  // icons
  display.drawInvertedBitmap(162 * PosX, 204 + (48 + 8) * PosY,
                             wi_thermometer_48x48, 48, 48, GxEPD_BLACK);
  display.drawInvertedBitmap(162 * PosX + 48 - 24, 204 + (48 + 8) * PosY + 4,
                             wi_raindrops_24x24, 24, 24, GxEPD_BLACK);
  
  // labels
  display.setFont(&FONT_7pt8b);
  drawString(48 + (162 * PosX), 204 + 10 + (48 + 8) * PosY, TXT_DEWPOINT, LEFT);

  // Dew point
  display.setFont(&FONT_12pt8b);
  if (!std::isnan(current.dew_point))
  {
#ifdef UNITS_TEMP_KELVIN
  dataStr = String(std::round(current.dew_point * 10) / 10.0f, 1);
#endif
#ifdef UNITS_TEMP_CELSIUS
  dataStr = String(std::round(kelvin_to_celsius(current.dew_point) * 10) / 10.0f, 1);
#endif
#ifdef UNITS_TEMP_FAHRENHEIT
  dataStr = String(static_cast<int>(
            std::round(kelvin_to_fahrenheit(current.dew_point))));
#endif
  }
  else
  {
    dataStr = "--";
  }
#if defined(UNITS_TEMP_CELSIUS) || defined(UNITS_TEMP_FAHRENHEIT)
  dataStr += "\260";
#endif
  drawString(48 + (162 * PosX), 204 + 17 / 2 + (48 + 8) * PosY + 48 / 2, dataStr, LEFT);
  return;
} 
#endif
// end drawCurrentDewpoint

//End defining functions for left panel.

/* Draws a compact umbrella widget (128x128) showing rain alert status.
 * x, y: top-left position
 * hourly: hourly forecast data
 * hours: number of hours to check
 * current_dt: current timestamp to calculate time until rain
 * rainTimeStr: pre-formatted rain time string from TimeCoordinator
 * 
 * Uses UmbrellaParser for data validation and wind-optional logic.
 */
void drawUmbrellaWidget(int x, int y, const owm_hourly_t *hourly, int hours,
                         int64_t current_dt, const char* rainTimeStr)
{
  // Position configuration - adjust these to move elements
  const int ICON_OFFSET_Y = -15;        // Vertical offset of umbrella icon
  const int TEXT_OFFSET_Y = 110;       // Vertical offset of text line
  const int X_LINE1_START_X = 10;      // X start position for first diagonal line
  const int X_LINE1_END_X = 118;       // X end position for first diagonal line
  const int X_LINE_START_Y = -2;       // Y start position for X lines
  const int X_LINE_END_Y = 106;        // Y end position for X lines
  
  int centerX = x + 64; // Center of 128px width
  
  // Parse and validate API data using UmbrellaParser
  UmbrellaData data = UmbrellaParser::parse(hourly, hours, current_dt, rainTimeStr);
  
  // State 1: No rain (POP < 20%)
  // Shows: Umbrella with X overlay + "No rain (X%)"
  // Note: POP < 20% is always "No rain", regardless of wind
  if (data.state == UMBRELLA_NO_RAIN) {
    // Draw umbrella icon
    display.drawInvertedBitmap(x, y + ICON_OFFSET_Y, wi_umbrella_128x128, 128, 128, GxEPD_BLACK);
    // Draw thick X over icon (3 parallel lines for thickness)
    for (int offset = -1; offset <= 1; offset++) {
      display.drawLine(x + X_LINE1_START_X + offset, y + X_LINE_START_Y, 
                       x + X_LINE1_END_X + offset, y + X_LINE_END_Y, GxEPD_BLACK);
      display.drawLine(x + X_LINE1_END_X + offset, y + X_LINE_START_Y, 
                       x + X_LINE1_START_X + offset, y + X_LINE_END_Y, GxEPD_BLACK);
    }
    // Text below with probability in parentheses
    display.setFont(&FONT_8pt8b);
    drawString(centerX, y + TEXT_OFFSET_Y, "No rain (" + data.popDisplayStr + "%)", CENTER);
  }
  // State 3: Take umbrella (POP >= 70%)
  // Shows: Open umbrella icon + "Rain at..." 
  else if (data.state == UMBRELLA_TAKE) {
    // Draw open umbrella icon
    display.drawInvertedBitmap(x, y + ICON_OFFSET_Y, wi_umbrella_128x128, 128, 128, GxEPD_BLACK);
    
    display.setFont(&FONT_8pt8b);
    if (data.minutesUntilRainValid && data.minutesUntilRain > 0 && 
        data.rainTimeDisplayStr != UMBRELLA_DEFAULT_DISPLAY_VALUE) {
      drawString(centerX, y + TEXT_OFFSET_Y, 
                 "Rain " + data.rainTimeDisplayStr + " (" + data.popDisplayStr + "%)", CENTER);
    } else {
      drawString(centerX, y + TEXT_OFFSET_Y, 
                 "Rain now (" + data.popDisplayStr + "%)", CENTER);
    }
  }
  // State: Too windy (POP >= 20% AND wind >= 18 km/h)
  // Shows: Open umbrella icon + "Too windy (X km/h)"
  // Note: Only reached if wind data is valid
  else if (data.state == UMBRELLA_TOO_WINDY) {
    // Draw open umbrella icon
    display.drawInvertedBitmap(x, y + ICON_OFFSET_Y, wi_umbrella_128x128, 128, 128, GxEPD_BLACK);
    
    display.setFont(&FONT_8pt8b);
    drawString(centerX, y + TEXT_OFFSET_Y, 
               "Too windy (" + data.windDisplayStr + data.windUnitStr + ")", CENTER);
  }
  // State 2: Compact (POP 20-69% AND (wind < 18 km/h OR no wind data))
  // Shows: Closed umbrella icon + "Rain at..."
  else {
    // Draw closed umbrella icon (U+1F302 🌂 style)
    display.drawInvertedBitmap(x, y + ICON_OFFSET_Y, wi_closed_umbrella_128x128, 128, 128, GxEPD_BLACK);
    
    display.setFont(&FONT_8pt8b);
    if (data.minutesUntilRainValid && data.minutesUntilRain > 0 &&
        data.rainTimeDisplayStr != UMBRELLA_DEFAULT_DISPLAY_VALUE) {
      drawString(centerX, y + TEXT_OFFSET_Y, 
                 "Rain " + data.rainTimeDisplayStr + " (" + data.popDisplayStr + "%)", CENTER);
    } else {
      drawString(centerX, y + TEXT_OFFSET_Y, 
                 "Rain soon (" + data.popDisplayStr + "%)", CENTER);
    }
  }
}

/* This function is responsible for drawing the current conditions and
 * associated icons.
 */
void drawCurrentConditions(const owm_current_t &current,
                           const owm_daily_t &today,
                           const owm_resp_air_pollution_t &owm_air_pollution,
                           float inTemp, float inHumidity,
                           const owm_hourly_t *hourly,
                           const char* sunriseTimeStr,
                           const char* sunsetTimeStr,
                           const char* moonriseTimeStr,
                           const char* moonsetTimeStr,
                           const char* rainTimeStr)
{
    String dataStr, unitStr;

    // ==================== LEFT SIDE: Large Temperature Widget ====================
#ifdef UNITS_TEMP_KELVIN
    dataStr = String(static_cast<int>(std::round(current.temp)));
    unitStr = TXT_UNITS_TEMP_KELVIN;
#endif
#ifdef UNITS_TEMP_CELSIUS
    dataStr = String(static_cast<int>(std::round(kelvin_to_celsius(current.temp))));
    unitStr = TXT_UNITS_TEMP_CELSIUS;
#endif
#ifdef UNITS_TEMP_FAHRENHEIT
    dataStr = String(static_cast<int>(std::round(kelvin_to_fahrenheit(current.temp))));
    unitStr = TXT_UNITS_TEMP_FAHRENHEIT;
#endif

    // Temperature (large font)
    display.setFont(&FONT_48pt8b_temperature);
    drawString(164 / 2 - 20, 196 / 2 + 69 / 2, dataStr, CENTER);

    // Unit
    display.setFont(&FONT_14pt8b);
    drawString(display.getCursorX(), 196 / 2 - 69 / 2 + 20, unitStr, LEFT);

    // Feels Like
#ifdef UNITS_TEMP_KELVIN
    dataStr = String(TXT_FEELS_LIKE) + " " + String(static_cast<int>(std::round(current.feels_like)));
#endif
#ifdef UNITS_TEMP_CELSIUS
    dataStr = String(TXT_FEELS_LIKE) + " " 
              + String(static_cast<int>(std::round(kelvin_to_celsius(current.feels_like)))) + '\260';
#endif
#ifdef UNITS_TEMP_FAHRENHEIT
    dataStr = String(TXT_FEELS_LIKE) + " " 
              + String(static_cast<int>(std::round(kelvin_to_fahrenheit(current.feels_like)))) + '\260';
#endif

    display.setFont(&FONT_12pt8b);
    drawString(164 / 2, 98 + 69 / 2 + 12 + 17, dataStr, CENTER);

    // ==================== RIGHT SIDE: Cloud (top) + Umbrella (bottom) ====================
    const int rightX = 164 + 16;   // same X position as original right column

    // 1. Cloud / Current Condition Icon (Top) - 96x96 bitmap
    // Snow icon needs to be shifted left due to visual offset
    // int iconOffsetX = (current.weather.id >= 600 && current.weather.id < 700) ? 18 : 18;
    int iconOffsetX = 18;
    display.drawInvertedBitmap(rightX + iconOffsetX, -10,
        getCurrentConditionsBitmap96(current, today),
        96, 96, GxEPD_BLACK);

    // 2. Umbrella Widget (Bottom) - closer to cloud
    if (hourly != nullptr) {
        drawUmbrellaWidget(rightX, 75, hourly, 24, current.dt, rainTimeStr);
    }

    // ==================== LEFT PANEL: All other current data (unchanged) ====================
#ifdef POS_SUNRISE
    drawCurrentSunrise(sunriseTimeStr);
#endif
#ifdef POS_SUNSET
    drawCurrentSunset(sunsetTimeStr);
#endif
#ifdef POS_WIND
    drawCurrentWind(current);
#endif
#ifdef POS_HUMIDITY
    drawCurrentHumidity(current);
#endif
#ifdef POS_UVI
    drawCurrentUVI(current);
#endif
#ifdef POS_PRESSURE
    drawCurrentPressure(current);
#endif
#ifdef POS_VISIBILITY
    drawCurrentVisibility(current);
#endif
#ifdef POS_AIR_QULITY
    drawCurrentAirQuality(owm_air_pollution);
#endif
#ifdef POS_INTEMP
    drawCurrentInTemp(inTemp);
#endif
#ifdef POS_INHUMIDITY
    drawCurrentInHumidity(inHumidity);
#endif
#ifdef POS_MOONRISE
    drawCurrentMoonrise(moonriseTimeStr);
#endif
#ifdef POS_MOONSET
    drawCurrentMoonset(moonsetTimeStr);
#endif
#ifdef POS_MOONPHASE
    drawCurrentMoonphase(today);
#endif
#ifdef POS_DEWPOINT
    drawCurrentDewpoint(current);
#endif

    return;
} // end drawCurrentConditions

/* This function is responsible for drawing the five day forecast.
 * dayOfWeekArray: pre-calculated day of week (0=Sun, 1=Mon, etc.)
 * No timezone calculations here - all done by TimeCoordinator.
 */
void drawForecast(const owm_daily_t *daily, const int* dayOfWeekArray)
{
  // 5 day forecast starting today; weekdays are forced sequential to avoid repeated labels
  String hiStr, loStr;
  String dataStr, unitStr;
  int baseWday = dayOfWeekArray[0];
  for (int i = 0; i < 5; ++i)
  {
#ifndef DISP_BW_V1
    int x = 398 + (i * 82);
#elif defined(DISP_BW_V1)
    int x = 318 + (i * 64);
#endif
    const owm_daily_t &day = daily[i];
    // icons
    display.drawInvertedBitmap(x, 98 + 69 / 2 - 32 - 6,
                               getDailyForecastBitmap64(day),
                               64, 64, GxEPD_BLACK);
    // day of week label - calculate sequential weekday labels from today
    display.setFont(&FONT_11pt8b);
    char dayBuffer[8] = {};
    tm tmpTm = {};  // Only wday is used by _strftime for "%a"
    tmpTm.tm_wday = (baseWday + i) % 7;
    _strftime(dayBuffer, sizeof(dayBuffer), "%a", &tmpTm); // abbreviated day
    drawString(x + 31 - 2, 98 + 69 / 2 - 32 - 26 - 6 + 16, dayBuffer, CENTER);

    // high | low
    display.setFont(&FONT_8pt8b);
    drawString(x + 31, 98 + 69 / 2 + 38 - 6 + 12, "|", CENTER);
#ifdef UNITS_TEMP_KELVIN
    hiStr = String(static_cast<int>(std::round(day.temp.max)));
    loStr = String(static_cast<int>(std::round(day.temp.min)));
#endif
#ifdef UNITS_TEMP_CELSIUS
    hiStr = String(static_cast<int>(
                std::round(kelvin_to_celsius(day.temp.max)))) +
            "\260";
    loStr = String(static_cast<int>(
                std::round(kelvin_to_celsius(day.temp.min)))) +
            "\260";
#endif
#ifdef UNITS_TEMP_FAHRENHEIT
    hiStr = String(static_cast<int>(
                std::round(kelvin_to_fahrenheit(day.temp.max)))) +
            "\260";
    loStr = String(static_cast<int>(
                std::round(kelvin_to_fahrenheit(day.temp.min)))) +
            "\260";
#endif
#ifdef TEMP_ORDER_HL
    drawString(x + 31 - 4, 98 + 69 / 2 + 38 - 6 + 12, hiStr, RIGHT);
    drawString(x + 31 + 5, 98 + 69 / 2 + 38 - 6 + 12, loStr, LEFT);
#endif
#ifdef TEMP_ORDER_LH
    drawString(x + 31 - 4, 98 + 69 / 2 + 38 - 6 + 12, loStr, RIGHT);
    drawString(x + 31 + 5, 98 + 69 / 2 + 38 - 6 + 12, hiStr, LEFT);
#endif

// daily forecast precipitation
#if DISPLAY_DAILY_PRECIP
    float dailyPrecip;
#if defined(UNITS_DAILY_PRECIP_POP)
    dailyPrecip = day.pop * 100;
    dataStr = String(static_cast<int>(dailyPrecip));
    unitStr = "%";
#else
    dailyPrecip = day.snow + day.rain;
#if defined(UNITS_DAILY_PRECIP_MILLIMETERS)
    // Round up to nearest mm
    dailyPrecip = std::round(dailyPrecip);
    dataStr = String(static_cast<int>(dailyPrecip));
    unitStr = String(" ") + TXT_UNITS_PRECIP_MILLIMETERS;
#elif defined(UNITS_DAILY_PRECIP_CENTIMETERS)
    // Round up to nearest 0.1 cm
    dailyPrecip = millimeters_to_centimeters(dailyPrecip);
    dailyPrecip = std::round(dailyPrecip * 10) / 10.0f;
    dataStr = String(dailyPrecip, 1);
    unitStr = String(" ") + TXT_UNITS_PRECIP_CENTIMETERS;
#elif defined(UNITS_DAILY_PRECIP_INCHES)
    // Round up to nearest 0.1 inch
    dailyPrecip = millimeters_to_inches(dailyPrecip);
    dailyPrecip = std::round(dailyPrecip * 10) / 10.0f;
    dataStr = String(dailyPrecip, 1);
    unitStr = String(" ") + TXT_UNITS_PRECIP_INCHES;
#endif
#endif
#if (DISPLAY_DAILY_PRECIP == 2) // smart
      if (dailyPrecip > 0.0f)
      {
#endif
        display.setFont(&FONT_6pt8b);
        drawString(x + 31, 98 + 69 / 2 + 38 - 6 + 26,
                   dataStr + unitStr, CENTER);
#if (DISPLAY_DAILY_PRECIP == 2) // smart
      }
#endif
#endif // DISPLAY_DAILY_PRECIP
    }

    return;
  } // end drawForecast

  /* This function is responsible for drawing the current alerts if any.
   * Up to 2 alerts can be drawn.
   */
  void drawAlerts(std::vector<owm_alerts_t> & alerts,
                  const String &city, const String &date)
  {
#if DEBUG_LEVEL >= 1
  Serial.println("[debug] alerts.size()    : " + String(alerts.size()));
#endif
  if (alerts.size() == 0)
  { // no alerts to draw
    return;
  }

  int *ignore_list = (int *) calloc(alerts.size(), sizeof(*ignore_list));
  int *alert_indices = (int *) calloc(alerts.size(), sizeof(*alert_indices));
  if (!ignore_list || !alert_indices)
  {
    Serial.println("Error: Failed to allocate memory while handling alerts.");
    free(ignore_list);
    free(alert_indices);
    return;
  }

  // Converts all event text and tags to lowercase, removes extra information,
  // and filters out redundant alerts of lesser urgency.
  filterAlerts(alerts, ignore_list);

  // limit alert text width so that is does not run into the location or date
  // strings
  display.setFont(&FONT_16pt8b);
  int city_w = getStringWidth(city);
  display.setFont(&FONT_12pt8b);
  int date_w = getStringWidth(date);
  int max_w = DISP_WIDTH - 2 - std::max(city_w, date_w) - (196 + 4) - 8;

  // find indices of valid alerts
  int num_valid_alerts = 0;
#if DEBUG_LEVEL >= 1
  Serial.print("[debug] ignore_list      : [ ");
#endif
  for (int i = 0; i < alerts.size(); ++i)
  {
#if DEBUG_LEVEL >= 1
    Serial.print(String(ignore_list[i]) + " ");
#endif
    if (!ignore_list[i])
    {
      alert_indices[num_valid_alerts] = i;
      ++num_valid_alerts;
    }
  }
#if DEBUG_LEVEL >= 1
  Serial.println("]\n[debug] num_valid_alerts : " + String(num_valid_alerts));
#endif

  if (num_valid_alerts == 1)
  { // 1 alert
    // adjust max width to for 48x48 icons
    max_w -= 48;

    owm_alerts_t &cur_alert = alerts[alert_indices[0]];
    display.drawInvertedBitmap(196, 8, getAlertBitmap48(cur_alert), 48, 48,
                               ACCENT_COLOR);
    // must be called after getAlertBitmap
    toTitleCase(cur_alert.event);

    display.setFont(&FONT_14pt8b);
    if (getStringWidth(cur_alert.event) <= max_w)
    { // Fits on a single line, draw along bottom
      drawString(196 + 48 + 4, 24 + 8 - 12 + 20 + 1, cur_alert.event, LEFT);
    }
    else
    { // use smaller font
      display.setFont(&FONT_12pt8b);
      if (getStringWidth(cur_alert.event) <= max_w)
      { // Fits on a single line with smaller font, draw along bottom
        drawString(196 + 48 + 4, 24 + 8 - 12 + 17 + 1, cur_alert.event, LEFT);
      }
      else
      { // Does not fit on a single line, draw higher to allow room for 2nd line
        drawMultiLnString(196 + 48 + 4, 24 + 8 - 12 + 17 - 11,
                          cur_alert.event, LEFT, max_w, 2, 23);
      }
    }
  } // end 1 alert
  else
  { // 2 alerts
    // adjust max width to for 32x32 icons
    max_w -= 32;

    display.setFont(&FONT_12pt8b);
    for (int i = 0; i < 2; ++i)
    {
      owm_alerts_t &cur_alert = alerts[alert_indices[i]];

      display.drawInvertedBitmap(196, (i * 32), getAlertBitmap32(cur_alert),
                                 32, 32, ACCENT_COLOR);
      // must be called after getAlertBitmap
      toTitleCase(cur_alert.event);

      drawMultiLnString(196 + 32 + 3, 5 + 17 + (i * 32),
                        cur_alert.event, LEFT, max_w, 1, 0);
    } // end for-loop
  } // end 2 alerts

  free(ignore_list);
  free(alert_indices);

  return;
} // end drawAlerts

/* This function is responsible for drawing the city string and date
 * information in the top right corner.
 */
void drawLocationDate(const String &city, const String &date)
{
  // location, date
  display.setFont(&FONT_16pt8b);
  drawString(DISP_WIDTH - 2, 23, city, RIGHT, ACCENT_COLOR);
  display.setFont(&FONT_12pt8b);
  drawString(DISP_WIDTH - 2, 30 + 4 + 17, date, RIGHT);
  return;
} // end drawLocationDate

/* The % operator in C++ is not a true modulo operator but it instead a
 * remainder operator. The remainder operator and modulo operator are equivalent
 * for positive numbers, but not for negatives. The follow implementation of the
 * modulo operator works for +/-a and +b.
 */
inline int modulo(int a, int b)
{
  const int result = a % b;
  return result >= 0 ? result : result + b;
}

/* Convert temperature in kelvin to the display y coordinate to be plotted.
 */
int kelvin_to_plot_y(float kelvin, int tempBoundMin, float yPxPerUnit,
                     int yBoundMin)
{
#ifdef UNITS_TEMP_KELVIN
  return static_cast<int>(std::round(
    yBoundMin - (yPxPerUnit * (kelvin - tempBoundMin)) ));
#endif
#ifdef UNITS_TEMP_CELSIUS
  return static_cast<int>(std::round(
    yBoundMin - (yPxPerUnit * (kelvin_to_celsius(kelvin) - tempBoundMin)) ));
#endif
#ifdef UNITS_TEMP_FAHRENHEIT
  return static_cast<int>(std::round(
    yBoundMin - (yPxPerUnit * (kelvin_to_fahrenheit(kelvin) - tempBoundMin)) ));
#endif
}

/* This function is responsible for drawing the outlook graph for the specified
 * number of hours(up to 48).
 * hourlyLabels: pre-formatted hour labels from TimeCoordinator (e.g., "14h", "15h")
 * No timezone calculations here - all done by TimeCoordinator.
 */
void drawOutlookGraph(const owm_hourly_t *hourly, const owm_daily_t *daily,
                      const char hourlyLabels[][6], int startIndex)
{
  const int xPos0 = 350;
  int xPos1 = DISP_WIDTH;
  const int yPos0 = 216;
  const int yPos1 = DISP_HEIGHT - 46;

  // Limit to 24 hours from current hour index (but not exceeding available data)
  int endIndex = min(startIndex + 24, OWM_NUM_HOURLY);
  int numHours = endIndex - startIndex;
  

  // calculate y max/min and intervals using only future hours
  int yMajorTicks = 5;
#ifdef UNITS_TEMP_KELVIN
  float tempMin = hourly[startIndex].temp;
#endif
#ifdef UNITS_TEMP_CELSIUS
  float tempMin = kelvin_to_celsius(hourly[startIndex].temp);
#endif
#ifdef UNITS_TEMP_FAHRENHEIT
  float tempMin = kelvin_to_fahrenheit(hourly[startIndex].temp);
#endif
  float tempMax = tempMin;
#ifdef ENABLE_HOURLY_PRECIP_GRAPH
#ifdef UNITS_HOURLY_PRECIP_POP
  float precipMax = hourly[startIndex].pop;
#else
  float precipMax = hourly[startIndex].rain_1h + hourly[startIndex].snow_1h;
#endif
#endif
  int yTempMajorTicks = 5;
  float newTemp = 0;
  for (int i = startIndex + 1; i < endIndex; ++i)
  {
#ifdef UNITS_TEMP_KELVIN
    newTemp = hourly[i].temp;
#endif
#ifdef UNITS_TEMP_CELSIUS
    newTemp = kelvin_to_celsius(hourly[i].temp);
#endif
#ifdef UNITS_TEMP_FAHRENHEIT
    newTemp = kelvin_to_fahrenheit(hourly[i].temp);
#endif
    tempMin = std::min(tempMin, newTemp);
    tempMax = std::max(tempMax, newTemp);
#ifdef ENABLE_HOURLY_PRECIP_GRAPH
#ifdef UNITS_HOURLY_PRECIP_POP
    precipMax = std::max<float>(precipMax, hourly[i].pop);
#else
    precipMax = std::max<float>(
                precipMax, hourly[i].rain_1h + hourly[i].snow_1h);
#endif
#endif
  }
  int tempBoundMin = static_cast<int>(tempMin - 1)
                      - modulo(static_cast<int>(tempMin - 1), yTempMajorTicks);
  int tempBoundMax = static_cast<int>(tempMax + 1)
   + (yTempMajorTicks - modulo(static_cast<int>(tempMax + 1), yTempMajorTicks));

  // while we have to many major ticks then increase the step
  while ((tempBoundMax - tempBoundMin) / yTempMajorTicks > yMajorTicks)
  {
    yTempMajorTicks += 5;
    tempBoundMin = static_cast<int>(tempMin - 1)
                      - modulo(static_cast<int>(tempMin - 1), yTempMajorTicks);
    tempBoundMax = static_cast<int>(tempMax + 1) + (yTempMajorTicks
                      - modulo(static_cast<int>(tempMax + 1), yTempMajorTicks));
  }
  // while we have not enough major ticks, add to either bound
  while ((tempBoundMax - tempBoundMin) / yTempMajorTicks < yMajorTicks)
  {
    // add to whatever bound is closer to the actual min/max
    if (tempMin - tempBoundMin <= tempBoundMax - tempMax)
    {
      tempBoundMin -= yTempMajorTicks;
    }
    else
    {
      tempBoundMax += yTempMajorTicks;
    }
  }

#ifdef ENABLE_HOURLY_PRECIP_GRAPH
#ifdef UNITS_HOURLY_PRECIP_POP
  xPos1 = DISP_WIDTH - 23;
  // Always use 100 as max scale for POP (0-100%), even when no precipitation
  // This avoids division by zero and keeps the scale consistent
  float precipBoundMax = 100.0f;
#else
#ifdef UNITS_HOURLY_PRECIP_MILLIMETERS
  xPos1 = DISP_WIDTH - 24;
  // Ensure minimum of 1mm to avoid division by zero when no precipitation
  float precipBoundMax = std::max(1.0f, std::ceil(precipMax));
  int yPrecipMajorTickDecimals = (precipBoundMax < 10);
#endif
#ifdef UNITS_HOURLY_PRECIP_CENTIMETERS
  xPos1 = DISP_WIDTH - 25;
  precipMax = millimeters_to_centimeters(precipMax);
  // Round up to nearest 0.1 cm, with minimum of 0.1 to avoid division by zero
  float precipBoundMax = std::max(0.1f, std::ceil(precipMax * 10) / 10.0f);
  int yPrecipMajorTickDecimals;
  if (precipBoundMax < 1)
  {
    yPrecipMajorTickDecimals = 2;
    if (precipBoundMax > 0)
    {
      xPos1 -= 6; // needs extra room
    }
  }
  else if (precipBoundMax < 10)
  {
    yPrecipMajorTickDecimals = 1;
  }
  else
  {
    yPrecipMajorTickDecimals = 0;
  }
#endif
#ifdef UNITS_HOURLY_PRECIP_INCHES
  xPos1 = DISP_WIDTH - 25;
  precipMax = millimeters_to_inches(precipMax);
  // Round up to nearest 0.1 inch, with minimum of 0.1 to avoid division by zero
  float precipBoundMax = std::max(0.1f, std::ceil(precipMax * 10) / 10.0f);
  int yPrecipMajorTickDecimals;
  if (precipBoundMax < 1)
  {
    yPrecipMajorTickDecimals = 2;
  }
  else if (precipBoundMax < 10)
  {
    yPrecipMajorTickDecimals = 1;
  }
  else
  {
    yPrecipMajorTickDecimals = 0;
  }
#endif
  float yPrecipMajorTickValue = precipBoundMax / yMajorTicks;
  float precipRoundingMultiplier = std::pow(10.f, yPrecipMajorTickDecimals);
#endif

  if (precipBoundMax > 0)
  { // fill need extra room for labels
    xPos1 -= 23;
  }
#endif // ENABLE_HOURLY_PRECIP_GRAPH

  // draw x axis
  display.drawLine(xPos0, yPos1    , xPos1, yPos1    , GxEPD_BLACK);
  display.drawLine(xPos0, yPos1 - 1, xPos1, yPos1 - 1, GxEPD_BLACK);

  // draw y axis
  float yInterval = (yPos1 - yPos0) / static_cast<float>(yMajorTicks);
  for (int i = 0; i <= yMajorTicks; ++i)
  {
    String dataStr;
    int yTick = static_cast<int>(yPos0 + (i * yInterval));
    display.setFont(&FONT_8pt8b);
    // Temperature
    dataStr = String(tempBoundMax - (i * yTempMajorTicks));
#if defined(UNITS_TEMP_CELSIUS) || defined(UNITS_TEMP_FAHRENHEIT)
    dataStr += "\260";
#endif
    drawString(xPos0 - 8, yTick + 4, dataStr, RIGHT, ACCENT_COLOR);

#ifdef ENABLE_HOURLY_PRECIP_GRAPH
    if (precipBoundMax > 0)
    { // don't labels if precip is 0
#ifdef UNITS_HOURLY_PRECIP_POP
      // PoP
      dataStr = String(100 - (i * 20));
      String precipUnit = "%";
#else
      // Precipitation volume
      float precipTick = precipBoundMax - (i * yPrecipMajorTickValue);
      precipTick = std::round(precipTick * precipRoundingMultiplier)
                              / precipRoundingMultiplier;
      dataStr = String(precipTick, yPrecipMajorTickDecimals);
#ifdef UNITS_HOURLY_PRECIP_MILLIMETERS
      String precipUnit = String(" ") + TXT_UNITS_PRECIP_MILLIMETERS;
#endif
#ifdef UNITS_HOURLY_PRECIP_CENTIMETERS
      String precipUnit = String(" ") + TXT_UNITS_PRECIP_CENTIMETERS;
#endif
#ifdef UNITS_HOURLY_PRECIP_INCHES
      String precipUnit = String(" ") + TXT_UNITS_PRECIP_INCHES;
#endif
#endif

      drawString(xPos1 + 8, yTick + 4, dataStr, LEFT);
      display.setFont(&FONT_5pt8b);
      drawString(display.getCursorX(), yTick + 4, precipUnit, LEFT);
    } // end draw labels if precip is >0
#endif

    // draw dotted line
    if (i < yMajorTicks)
    {
      for (int x = xPos0; x <= xPos1 + 1; x += 3)
      {
        display.drawPixel(x, yTick + (yTick % 2), GxEPD_BLACK);
      }
    }
  }

  // int xMaxTicks = 12;  // Show 12 ticks (every 2 hours for 24h) - not used directly
  int hourInterval = 2;  // Every 2 hours
  float xInterval = (xPos1 - xPos0 - 1) / static_cast<float>(numHours);
  display.setFont(&FONT_8pt8b);

  // precalculate all x and y coordinates for temperature values
  float yPxPerUnit = (yPos1 - yPos0)
                     / static_cast<float>(tempBoundMax - tempBoundMin);
  std::vector<int> x_t;
  std::vector<int> y_t;
  x_t.reserve(numHours);
  y_t.reserve(numHours);
  for (int i = 0; i < numHours; ++i)
  {
    int hourlyIdx = startIndex + i;
    y_t[i] = kelvin_to_plot_y(hourly[hourlyIdx].temp, tempBoundMin, yPxPerUnit, yPos1);
    x_t[i] = static_cast<int>(std::round(xPos0 + (i * xInterval)
                                          + (0.5 * xInterval) ));
  }

#if DISPLAY_HOURLY_ICONS
  int day_idx = 0;
#endif
  display.setFont(&FONT_8pt8b);
  for (int i = 0; i < numHours; ++i)
  {
    int hourlyIdx = startIndex + i;
    int xTick = static_cast<int>(xPos0 + (i * xInterval));
    int x0_t, x1_t, y0_t, y1_t;

    if (i > 0)
    {
      // temperature
      x0_t = x_t[i - 1];
      x1_t = x_t[i    ];
      y0_t = y_t[i - 1];
      y1_t = y_t[i    ];
      // graph temperature
      display.drawLine(x0_t    , y0_t    , x1_t    , y1_t    , ACCENT_COLOR);
      display.drawLine(x0_t    , y0_t + 1, x1_t    , y1_t + 1, ACCENT_COLOR);
      display.drawLine(x0_t - 1, y0_t    , x1_t - 1, y1_t    , ACCENT_COLOR);

      // draw hourly bitmap
#if DISPLAY_HOURLY_ICONS
      if (daily[day_idx].dt + 86400 <= hourly[hourlyIdx].dt) {
        ++day_idx;
      }
      if ((i % hourInterval) == 0) // skip first and last tick
      {
        int y_b = INT_MAX;
        // find the highest (lowest in coordinate value) temperature point that
        // exists within the width of the icon.
        // find closest point above the temperature line where the icon won't
        // interect the temperature line.
        // y = mx + b
        int span = static_cast<int>(std::round(16 / xInterval));
        int l_idx = std::max(i - 1 - span, 0);
        int r_idx = std::min(i + span, numHours - 1);
        // left intersecting slope
        float m_l = (y_t[l_idx + 1] - y_t[l_idx]) / xInterval;
        int x_l = xTick - 16 - x_t[l_idx];
        int y_l = static_cast<int>(std::round(m_l * x_l + y_t[l_idx]));
        y_b = std::min(y_l, y_b);
        // right intersecting slope
        float m_r = (y_t[r_idx] - y_t[r_idx - 1]) / xInterval;
        int x_r = xTick + 16 - x_t[r_idx - 1];
        int y_r = static_cast<int>(std::round(m_r * x_r + y_t[r_idx - 1]));
        y_b = std::min(y_r, y_b);
        // any peaks in between
        for (int idx = l_idx + 1; idx < r_idx; ++idx)
        {
          y_b = std::min(y_t[idx], y_b);
        }
        const uint8_t *bitmap = getHourlyForecastBitmap32(hourly[hourlyIdx],
                                                          daily[day_idx]);
        display.drawInvertedBitmap(xTick - 16, y_b - 32,
                                   bitmap, 32, 32, GxEPD_BLACK);
      }
#endif
    }

    // Draw x tick and label every 2 hours - MOVED HERE to always draw regardless of precipitation
    if ((i % hourInterval) == 0)
    {
      int xTick = static_cast<int>(std::round(xPos0 + (i * xInterval)));
      // draw x tick marks
      display.drawLine(xTick    , yPos1 + 1, xTick    , yPos1 + 4, GxEPD_BLACK);
      display.drawLine(xTick + 1, yPos1 + 1, xTick + 1, yPos1 + 4, GxEPD_BLACK);
      // draw x axis labels - use pre-formatted labels from TimeCoordinator
      drawString(xTick, yPos1 + 1 + 12 + 4 + 3, hourlyLabels[hourlyIdx], CENTER);
    }

#ifdef ENABLE_HOURLY_PRECIP_GRAPH
#ifdef UNITS_HOURLY_PRECIP_POP
    float precipVal = hourly[hourlyIdx].pop * 100;
    if (i < 3) {
    }
#else
    float precipVal = hourly[hourlyIdx].rain_1h + hourly[hourlyIdx].snow_1h;
    if (i < 3) {
    }
#ifdef UNITS_HOURLY_PRECIP_CENTIMETERS
    precipVal = millimeters_to_centimeters(precipVal);
#endif
#ifdef UNITS_HOURLY_PRECIP_INCHES
    precipVal = millimeters_to_inches(precipVal);
#endif
#endif

    x0_t = static_cast<int>(std::round( xPos0 + 1 + (i * xInterval)));
    x1_t = static_cast<int>(std::round( xPos0 + 1 + ((i + 1) * xInterval) ));
    
    yPxPerUnit = (yPos1 - yPos0) / precipBoundMax;
    y0_t = static_cast<int>(std::round( yPos1 - (yPxPerUnit * (precipVal)) ));
    y1_t = yPos1;
#endif

    // graph Precipitation
#ifdef ENABLE_HOURLY_PRECIP_GRAPH
    // SAFETY FIRST: Ensure all coordinates are within valid range
    int draw_y0 = y0_t;
    int draw_y1 = y1_t;
    int draw_x0 = x0_t;
    int draw_x1 = x1_t;
    
    // Clamp to display bounds
    if (draw_y0 < 0) draw_y0 = 0;
    if (draw_y1 > DISP_HEIGHT) draw_y1 = DISP_HEIGHT;
    if (draw_x0 < 0) draw_x0 = 0;
    if (draw_x1 > DISP_WIDTH) draw_x1 = DISP_WIDTH;
    
    // Skip if no valid area
    if (draw_y0 >= draw_y1 || draw_x0 >= draw_x1) continue;
    
    // Draw dotted pattern bar
    // Option 1: Vertical lines every 3 pixels (tested, works but less aesthetic)
    // for (int y = draw_y0; y < draw_y1; y++) {
    //   for (int x = draw_x0; x < draw_x1; x++) {
    //     // if (x % 3 == 0) { // Draw a vertical line every 3 pixels
    //     // if ((x + y) % 4 == 0) {
    //     if (x % 2 == 0) {
    //       display.drawPixel(x, y, GxEPD_BLACK);
    //     }
    //   }
    // }
    
    // Option 2: 2x2 block checkerboard pattern (NOT WORKING - white screen)
    // Creates larger checkerboard blocks but causes white screen
    // Uncomment to test:
    // for (int y = draw_y0; y < draw_y1; y++) {
    //   for (int x = draw_x0; x < draw_x1; x++) {
    //     if (((x / 2) + (y / 2)) % 2 == 0) {
    //       display.drawPixel(x, y, GxEPD_BLACK);
    //     }
    //   }
    // }
    
    // Option 3: Diagonal cross pattern (NOT WORKING - white screen)
    // Creates diagonal lines but causes white screen
    // Note: Any pattern using 'y' in the condition fails
    // Uncomment to test:
    // for (int y = draw_y0; y < draw_y1; y++) {
    //   for (int x = draw_x0; x < draw_x1; x++) {
    //     if ((x + y) % 4 == 0 || (x - y) % 4 == 0) {
    //       display.drawPixel(x, y, GxEPD_BLACK);
    //     }
    //   }
    // }
    
    // Option 4: Staggered rows pattern (NOT WORKING - white screen)
    // Alternating offset dots in even/odd rows - fails due to y dependency
    // Uncomment to test:
    // for (int y = draw_y0; y < draw_y1; y++) {
    //   for (int x = draw_x0; x < draw_x1; x++) {
    //     if (y % 2 == 0) {
    //       if (x % 4 == 0) display.drawPixel(x, y, GxEPD_BLACK);
    //     } else {
    //       if ((x + 2) % 4 == 0) display.drawPixel(x, y, GxEPD_BLACK);
    //     }
    //   }
    // }
    
    // Option 5: Vertical stripe blocks (to test)
    // 2 pixels ON, 2 pixels OFF - creates vertical stripes
    // Uncomment to test:
    // for (int y = draw_y0; y < draw_y1; y++) {
    //   for (int x = draw_x0; x < draw_x1; x++) {
    //     if (x % 4 < 2) { // 2 pixels on, 2 pixels off
    //       display.drawPixel(x, y, GxEPD_BLACK);
    //     }
    //   }
    // }
    
    // Option 6: Two-pass checkerboard (to test)
    // Draw even rows first, then odd rows offset
    // This creates a checkerboard pattern in two separate passes
    // Uncomment to test:
    // // Etapa 1: Even rows (0, 2, 4...) with even columns
    // for (int y = draw_y0; y < draw_y1; y += 2) {
    //   for (int x = draw_x0; x < draw_x1; x++) {
    //     if (x % 2 == 0) display.drawPixel(x, y, GxEPD_BLACK);
    //   }
    // }
    // // Etapa 2: Odd rows (1, 3, 5...) with odd columns
    // for (int y = draw_y0 + 1; y < draw_y1; y += 2) {
    //   for (int x = draw_x0; x < draw_x1; x++) {
    //     if (x % 2 != 0) display.drawPixel(x, y, GxEPD_BLACK);
    //   }
    // }
    
    // Option 7: Dotted vertical lines (WORKS!)
    // Vertical lines with controlled gaps
    // Uncomment to switch between variants:
    
    // Original - 70% fill, gap of 3 lines:
    // for (int y = draw_y0; y < draw_y1; y++) {
    //   for (int x = draw_x0; x < draw_x1; x++) {
    //     if (x % 3 == 0 && y % 10 < 7) { 
    //       display.drawPixel(x, y, GxEPD_BLACK);
    //     }
    //   }
    // }
    
    // Alternative C - 75% fill, denser vertical lines (x%2): BEST SO FAR
    // Uncomment to use:
    // for (int y = draw_y0; y < draw_y1; y++) {
    //   for (int x = draw_x0; x < draw_x1; x++) {
    //     if (x % 2 == 0 && y % 4 < 3) { 
    //       display.drawPixel(x, y, GxEPD_BLACK);
    //     }
    //   }
    // }
    
    // Option 8: Diagonal dashed lines (BEST - aesthetic diagonal pattern)
    // 5px vertical dashes with lateral offset based on x position
    // Creates a nice diagonal/staggered pattern
    for (int y = draw_y0; y < draw_y1; y++) {
      for (int x = draw_x0; x < draw_x1; x++) {
        if (x % 3 == 0) {
          if ((y + (x * 2)) % 8 < 5) { // 5px dashes, staggered
            display.drawPixel(x, y, GxEPD_BLACK);
          }
        }
      }
    }
    
    // Alternative A - 75% fill, gap of 1 line (denser):
    // for (int y = draw_y0; y < draw_y1; y++) {
    //   for (int x = draw_x0; x < draw_x1; x++) {
    //     if (x % 3 == 0 && y % 4 < 3) { 
    //       display.drawPixel(x, y, GxEPD_BLACK);
    //     }
    //   }
    // }
    
    // Alternative B - 80% fill, gap of 1 line:
    // for (int y = draw_y0; y < draw_y1; y++) {
    //   for (int x = draw_x0; x < draw_x1; x++) {
    //     if (x % 3 == 0 && y % 5 < 4) { 
    //       display.drawPixel(x, y, GxEPD_BLACK);
    //     }
    //   }
    // }
    
    // Alternative C - 75% fill with denser vertical lines:
    // for (int y = draw_y0; y < draw_y1; y++) {
    //   for (int x = draw_x0; x < draw_x1; x++) {
    //     if (x % 2 == 0 && y % 4 < 3) { 
    //       display.drawPixel(x, y, GxEPD_BLACK);
    //     }
    //   }
    // }
#endif

  }

  // draw the last tick mark (end of graph)
  if ((numHours % hourInterval) == 0)
  {
    int xTick = static_cast<int>(
                std::round(xPos0 + (numHours * xInterval)));
    // draw x tick marks
    display.drawLine(xTick    , yPos1 + 1, xTick    , yPos1 + 4, GxEPD_BLACK);
    display.drawLine(xTick + 1, yPos1 + 1, xTick + 1, yPos1 + 4, GxEPD_BLACK);
    // draw x axis labels - last hour shown
    drawString(xTick, yPos1 + 1 + 12 + 4 + 3, hourlyLabels[endIndex - 1], CENTER);
  }

  return;
} // end drawOutlookGraph

/* This function is responsible for drawing the status bar along the bottom of
 * the display.
 */
void drawStatusBar(const String &statusStr, const String &refreshTimeStr,
                   int rssi, uint32_t batVoltage)
{
  String dataStr;
  uint16_t dataColor = GxEPD_BLACK;
  display.setFont(&FONT_6pt8b);
  int pos = DISP_WIDTH - 2;
  const int sp = 2;

#if BATTERY_MONITORING
  // battery - (expecting 3.7v LiPo)
  uint32_t batPercent = calcBatPercent(batVoltage,
                                       MIN_BATTERY_VOLTAGE,
                                       MAX_BATTERY_VOLTAGE);
#if defined(DISP_3C_B) || defined(DISP_7C_F)
  if (batVoltage < WARN_BATTERY_VOLTAGE)
  {
    dataColor = ACCENT_COLOR;
  }
#endif
#if STATUS_BAR_EXTRAS_BAT_PERCENTAGE || STATUS_BAR_EXTRAS_BAT_VOLTAGE
  dataStr = "";
#if STATUS_BAR_EXTRAS_BAT_PERCENTAGE
  dataStr += String(batPercent) + "%";
#endif
#if STATUS_BAR_EXTRAS_BAT_VOLTAGE
  dataStr += " (" + String( std::round(batVoltage / 10.f) / 100.f, 2 ) + "v)";
#endif
  drawString(pos, DISP_HEIGHT - 1 - 2, dataStr, RIGHT, dataColor);
  pos -= getStringWidth(dataStr) + 1;
#endif
  pos -= 24;
  display.drawInvertedBitmap(pos, DISP_HEIGHT - 1 - 17,
                             getBatBitmap24(batPercent), 24, 24, dataColor);
  pos -= sp + 9;
#endif

  // WiFi
  dataColor = rssi >= -70 ? GxEPD_BLACK : ACCENT_COLOR;
#if STATUS_BAR_EXTRAS_WIFI_STRENGTH || STATUS_BAR_EXTRAS_WIFI_RSSI
  dataStr = "";
#if STATUS_BAR_EXTRAS_WIFI_STRENGTH
  dataStr += String(getWiFidesc(rssi));
#endif
#if STATUS_BAR_EXTRAS_WIFI_RSSI
  if (rssi != 0)
  {
    dataStr += " (" + String(rssi) + "dBm)";
  }
#endif
  drawString(pos, DISP_HEIGHT - 1 - 2, dataStr, RIGHT, dataColor);
  pos -= getStringWidth(dataStr) + 1;
#endif
  pos -= 18;
  display.drawInvertedBitmap(pos, DISP_HEIGHT - 1 - 13, getWiFiBitmap16(rssi),
                             16, 16, dataColor);
  pos -= sp + 8;

  // last refresh
  dataColor = GxEPD_BLACK;
  drawString(pos, DISP_HEIGHT - 1 - 2, refreshTimeStr, RIGHT, dataColor);
  pos -= getStringWidth(refreshTimeStr) + 25;
  display.drawInvertedBitmap(pos, DISP_HEIGHT - 1 - 21, wi_refresh_32x32,
                             32, 32, dataColor);
  pos -= sp;

  // status
  dataColor = ACCENT_COLOR;
  if (!statusStr.isEmpty())
  {
    drawString(pos, DISP_HEIGHT - 1 - 2, statusStr, RIGHT, dataColor);
    pos -= getStringWidth(statusStr) + 24;
    display.drawInvertedBitmap(pos, DISP_HEIGHT - 1 - 18, error_icon_24x24,
                               24, 24, dataColor);
  }

  return;
} // end drawStatusBar

/* This function is responsible for drawing prominent error messages to the
 * screen.
 *
 * If error message line 2 (errMsgLn2) is empty, line 1 will be automatically
 * wrapped.
 */
void drawError(const uint8_t *bitmap_196x196,
               const String &errMsgLn1, const String &errMsgLn2)
{
  display.setFont(&FONT_26pt8b);
  if (!errMsgLn2.isEmpty())
  {
    drawString(DISP_WIDTH / 2,
               DISP_HEIGHT / 2 + 196 / 2 + 21,
               errMsgLn1, CENTER);
    drawString(DISP_WIDTH / 2,
               DISP_HEIGHT / 2 + 196 / 2 + 21 + 55,
               errMsgLn2, CENTER);
  }
  else
  {
    drawMultiLnString(DISP_WIDTH / 2,
                      DISP_HEIGHT / 2 + 196 / 2 + 21,
                      errMsgLn1, CENTER, DISP_WIDTH - 200, 2, 55);
  }
  display.drawInvertedBitmap(DISP_WIDTH / 2 - 196 / 2,
                             DISP_HEIGHT / 2 - 196 / 2 - 21,
                             bitmap_196x196, 196, 196, ACCENT_COLOR);
  return;
} // end drawError

/* Draws a loading screen with a 196x196 icon and message.
 * Similar to drawError but uses ACCENT_COLOR for the icon.
 */
void drawLoading(const uint8_t *bitmap_196x196,
                 const String &msgLn1, const String &msgLn2)
{
  display.setFont(&FONT_26pt8b);
  if (!msgLn2.isEmpty())
  {
    drawString(DISP_WIDTH / 2,
               DISP_HEIGHT / 2 + 196 / 2 + 21,
               msgLn1, CENTER);
    drawString(DISP_WIDTH / 2,
               DISP_HEIGHT / 2 + 196 / 2 + 21 + 55,
               msgLn2, CENTER);
  }
  else
  {
    drawMultiLnString(DISP_WIDTH / 2,
                      DISP_HEIGHT / 2 + 196 / 2 + 21,
                      msgLn1, CENTER, DISP_WIDTH - 200, 2, 55);
  }
  display.drawInvertedBitmap(DISP_WIDTH / 2 - 196 / 2,
                             DISP_HEIGHT / 2 - 196 / 2 - 21,
                             bitmap_196x196, 196, 196, GxEPD_BLACK);
  return;
} // end drawLoading

