/* Renderer declarations for esp32-weather-epd using Waveshare library.
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

#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <vector>
#include <Arduino.h>
#include <time.h>
#include "api_response.h"
#include "config.h"

// Waveshare library includes
#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"

#define DISP_WIDTH  800
#define DISP_HEIGHT 480

#define GxEPD_BLACK 0
#define GxEPD_WHITE 1

// Compatibility class for GxEPD2 API
class DisplayClass {
public:
  bool nextPage();
  void firstPage();
  void setFullWindow();
  void setTextColor(uint16_t color);
  void setTextSize(uint8_t size);
  void setTextWrap(bool wrap);
  void setCursor(int16_t x, int16_t y);
  int16_t getCursorX();
};

extern DisplayClass display;

typedef enum alignment
{
  LEFT,
  RIGHT,
  CENTER
} alignment_t;

uint16_t getStringWidth(const String &text);
uint16_t getStringHeight(const String &text);
void drawString(int16_t x, int16_t y, const String &text, alignment_t alignment,
                uint16_t color=GxEPD_BLACK);
void drawMultiLnString(int16_t x, int16_t y, const String &text,
                       alignment_t alignment, uint16_t max_width,
                       uint16_t max_lines, int16_t line_spacing,
                       uint16_t color=GxEPD_BLACK);
void initDisplay();
void refreshDisplay();
void powerOffDisplay();
void drawCurrentConditions(const owm_current_t &current,
                           const owm_daily_t &today,
                           const owm_resp_air_pollution_t &owm_air_pollution,
                           float inTemp, float inHumidity);
void drawForecast(const owm_daily_t *daily, tm timeInfo);
void drawAlerts(std::vector<owm_alerts_t> &alerts,
                const String &city, const String &date);
void drawLocationDate(const String &city, const String &date);
void drawOutlookGraph(const owm_hourly_t *hourly, const owm_daily_t *daily,
                      tm timeInfo);
void drawStatusBar(const String &statusStr, const String &refreshTimeStr,
                   int rssi, uint32_t batVoltage);
void drawError(const uint8_t *bitmap_196x196,
               const String &errMsgLn1, const String &errMsgLn2="");
void drawCurrentSunrise(const owm_current_t &current);
void drawCurrentSunset(const owm_current_t &current);
void drawCurrentInTemp(float inTemp);
void drawCurrentInHumidity(float inHumidity);
void drawCurrentMoonrise(const owm_daily_t &today);
void drawCurrentMoonset(const owm_daily_t &today);
void drawCurrentWind(const owm_current_t &current);
void drawCurrentHumidity(const owm_current_t &current);
void drawCurrentUVI(const owm_current_t &current);
void drawCurrentPressure(const owm_current_t &current);
void drawCurrentVisibility(const owm_current_t &current);
void drawCurrentAirQuality(const owm_resp_air_pollution_t &owm_air_pollution);
void drawCurrentMoonphase(const owm_daily_t &today);
void drawCurrentDewpoint(const owm_current_t &current);

#endif
