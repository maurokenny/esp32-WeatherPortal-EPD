/* Renderer for esp32-weather-epd using Waveshare library.
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

#include "renderer.h"
#include "api_response.h"
#include "config.h"
#include "conversions.h"
#include "display_utils.h"
#include "_locale.h"
#include "_strftime.h"

// Include Adafruit_GFX for font definitions
#include <Adafruit_GFX.h>

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

// Waveshare library
#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"

// Display buffer (800x480 / 8 = 48000 bytes)
static UBYTE *imageBuffer = NULL;
static bool displayInitialized = false;

// Global display object for compatibility
DisplayClass display;

/* nextPage() - compatibility with GxEPD2 API
 * This version just refreshes the display once and returns false
 */
bool DisplayClass::nextPage() {
  if (!displayInitialized || imageBuffer == NULL) {
    return false;
  }
  
  // Send buffer to display
  EPD_7IN5_V2_Display(imageBuffer);
  
  // Return false to exit the do-while loop immediately
  return false;
}

/* Initialize display
 */
void initDisplay()
{
  if (displayInitialized) {
    return;
  }
  
  Serial.println("initDisplay: Starting...");
  
  // Initialize module
  DEV_Module_Init();
  Serial.println("initDisplay: DEV_Module_Init done");
  
  // Initialize display
  EPD_7IN5_V2_Init();
  Serial.println("initDisplay: EPD_7IN5_V2_Init done");
  
  EPD_7IN5_V2_Clear();
  Serial.println("initDisplay: EPD_7IN5_V2_Clear done");
  
  DEV_Delay_ms(500);
  
  // Allocate image buffer
  UWORD imagesize = ((EPD_7IN5_V2_WIDTH % 8 == 0) ? 
                     (EPD_7IN5_V2_WIDTH / 8) : 
                     (EPD_7IN5_V2_WIDTH / 8 + 1)) * EPD_7IN5_V2_HEIGHT;
  
  Serial.print("initDisplay: Buffer size = ");
  Serial.println(imagesize);
  
  if (imageBuffer == NULL) {
    imageBuffer = (UBYTE *)malloc(imagesize);
    if (imageBuffer == NULL) {
      Serial.println("Failed to allocate display buffer!");
      return;
    }
    Serial.println("initDisplay: Buffer allocated");
  }
  
  // Create new image
  Paint_NewImage(imageBuffer, EPD_7IN5_V2_WIDTH, EPD_7IN5_V2_HEIGHT, 0, WHITE);
  Serial.println("initDisplay: Paint_NewImage done");
  
  Paint_SelectImage(imageBuffer);
  Serial.println("initDisplay: Paint_SelectImage done");
  
  Paint_Clear(WHITE);
  Serial.println("initDisplay: Paint_Clear done");
  
  displayInitialized = true;
  Serial.println("initDisplay: Complete!");
  return;
}

/* Power-off e-paper display
 */
void powerOffDisplay()
{
  if (!displayInitialized) {
    return;
  }
  
  EPD_7IN5_V2_Sleep();
  return;
}

/* Refresh display - send buffer to screen
 */
static void refreshDisplay()
{
  if (!displayInitialized || imageBuffer == NULL) {
    return;
  }
  EPD_7IN5_V2_Display(imageBuffer);
}

/* Returns the string width in pixels
 */
uint16_t getStringWidth(const String &text)
{
  // Approximate width based on font
  return text.length() * Font20.Width;
}

/* Returns the string height in pixels
 */
uint16_t getStringHeight(const String &text)
{
  (void)text;
  return Font20.Height;
}

/* Draws a string with alignment
 */
void drawString(int16_t x, int16_t y, const String &text, alignment_t alignment,
                uint16_t color)
{
  if (!displayInitialized) {
    return;
  }
  
  uint16_t w = getStringWidth(text);
  
  switch (alignment) {
    case RIGHT:
      x -= w;
      break;
    case CENTER:
      x -= w / 2;
      break;
    case LEFT:
    default:
      break;
  }
  
  UWORD fgColor = (color == GxEPD_BLACK) ? BLACK : WHITE;
  UWORD bgColor = (color == GxEPD_BLACK) ? WHITE : BLACK;
  
  Paint_DrawString_EN(x, y, text.c_str(), &Font20, fgColor, bgColor);
}

/* Draw multi-line string
 */
void drawMultiLnString(int16_t x, int16_t y, const String &text,
                       alignment_t alignment, uint16_t max_width,
                       uint16_t max_lines, int16_t line_spacing,
                       uint16_t color)
{
  (void)max_width;
  (void)max_lines;
  (void)line_spacing;
  drawString(x, y, text, alignment, color);
}

/* Stub functions for compatibility
 */
void drawCurrentConditions(const owm_current_t &current,
                           const owm_daily_t &today,
                           const owm_resp_air_pollution_t &owm_air_pollution,
                           float inTemp, float inHumidity)
{
  (void)current;
  (void)today;
  (void)owm_air_pollution;
  (void)inTemp;
  (void)inHumidity;
  
  Paint_Clear(WHITE);
  Paint_DrawString_EN(10, 10, "Weather Display", &Font24, BLACK, WHITE);
  Paint_DrawString_EN(10, 50, "Waveshare Driver OK!", &Font20, BLACK, WHITE);
  refreshDisplay();
}

void drawForecast(const owm_daily_t *daily, tm timeInfo)
{
  (void)daily;
  (void)timeInfo;
}

void drawAlerts(std::vector<owm_alerts_t> &alerts,
                const String &city, const String &date)
{
  (void)alerts;
  (void)city;
  (void)date;
}

void drawLocationDate(const String &city, const String &date)
{
  (void)city;
  (void)date;
}

void drawOutlookGraph(const owm_hourly_t *hourly, const owm_daily_t *daily,
                      tm timeInfo)
{
  (void)hourly;
  (void)daily;
  (void)timeInfo;
}

void drawStatusBar(const String &statusStr, const String &refreshTimeStr,
                   int rssi, uint32_t batVoltage)
{
  (void)statusStr;
  (void)refreshTimeStr;
  (void)rssi;
  (void)batVoltage;
}

void drawError(const uint8_t *bitmap_196x196,
               const String &errMsgLn1, const String &errMsgLn2)
{
  (void)bitmap_196x196;
  
  if (!displayInitialized) {
    return;
  }
  
  Paint_Clear(WHITE);
  Paint_DrawString_EN(10, 10, "ERROR:", &Font24, BLACK, WHITE);
  Paint_DrawString_EN(10, 50, errMsgLn1.c_str(), &Font20, BLACK, WHITE);
  if (errMsgLn2.length() > 0) {
    Paint_DrawString_EN(10, 80, errMsgLn2.c_str(), &Font20, BLACK, WHITE);
  }
  refreshDisplay();
}

void drawCurrentSunrise(const owm_current_t &current) { (void)current; }
void drawCurrentSunset(const owm_current_t &current) { (void)current; }
void drawCurrentInTemp(float inTemp) { (void)inTemp; }
void drawCurrentInHumidity(float inHumidity) { (void)inHumidity; }
void drawCurrentMoonrise(const owm_daily_t &today) { (void)today; }
void drawCurrentMoonset(const owm_daily_t &today) { (void)today; }
void drawCurrentWind(const owm_current_t &current) { (void)current; }
void drawCurrentHumidity(const owm_current_t &current) { (void)current; }
void drawCurrentUVI(const owm_current_t &current) { (void)current; }
void drawCurrentPressure(const owm_current_t &current) { (void)current; }
void drawCurrentVisibility(const owm_current_t &current) { (void)current; }
void drawCurrentAirQuality(const owm_resp_air_pollution_t &owm_air_pollution) { (void)owm_air_pollution; }
void drawCurrentMoonphase(const owm_daily_t &today) { (void)today; }
void drawCurrentDewpoint(const owm_current_t &current) { (void)current; }
