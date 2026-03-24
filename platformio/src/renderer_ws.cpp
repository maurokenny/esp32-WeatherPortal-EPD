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

// Standard C libraries
#include <time.h>
#include <stdio.h>

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

/* Helper function to draw temperature with large font
 */
static void drawTemperature(int x, int y, float temp)
{
  char buf[16];
  sprintf(buf, "%.1f", temp);
  Paint_DrawString_EN(x, y, buf, &Font24, BLACK, WHITE);
  Paint_DrawString_EN(x + 80, y + 5, "C", &Font20, BLACK, WHITE);
}

/* Helper to get weather icon bitmap based on weather code
 */
static const uint8_t *getWeatherIcon196(int weatherId)
{
  // Map OWM weather IDs to icon bitmaps
  // This is a simplified mapping - expand as needed
  switch (weatherId / 100) {
    case 2: return wi_thunderstorm_196x196;  // Thunderstorm
    case 3: return wi_showers_196x196;       // Drizzle
    case 5: return wi_rain_196x196;          // Rain
    case 6: return wi_snow_196x196;          // Snow
    case 7: return wi_fog_196x196;           // Atmosphere (fog, mist)
    case 8: 
      if (weatherId == 800) return wi_day_sunny_196x196;  // Clear
      if (weatherId == 801) return wi_day_sunny_overcast_196x196;  // Few clouds
      if (weatherId == 802) return wi_day_cloudy_196x196;  // Scattered clouds
      return wi_cloudy_196x196;  // Broken/overcast clouds
    default: return wi_na_196x196;
  }
}

/* Draw current weather conditions
 */
void drawCurrentConditions(const owm_current_t &current,
                           const owm_daily_t &today,
                           const owm_resp_air_pollution_t &owm_air_pollution,
                           float inTemp, float inHumidity)
{
  (void)today;
  (void)owm_air_pollution;
  (void)inTemp;
  (void)inHumidity;
  
  // Debug: Print received values
  Serial.println("DEBUG drawCurrentConditions:");
  Serial.println("  current.temp: " + String(current.temp));
  Serial.println("  current.humidity: " + String(current.humidity));
  Serial.println("  current.pressure: " + String(current.pressure));
  Serial.println("  current.weather.id: " + String(current.weather.id));
  Serial.println("  current.weather.description: " + current.weather.description);
  
  // Clear display
  Paint_Clear(WHITE);
  
  // Draw city name at top
  Paint_DrawString_EN(10, 10, CITY_STRING.c_str(), &Font24, BLACK, WHITE);
  
  // Draw large temperature
  char tempStr[32];
  sprintf(tempStr, "%.1f", current.temp);
  Paint_DrawString_EN(10, 50, tempStr, &Font24, BLACK, WHITE);
  Paint_DrawString_EN(120, 55, "C", &Font20, BLACK, WHITE);
  
  // Draw feels like
  char feelsStr[48];
  sprintf(feelsStr, "Feels like %.1f C", current.feels_like);
  Paint_DrawString_EN(10, 90, feelsStr, &Font20, BLACK, WHITE);
  
  // Draw weather description
  Paint_DrawString_EN(10, 120, current.weather.description.c_str(), &Font20, BLACK, WHITE);
  
  // Draw weather icon on right side
  const uint8_t *icon = getWeatherIcon196(current.weather.id);
  Paint_DrawImage(icon, 500, 20, 196, 196);
  
  // Draw basic info
  char buf[64];
  sprintf(buf, "Humidity: %d%%", current.humidity);
  Paint_DrawString_EN(10, 160, buf, &Font16, BLACK, WHITE);
  
  sprintf(buf, "Wind: %.1f km/h", current.wind_speed);
  Paint_DrawString_EN(10, 180, buf, &Font16, BLACK, WHITE);
  
  sprintf(buf, "Pressure: %d hPa", current.pressure);
  Paint_DrawString_EN(10, 200, buf, &Font16, BLACK, WHITE);
  
  // Draw additional widgets
  drawCurrentSunrise(current);
  drawCurrentSunset(current);
  drawCurrentWind(current);
  drawCurrentHumidity(current);
  drawCurrentUVI(current);
  drawCurrentPressure(current);
  drawCurrentVisibility(current);
  drawCurrentDewpoint(current);
  
  refreshDisplay();
}

void drawForecast(const owm_daily_t *daily, tm timeInfo)
{
  (void)timeInfo;
  
  // Draw 7-day forecast at bottom
  int y = 350;
  Paint_DrawString_EN(10, y, "7-Day Forecast:", &Font20, BLACK, WHITE);
  
  for (int i = 0; i < 7 && i < OWM_NUM_DAILY; i++) {
    int x = 10 + (i * 110);
    char buf[32];
    
    // Day name (simplified - just show day number)
    sprintf(buf, "+%d", i + 1);
    Paint_DrawString_EN(x, y + 25, buf, &Font16, BLACK, WHITE);
    
    // High/Low temp
    sprintf(buf, "%.0f/%.0f", daily[i].temp.max, daily[i].temp.min);
    Paint_DrawString_EN(x, y + 45, buf, &Font16, BLACK, WHITE);
  }
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
  // Already drawn in drawCurrentConditions, but can add date here
  (void)city;
  Paint_DrawString_EN(600, 10, date.c_str(), &Font20, BLACK, WHITE);
}

void drawOutlookGraph(const owm_hourly_t *hourly, const owm_daily_t *daily,
                      tm timeInfo)
{
  (void)daily;
  (void)timeInfo;
  
  // Simple hourly temperature display
  int y = 400;
  Paint_DrawString_EN(10, y, "Next 24h:", &Font16, BLACK, WHITE);
  
  // Show temperatures for next 8 hours (every 3 hours)
  for (int i = 0; i < 8 && i < OWM_NUM_HOURLY; i += 3) {
    int x = 10 + (i / 3) * 95;
    char buf[32];
    
    // Hour
    time_t t = hourly[i].dt;
    struct tm *tm_info = localtime(&t);
    strftime(buf, sizeof(buf), "%Hh", tm_info);
    Paint_DrawString_EN(x, y + 20, buf, &Font12, BLACK, WHITE);
    
    // Temperature
    sprintf(buf, "%.0f", hourly[i].temp);
    Paint_DrawString_EN(x, y + 35, buf, &Font16, BLACK, WHITE);
  }
}

void drawStatusBar(const String &statusStr, const String &refreshTimeStr,
                   int rssi, uint32_t batVoltage)
{
  (void)statusStr;
  
  int y = 460;  // Bottom of screen
  
  // Draw refresh time
  Paint_DrawString_EN(10, y, refreshTimeStr.c_str(), &Font16, BLACK, WHITE);
  
  // Draw WiFi signal strength
  char buf[32];
  sprintf(buf, "WiFi: %d dBm", rssi);
  Paint_DrawString_EN(300, y, buf, &Font16, BLACK, WHITE);
  
  // Draw battery voltage
  sprintf(buf, "Bat: %.2fV", batVoltage / 1000.0);
  Paint_DrawString_EN(550, y, buf, &Font16, BLACK, WHITE);
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

void drawCurrentSunrise(const owm_current_t &current) { 
  char buf[32];
  time_t t = current.sunrise;
  struct tm *tm_info = localtime(&t);
  strftime(buf, sizeof(buf), "%H:%M", tm_info);
  Paint_DrawString_EN(10, 240, "Sunrise:", &Font16, BLACK, WHITE);
  Paint_DrawString_EN(10, 260, buf, &Font16, BLACK, WHITE);
}

void drawCurrentSunset(const owm_current_t &current) { 
  char buf[32];
  time_t t = current.sunset;
  struct tm *tm_info = localtime(&t);
  strftime(buf, sizeof(buf), "%H:%H", tm_info);
  Paint_DrawString_EN(120, 240, "Sunset:", &Font16, BLACK, WHITE);
  Paint_DrawString_EN(120, 260, buf, &Font16, BLACK, WHITE);
}

void drawCurrentInTemp(float inTemp) { 
  if (!isnan(inTemp)) {
    char buf[32];
    sprintf(buf, "In: %.1f C", inTemp);
    Paint_DrawString_EN(230, 240, buf, &Font16, BLACK, WHITE);
  }
}

void drawCurrentInHumidity(float inHumidity) { 
  if (!isnan(inHumidity)) {
    char buf[32];
    sprintf(buf, "In: %.0f%%", inHumidity);
    Paint_DrawString_EN(230, 260, buf, &Font16, BLACK, WHITE);
  }
}

void drawCurrentMoonrise(const owm_daily_t &today) { (void)today; }
void drawCurrentMoonset(const owm_daily_t &today) { (void)today; }

void drawCurrentWind(const owm_current_t &current) { 
  char buf[48];
  sprintf(buf, "Wind: %.1f km/h", current.wind_speed);
  Paint_DrawString_EN(10, 280, buf, &Font16, BLACK, WHITE);
}

void drawCurrentHumidity(const owm_current_t &current) { 
  char buf[32];
  sprintf(buf, "Humidity: %d%%", current.humidity);
  Paint_DrawString_EN(10, 300, buf, &Font16, BLACK, WHITE);
}

void drawCurrentUVI(const owm_current_t &current) { 
  char buf[32];
  sprintf(buf, "UV Index: %.1f", current.uvi);
  Paint_DrawString_EN(10, 320, buf, &Font16, BLACK, WHITE);
}

void drawCurrentPressure(const owm_current_t &current) { 
  char buf[32];
  sprintf(buf, "Pressure: %d hPa", current.pressure);
  Paint_DrawString_EN(10, 340, buf, &Font16, BLACK, WHITE);
}

void drawCurrentVisibility(const owm_current_t &current) { 
  char buf[48];
  sprintf(buf, "Visibility: %.1f km", current.visibility / 1000.0);
  Paint_DrawString_EN(10, 360, buf, &Font16, BLACK, WHITE);
}

void drawCurrentAirQuality(const owm_resp_air_pollution_t &owm_air_pollution) { 
  (void)owm_air_pollution;
  // Not implemented for Open-Meteo
}

void drawCurrentMoonphase(const owm_daily_t &today) { (void)today; }

void drawCurrentDewpoint(const owm_current_t &current) { 
  char buf[32];
  sprintf(buf, "Dew Point: %.1f C", current.dew_point);
  Paint_DrawString_EN(350, 240, buf, &Font16, BLACK, WHITE);
}
