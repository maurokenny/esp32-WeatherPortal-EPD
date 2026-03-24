/* Renderer with original esp32-weather-epd layout */
/* BUGFIX VERSION: Fixed bitmap rendering, font sizes, positioning */

#include "renderer.h"
#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include <time.h>
#include <stdio.h>
#include <math.h>

// Icon headers
#include "icons/icons_196x196.h"
#include "icons/icons_64x64.h"
#include "icons/icons_48x48.h"
#include "icons/icons_32x32.h"
#include "icons/icons_24x24.h"
#include "icons/icons_128x128.h"

// Display buffer
static UBYTE *imageBuffer = NULL;
static bool displayInitialized = false;

// LARGER Font mapping for better readability on 7.5" display
#define FONT_7PT    Font8
#define FONT_8PT    Font12   // Increased from Font8
#define FONT_11PT   Font12
#define FONT_12PT   Font16   // Increased from Font12
#define FONT_14PT   Font20   // Increased from Font16
#define FONT_16PT   Font24   // Increased from Font16
#define FONT_24PT   Font24

// Widget positions (2 columns x 5 rows)
#define WIDGET_COL_WIDTH  162
#define WIDGET_ROW_HEIGHT 58   // Increased slightly
#define WIDGET_BASE_Y     206
#define WIDGET_ICON_SIZE  48

// Graph area constants
#define GRAPH_X      350
#define GRAPH_Y      220   // Moved down slightly
#define GRAPH_W      430
#define GRAPH_H      175   // Slightly smaller
#define GRAPH_BOTTOM (GRAPH_Y + GRAPH_H)

// Header positions
#define HEADER_CITY_Y    28
#define HEADER_DATE_Y    56
#define HEADER_RIGHT_X   785  // Margin from right edge

// Status bar positions
#define STATUS_BAR_Y     452  // Higher up to avoid overlap

// Temperature display
#define TEMP_X           275
#define TEMP_Y_POS       95
#define TEMP_UNIT_X      335

// Display object
DisplayClass display;

/* nextPage() - GxEPD2 compatibility */
bool DisplayClass::nextPage() {
  if (!displayInitialized || imageBuffer == NULL) return false;
  EPD_7IN5_V2_Display(imageBuffer);
  return false;
}

void DisplayClass::firstPage() {
  Paint_Clear(WHITE);
}

void DisplayClass::setFullWindow() {}
void DisplayClass::setTextColor(uint16_t color) { (void)color; }
void DisplayClass::setTextSize(uint8_t size) { (void)size; }
void DisplayClass::setTextWrap(bool wrap) { (void)wrap; }
void DisplayClass::setCursor(int16_t x, int16_t y) { (void)x; (void)y; }
int16_t DisplayClass::getCursorX() { return 0; }

/* Initialize display */
void initDisplay() {
  if (displayInitialized) return;
  
  DEV_Module_Init();
  EPD_7IN5_V2_Init();
  EPD_7IN5_V2_Clear();
  DEV_Delay_ms(500);
  
  UWORD Imagesize = ((EPD_7IN5_V2_WIDTH % 8 == 0) ? 
                     (EPD_7IN5_V2_WIDTH / 8) : 
                     (EPD_7IN5_V2_WIDTH / 8 + 1)) * EPD_7IN5_V2_HEIGHT;
  
  if ((imageBuffer = (UBYTE *)malloc(Imagesize)) == NULL) {
    Serial.println("Failed to allocate display buffer!");
    return;
  }
  
  Paint_NewImage(imageBuffer, EPD_7IN5_V2_WIDTH, EPD_7IN5_V2_HEIGHT, 0, WHITE);
  Paint_Clear(WHITE);
  
  displayInitialized = true;
}

/* Refresh display */
void refreshDisplay() {
  if (displayInitialized && imageBuffer != NULL) {
    EPD_7IN5_V2_Display(imageBuffer);
  }
}

/* Power off display */
void powerOffDisplay() {
  if (displayInitialized) {
    EPD_7IN5_V2_Sleep();
  }
}

/* Draw string with alignment helper - FIXED for better positioning */
static void drawString(int x, int y, const char* text, sFONT* font, alignment_t align) {
  if (text == NULL || *text == '\0') return;
  
  int textWidth = strlen(text) * font->Width;
  int drawX = x;
  
  if (align == RIGHT) {
    drawX = x - textWidth;
  } else if (align == CENTER) {
    drawX = x - textWidth / 2;
  }
  
  // Bounds checking
  if (drawX < 0) drawX = 0;
  if (drawX + textWidth > DISP_WIDTH) drawX = DISP_WIDTH - textWidth;
  if (y < 0) y = 0;
  if (y + font->Height > DISP_HEIGHT) y = DISP_HEIGHT - font->Height;
  
  Paint_DrawString_EN(drawX, y, text, font, BLACK, WHITE);
}

/* SAFE: Draw point with bounds checking */
static void safeDrawPoint(int x, int y, DOT_PIXEL pixel, DOT_STYLE style) {
  if (x >= 0 && x < DISP_WIDTH && y >= 0 && y < DISP_HEIGHT) {
    Paint_DrawPoint(x, y, BLACK, pixel, style);
  }
}

/* FIXED: Draw bitmap with correct colors (inverted) and bounds checking */
static void drawBitmap(int x, int y, const uint8_t* bitmap, int w, int h) {
  if (bitmap == NULL) {
    // Placeholder rectangle
    Paint_DrawRectangle(x, y, x + w - 1, y + h - 1, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    return;
  }
  
  // Manual 1-bit bitmap rendering - INVERTED (bit=0 means draw BLACK pixel)
  // The icons in the library have white icon on black background
  // We want: icon=BLACK on white background
  int rowBytes = (w + 7) / 8;  // Bytes per row (padded)
  
  for (int row = 0; row < h; row++) {
    for (int col = 0; col < w; col++) {
      int byteIndex = row * rowBytes + (col / 8);
      int bitIndex = 7 - (col % 8);  // MSB first
      
      if (byteIndex < (w * h / 8 + h)) {  // Bounds check
        // INVERTED: Draw when bit is 0 (icon is white in source, becomes black)
        if (!(bitmap[byteIndex] & (1 << bitIndex))) {
          safeDrawPoint(x + col, y + row, DOT_PIXEL_1X1, DOT_FILL_AROUND);
        }
      }
    }
  }
}

/* Get weather icon 196x196 based on WMO code */
static const uint8_t* getWeatherIcon196(int wmoCode, bool isDay) {
  if (wmoCode == 0) {
    return isDay ? wi_day_sunny_196x196 : wi_night_clear_196x196;
  } else if (wmoCode <= 2) {
    return isDay ? wi_day_cloudy_196x196 : wi_night_cloudy_196x196;
  } else if (wmoCode == 3) {
    return wi_cloudy_196x196;
  } else if (wmoCode <= 48) {
    return isDay ? wi_day_fog_196x196 : wi_night_fog_196x196;
  } else if (wmoCode <= 67) {
    return isDay ? wi_day_rain_196x196 : wi_night_rain_196x196;
  } else if (wmoCode <= 77) {
    return isDay ? wi_day_snow_196x196 : wi_night_snow_196x196;
  } else if (wmoCode <= 82) {
    return isDay ? wi_day_showers_196x196 : wi_night_showers_196x196;
  } else if (wmoCode >= 95) {
    return isDay ? wi_day_thunderstorm_196x196 : wi_night_thunderstorm_196x196;
  }
  return wi_na_196x196;
}

/* Get weather icon 64x64 based on WMO code */
static const uint8_t* getWeatherIcon64(int wmoCode, bool isDay) {
  if (wmoCode == 0) {
    return isDay ? wi_day_sunny_64x64 : wi_night_clear_64x64;
  } else if (wmoCode <= 2) {
    return isDay ? wi_day_cloudy_64x64 : wi_night_cloudy_64x64;
  } else if (wmoCode == 3) {
    return wi_cloudy_64x64;
  } else if (wmoCode <= 48) {
    return isDay ? wi_day_fog_64x64 : wi_night_fog_64x64;
  } else if (wmoCode <= 67) {
    return isDay ? wi_day_rain_64x64 : wi_night_rain_64x64;
  } else if (wmoCode <= 77) {
    return isDay ? wi_day_snow_64x64 : wi_night_snow_64x64;
  } else if (wmoCode <= 82) {
    return isDay ? wi_day_showers_64x64 : wi_night_showers_64x64;
  } else if (wmoCode >= 95) {
    return isDay ? wi_day_thunderstorm_64x64 : wi_night_thunderstorm_64x64;
  }
  return wi_na_64x64;
}

/* Get widget icon 48x48 */
static const uint8_t* getWidgetIcon(int widgetType) {
  switch (widgetType) {
    case 0: return wi_sunrise_48x48;
    case 1: return wi_sunset_48x48;
    case 2: return wi_humidity_48x48;
    case 3: return wi_windy_48x48;
    case 4: return wi_barometer_48x48;
    case 5: return wi_thermometer_48x48;
    case 6: return wi_day_sunny_48x48;
    case 7: return wi_moon_full_48x48;
    case 8: return wi_raindrop_48x48;
    case 9: return wi_cloudy_48x48;
    default: return wi_na_48x48;
  }
}

/* Get battery icon based on voltage */
static const uint8_t* getBatteryIcon(uint32_t voltage) {
  if (voltage == UINT32_MAX) return battery_0_bar_0deg_32x32;
  if (voltage >= 4100) return battery_full_0deg_32x32;
  if (voltage >= 3900) return battery_6_bar_0deg_32x32;
  if (voltage >= 3800) return battery_5_bar_0deg_32x32;
  if (voltage >= 3700) return battery_4_bar_0deg_32x32;
  if (voltage >= 3600) return battery_3_bar_0deg_32x32;
  if (voltage >= 3500) return battery_2_bar_0deg_32x32;
  if (voltage >= 3400) return battery_1_bar_0deg_32x32;
  return battery_0_bar_0deg_32x32;
}

/* Get WiFi signal icon based on RSSI */
static const uint8_t* getWiFiIcon(int rssi) {
  if (rssi > -50) return wifi_32x32;
  if (rssi > -65) return wifi_3_bar_32x32;
  if (rssi > -75) return wifi_2_bar_32x32;
  if (rssi > -85) return wifi_1_bar_32x32;
  return wifi_off_32x32;
}

/* Get wind direction icon based on degrees */
static const uint8_t* getWindDirectionIcon(int degrees) {
  int index = ((degrees + 11) % 360) / 23;
  switch (index) {
    case 0: return wi_direction_up_48x48;
    case 1: return wi_direction_up_right_48x48;
    case 2: return wi_direction_up_right_48x48;
    case 3: return wi_direction_right_48x48;
    case 4: return wi_direction_right_48x48;
    case 5: return wi_direction_down_right_48x48;
    case 6: return wi_direction_down_right_48x48;
    case 7: return wi_direction_down_48x48;
    case 8: return wi_direction_down_48x48;
    case 9: return wi_direction_down_left_48x48;
    case 10: return wi_direction_down_left_48x48;
    case 11: return wi_direction_left_48x48;
    case 12: return wi_direction_left_48x48;
    case 13: return wi_direction_up_left_48x48;
    case 14: return wi_direction_up_left_48x48;
    case 15: return wi_direction_up_48x48;
    default: return wi_direction_up_48x48;
  }
}

/* Convert degrees to cardinal direction */
static const char* degreesToCardinal(int degrees) {
  const char* directions[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                              "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
  int index = ((degrees + 11) % 360) / 23;
  return directions[index];
}

/* Calculate moon phase (0.0 = new, 0.5 = full, 1.0 = new) */
static float calculateMoonPhase(int year, int month, int day) {
  if (month < 3) {
    year--;
    month += 12;
  }
  
  double c = 365.25 * year;
  double e = 30.6 * (month + 1);
  double jd = c + e + day - 694039.09;
  jd /= 29.53059;
  double phase = jd - floor(jd);
  
  return (float)phase;
}

/* Get moon phase icon and name */
static const uint8_t* getMoonPhaseIcon(float phase, const char** name) {
  if (phase < 0.0625 || phase >= 0.9375) {
    *name = "New";
    return wi_moon_new_48x48;
  } else if (phase < 0.1875) {
    *name = "Wax Cresc";
    return wi_moon_waxing_crescent_3_48x48;
  } else if (phase < 0.3125) {
    *name = "1st Quart";
    return wi_moon_first_quarter_48x48;
  } else if (phase < 0.4375) {
    *name = "Wax Gibb";
    return wi_moon_waxing_gibbous_4_48x48;
  } else if (phase < 0.5625) {
    *name = "Full";
    return wi_moon_full_48x48;
  } else if (phase < 0.6875) {
    *name = "Wan Gibb";
    return wi_moon_waning_gibbous_4_48x48;
  } else if (phase < 0.8125) {
    *name = "Last Quart";
    return wi_moon_third_quarter_48x48;
  } else {
    *name = "Wan Cresc";
    return wi_moon_waning_crescent_3_48x48;
  }
}

/* Format time from timestamp */
static void formatTime(char* buf, size_t size, int64_t timestamp) {
  time_t t = (time_t)timestamp;
  struct tm *tm_info = localtime(&t);
  strftime(buf, size, "%H:%M", tm_info);
}

/* Draw a single widget with LARGER fonts */
static void drawWidget(int pos, const char* label, const char* value, const uint8_t* icon) {
  int col = pos % 2;
  int row = pos / 2;
  int x = WIDGET_COL_WIDTH * col + 2;  // Small left margin
  int y = WIDGET_BASE_Y + (WIDGET_ROW_HEIGHT * row);
  
  drawBitmap(x, y, icon, 48, 48);
  drawString(x + 54, y + 6, label, &FONT_8PT, LEFT);    // Increased from FONT_7PT
  drawString(x + 54, y + 26, value, &FONT_12PT, LEFT);  // Increased from FONT_12PT
}

/* BUGFIX: drawCurrentConditions - Larger fonts, better spacing */
void drawCurrentConditions(const owm_current_t &current,
                           const owm_daily_t &today,
                           const owm_resp_air_pollution_t &owm_air_pollution,
                           float inTemp, float inHumidity) {
  (void)owm_air_pollution;
  (void)inTemp;
  (void)inHumidity;
  
  char buf[64];
  bool isDay = (current.dt > today.sunrise && current.dt < today.sunset);
  
  // ==== TOP LEFT: Weather Icon (196x196) ====
  const uint8_t* weatherIcon = getWeatherIcon196(current.weather.id, isDay);
  drawBitmap(2, 2, weatherIcon, 196, 196);
  
  // ==== TEMPERATURE - LARGER ====
  snprintf(buf, sizeof(buf), "%.0f", current.temp);
  drawString(TEMP_X, TEMP_Y_POS, buf, &FONT_24PT, CENTER);  // Larger font
  drawString(TEMP_UNIT_X, TEMP_Y_POS + 5, "C", &FONT_14PT, LEFT);
  
  // ==== FEELS LIKE ====
  snprintf(buf, sizeof(buf), "Feels like %.0fC", current.feels_like);
  drawString(TEMP_X, TEMP_Y_POS + 55, buf, &FONT_12PT, CENTER);
  
  // ==== WIDGET GRID (2x5 = 10 widgets) ====
  
  // Widget 0: Sunrise
  formatTime(buf, sizeof(buf), current.sunrise);
  drawWidget(0, "Sunrise", buf, getWidgetIcon(0));
  
  // Widget 1: Sunset
  formatTime(buf, sizeof(buf), current.sunset);
  drawWidget(1, "Sunset", buf, getWidgetIcon(1));
  
  // Widget 2: Humidity
  snprintf(buf, sizeof(buf), "%d%%", current.humidity);
  drawWidget(2, "Humidity", buf, getWidgetIcon(2));
  
  // Widget 3: Wind with direction
  if (current.wind_deg > 0) {
    snprintf(buf, sizeof(buf), "%.0f km/h %s", current.wind_speed * 3.6f, 
             degreesToCardinal(current.wind_deg));
  } else {
    snprintf(buf, sizeof(buf), "%.0f km/h", current.wind_speed * 3.6f);
  }
  drawWidget(3, "Wind", buf, 
             current.wind_deg > 0 ? getWindDirectionIcon(current.wind_deg) : getWidgetIcon(3));
  
  // Widget 4: Pressure
  snprintf(buf, sizeof(buf), "%d hPa", current.pressure);
  drawWidget(4, "Pressure", buf, getWidgetIcon(4));
  
  // Widget 5: UV Index
  snprintf(buf, sizeof(buf), "%.1f", current.uvi);
  drawWidget(5, "UV Index", buf, getWidgetIcon(5));
  
  // Widget 6: Visibility
  if (current.visibility > 0) {
    snprintf(buf, sizeof(buf), "%.1f km", current.visibility / 1000.0f);
  } else {
    strcpy(buf, "--");
  }
  drawWidget(6, "Visibility", buf, getWidgetIcon(6));
  
  // Widget 7: Moon Phase
  time_t today_t = (time_t)today.dt;
  struct tm *today_tm = localtime(&today_t);
  float moonPhase = calculateMoonPhase(today_tm->tm_year + 1900, 
                                        today_tm->tm_mon + 1, 
                                        today_tm->tm_mday);
  const char* moonName;
  const uint8_t* moonIcon = getMoonPhaseIcon(moonPhase, &moonName);
  snprintf(buf, sizeof(buf), "%s", moonName);
  drawWidget(7, "Moon", buf, moonIcon);
  
  // Widget 8: Dew Point
  snprintf(buf, sizeof(buf), "%.0fC", current.dew_point);
  drawWidget(8, "Dew Point", buf, getWidgetIcon(8));
  
  // Widget 9: Cloud Cover
  snprintf(buf, sizeof(buf), "%d%%", current.clouds);
  drawWidget(9, "Clouds", buf, getWidgetIcon(9));
}

/* BUGFIX: drawForecast - Fixed day calculation, moved down */
void drawForecast(const owm_daily_t *daily, tm timeInfo) {
  (void)timeInfo;
  char buf[32];
  
  // MOVED DOWN: icons at y=85 (was 68), temps at y=155 (was 138)
  const int ICON_Y = 85;
  const int FORECAST_TEMP_Y = 155;
  const int DAY_Y = 65;  // Day name above icon
  
  for (int i = 0; i < 5; i++) {
    int x = 398 + (i * 82);
    
    // Day of week - use the daily[i].dt timestamp
    time_t t = (time_t)daily[i].dt;
    struct tm *tm_info = localtime(&t);
    drawString(x + 32, DAY_Y, getDayName(tm_info->tm_wday), &FONT_11PT, CENTER);
    
    // Weather icon 64x64 - MOVED DOWN
    const uint8_t* icon = getWeatherIcon64(daily[i].weather.id, true);
    drawBitmap(x, ICON_Y, icon, 64, 64);
    
    // High/Low temperatures - MOVED DOWN
    snprintf(buf, sizeof(buf), "%.0f", daily[i].temp.max);
    drawString(x + 25, FORECAST_TEMP_Y, buf, &FONT_8PT, RIGHT);
    drawString(x + 35, FORECAST_TEMP_Y, "|", &FONT_8PT, CENTER);
    snprintf(buf, sizeof(buf), "%.0f", daily[i].temp.min);
    drawString(x + 45, FORECAST_TEMP_Y, buf, &FONT_8PT, LEFT);
  }
}

/* BUGFIX: drawOutlookGraph - Adjusted position */
void drawOutlookGraph(const owm_hourly_t *hourly, const owm_daily_t *daily,
                      tm timeInfo) {
  (void)daily;
  (void)timeInfo;
  
  // Title - moved down
  drawString(GRAPH_X + GRAPH_W/2, 208, "24-Hour Temperature", &FONT_12PT, CENTER);
  
  // Draw border
  Paint_DrawRectangle(GRAPH_X, GRAPH_Y, GRAPH_X + GRAPH_W, GRAPH_BOTTOM, 
                      BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  
  // Find min/max temperatures
  float minTemp = hourly[0].temp;
  float maxTemp = hourly[0].temp;
  for (int i = 1; i < 24; i++) {
    if (hourly[i].temp < minTemp) minTemp = hourly[i].temp;
    if (hourly[i].temp > maxTemp) maxTemp = hourly[i].temp;
  }
  float tempRange = maxTemp - minTemp;
  if (tempRange < 1.0f) tempRange = 1.0f;
  
  // Draw average line - with bounds checking
  float avgTemp = (minTemp + maxTemp) / 2.0f;
  int avgY = GRAPH_BOTTOM - (int)(((avgTemp - minTemp) / tempRange) * (GRAPH_H - 20)) - 10;
  // Clamp avgY to valid range
  if (avgY < GRAPH_Y) avgY = GRAPH_Y;
  if (avgY > GRAPH_BOTTOM) avgY = GRAPH_BOTTOM;
  for (int x = GRAPH_X; x < GRAPH_X + GRAPH_W; x += 6) {
    safeDrawPoint(x, avgY, DOT_PIXEL_1X1, DOT_FILL_AROUND);
  }
  
  // Draw min/max labels
  char buf[16];
  snprintf(buf, sizeof(buf), "%.0f", maxTemp);
  drawString(GRAPH_X + GRAPH_W + 5, GRAPH_Y + 5, buf, &FONT_8PT, LEFT);
  snprintf(buf, sizeof(buf), "%.0f", minTemp);
  drawString(GRAPH_X + GRAPH_W + 5, GRAPH_BOTTOM - 15, buf, &FONT_8PT, LEFT);
  
  // Calculate points
  int pointsToDraw = 24;
  int xStep = GRAPH_W / (pointsToDraw - 1);
  int xPoints[24];
  int yPoints[24];
  
  for (int i = 0; i < pointsToDraw; i++) {
    xPoints[i] = GRAPH_X + (i * xStep);
    yPoints[i] = GRAPH_BOTTOM - (int)(((hourly[i].temp - minTemp) / tempRange) * (GRAPH_H - 20)) - 10;
  }
  
  // Fill area under curve - with bounds checking
  for (int x = 0; x < GRAPH_W; x += 4) {
    int col = GRAPH_X + x;
    float idx = (float)x / xStep;
    int i1 = (int)idx;
    int i2 = i1 + 1;
    if (i2 >= pointsToDraw) i2 = pointsToDraw - 1;
    float frac = idx - i1;
    int yAtX = yPoints[i1] + (int)((yPoints[i2] - yPoints[i1]) * frac);
    
    // Clamp yAtX to valid range
    if (yAtX < GRAPH_Y) yAtX = GRAPH_Y;
    if (yAtX > GRAPH_BOTTOM) yAtX = GRAPH_BOTTOM;
    
    for (int y = GRAPH_BOTTOM - 1; y >= yAtX; y -= 3) {
      safeDrawPoint(col, y, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    }
  }
  
  // Draw temperature line - points are already clamped, but ensure they're valid
  for (int i = 0; i < pointsToDraw - 1; i++) {
    // Additional safety check
    if (xPoints[i] >= 0 && xPoints[i] < DISP_WIDTH && 
        yPoints[i] >= 0 && yPoints[i] < DISP_HEIGHT &&
        xPoints[i+1] >= 0 && xPoints[i+1] < DISP_WIDTH && 
        yPoints[i+1] >= 0 && yPoints[i+1] < DISP_HEIGHT) {
      Paint_DrawLine(xPoints[i], yPoints[i], xPoints[i+1], yPoints[i+1], 
                     BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    }
  }
  
  // Draw points and labels every 3 hours
  for (int i = 0; i < pointsToDraw; i += 3) {
    Paint_DrawPoint(xPoints[i], yPoints[i], BLACK, DOT_PIXEL_2X2, DOT_FILL_AROUND);
    
    time_t t = (time_t)hourly[i].dt;
    struct tm *tm_info = localtime(&t);
    strftime(buf, sizeof(buf), "%Hh", tm_info);
    drawString(xPoints[i], GRAPH_BOTTOM + 5, buf, &FONT_8PT, CENTER);
    
    snprintf(buf, sizeof(buf), "%.0f", hourly[i].temp);
    drawString(xPoints[i], yPoints[i] - 15, buf, &FONT_8PT, CENTER);
  }
}

/* BUGFIX: drawLocationDate - Fixed position with proper margin */
void drawLocationDate(const String &city, const String &date) {
  // Clear header area first (prevent overlap)
  // Paint_ClearWindows(600, 0, DISP_WIDTH, 70, WHITE);
  
  // City name - larger font, positioned with margin from right
  drawString(HEADER_RIGHT_X, HEADER_CITY_Y, city.c_str(), &FONT_16PT, RIGHT);
  
  // Date
  drawString(HEADER_RIGHT_X, HEADER_DATE_Y, date.c_str(), &FONT_12PT, RIGHT);
}

/* BUGFIX: drawStatusBar - Fixed position, no overlap, Updated closer to WiFi */
void drawStatusBar(const String &statusStr, const String &refreshTimeStr,
                   int rssi, uint32_t batVoltage) {
  (void)statusStr;
  
  int y = STATUS_BAR_Y;
  char buf[32];
  
  // Updated time - MOVED CLOSER to WiFi (was x=10, now x=280)
  // Just show time, no "Updated" text to save space
  const char* timeOnly = refreshTimeStr.c_str();
  // Skip "Updated " prefix if present (8 chars)
  if (strncmp(timeOnly, "Updated ", 8) == 0) {
    timeOnly += 8;
  }
  drawString(280, y, timeOnly, &FONT_8PT, LEFT);
  
  // Center: WiFi icon + RSSI
  const uint8_t* wifiIcon = getWiFiIcon(rssi);
  drawBitmap(380, y - 8, wifiIcon, 32, 32);
  snprintf(buf, sizeof(buf), "%d dBm", rssi);
  drawString(420, y, buf, &FONT_8PT, LEFT);
  
  // Right: Battery icon + voltage
  const uint8_t* batIcon = getBatteryIcon(batVoltage);
  drawBitmap(720, y - 8, batIcon, 32, 32);
  if (batVoltage != UINT32_MAX) {
    snprintf(buf, sizeof(buf), "%.2fV", batVoltage / 1000.0f);
    drawString(758, y, buf, &FONT_8PT, LEFT);
  }
}

/* drawAlerts - Weather alerts screen */
void drawAlerts(std::vector<owm_alerts_t> &alerts,
                const String &city, const String &date) {
  (void)city;
  (void)date;
  
  if (alerts.empty()) return;
  
  Paint_Clear(WHITE);
  
  drawString(400, 20, "WEATHER ALERTS", &FONT_14PT, CENTER);
  drawBitmap(10, 10, warning_icon_48x48, 48, 48);
  
  int y = 80;
  for (size_t i = 0; i < alerts.size() && i < 3; i++) {
    drawString(20, y, alerts[i].event.c_str(), &FONT_12PT, LEFT);
    y += 25;
    
    if (alerts[i].description.length() > 0) {
      String desc = alerts[i].description.substring(0, 100);
      drawString(20, y, desc.c_str(), &FONT_8PT, LEFT);
      y += 35;
    }
    
    Paint_DrawLine(20, y, 780, y, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    y += 20;
  }
  
  refreshDisplay();
}

/* drawError - Enhanced error screen */
void drawError(const uint8_t *bitmap_196x196,
               const String &errMsgLn1, const String &errMsgLn2) {
  Paint_Clear(WHITE);
  
  if (bitmap_196x196) {
    drawBitmap(302, 80, bitmap_196x196, 196, 196);
  } else {
    drawBitmap(336, 80, error_icon_196x196, 128, 128);
  }
  
  drawString(400, 280, "ERROR", &FONT_16PT, CENTER);
  drawString(400, 320, errMsgLn1.c_str(), &FONT_12PT, CENTER);
  if (errMsgLn2.length() > 0) {
    drawString(400, 350, errMsgLn2.c_str(), &FONT_8PT, CENTER);
  }
  
  drawString(400, 420, "Will retry in next update cycle", &FONT_8PT, CENTER);
  
  refreshDisplay();
}

/* Helper: get day name */
const char* getDayName(int dayOfWeek) {
  const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  return days[dayOfWeek % 7];
}

/* Stub functions */
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

/* Utility functions */
uint16_t getStringWidth(const String &text) {
  return text.length() * 12;
}

uint16_t getStringHeight(const String &text) {
  (void)text;
  return 16;
}

void drawMultiLnString(int16_t x, int16_t y, const String &text,
                       alignment_t alignment, uint16_t max_width,
                       uint16_t max_lines, int16_t line_spacing,
                       uint16_t color) {
  (void)x; (void)y; (void)text; (void)alignment; (void)max_width;
  (void)max_lines; (void)line_spacing; (void)color;
}

void drawString(int16_t x, int16_t y, const String &text, alignment_t alignment,
                uint16_t color) {
  (void)x; (void)y; (void)text; (void)alignment; (void)color;
}
