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
#include "icons/icons_24x24.h"
#include "icons/icons_16x16.h"
#include "display_utils.h"



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
#define WIDGET_ROW_HEIGHT 62   // More space between widget rows
#define WIDGET_BASE_Y     188   // Raised to accommodate taller row spacing
#define WIDGET_ICON_SIZE  48

// Graph area constants
#define GRAPH_X      350
#define GRAPH_Y      220   // Moved down slightly
#define GRAPH_W      430
#define GRAPH_H      175   // Slightly smaller
#define GRAPH_BOTTOM (GRAPH_Y + GRAPH_H)

// Header positions (moved up)
#define HEADER_CITY_Y    15
#define HEADER_DATE_Y    40
#define HEADER_RIGHT_X   785  // Margin from right edge

// Status bar positions
#define STATUS_BAR_Y     468  // Closer to bottom edge (480)

// Temperature display (moved up)
#define TEMP_X           240
#define TEMP_Y_POS       80
#define TEMP_UNIT_X      360

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
  
  Paint_DrawString_EN(drawX, y, text, font, WHITE, BLACK);
}

/* Draw scaled character (2x) using rectangles for speed */
static void DrawScaledChar(int x, int y, char c, sFONT *font, int scale)
{
    if (c < ' ' || c > '~') return;

    int bytesPerLine = (font->Width + 7) / 8;
    int charOffset = (c - ' ') * font->Height * bytesPerLine;

    const uint8_t *ptr = &font->table[charOffset];

    for (int row = 0; row < font->Height; row++) {
        for (int col = 0; col < font->Width; col++) {

            int byteIndex = row * bytesPerLine + (col / 8);
            int bitMask = 0x80 >> (col % 8);

            if (ptr[byteIndex] & bitMask) {

                Paint_DrawRectangle(
                    x + col * scale,
                    y + row * scale,
                    x + col * scale + scale - 1,
                    y + row * scale + scale - 1,
                    BLACK,
                    DOT_PIXEL_1X1,
                    DRAW_FILL_FULL
                );
            }
        }
    }
}

/* Draw scaled string (2x size) */
static void drawStringScaled(int x, int y, const char* text, sFONT* font, alignment_t align) {
    int scale = 2;
    int charWidth = font->Width * scale;
    int textWidth = strlen(text) * charWidth;
    int drawX = x;
    
    if (align == RIGHT) {
        drawX = x - textWidth;
    } else if (align == CENTER) {
        drawX = x - textWidth / 2;
    }
    
    for (size_t i = 0; i < strlen(text); i++) {
        DrawScaledChar(drawX + (i * charWidth), y, text[i], font, scale);
    }
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
                           const owm_hourly_t &currentHour,
                           const owm_resp_air_pollution_t &owm_air_pollution,
                           float inTemp, float inHumidity) {
  (void)owm_air_pollution;
  (void)inTemp;
  (void)inHumidity;
  
  char buf[64];
  bool isDay = (current.dt > today.sunrise && current.dt < today.sunset);
  
  // ==== TOP LEFT: Weather Icon (196x196) ====
  const uint8_t* weatherIcon = getWeatherIcon196(current.weather.id, isDay);
  drawBitmap(2, -15, weatherIcon, 196, 196);  // Moved up (y=-15)
  
  // ==== TEMPERATURE - SCALED 2x Font24 ====
  snprintf(buf, sizeof(buf), "%.0f", current.temp);
  // Use scaled 2x font for 48pt effect
  drawStringScaled(TEMP_X - 30, TEMP_Y_POS - 20, buf, &FONT_24PT, CENTER);
  // Draw degree symbol (°) above and to the right
  Paint_DrawCircle(TEMP_UNIT_X + 20, TEMP_Y_POS - 25, 5, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  // C at same height as scaled number
  drawString(TEMP_UNIT_X + 30, TEMP_Y_POS - 10, "C", &FONT_16PT, LEFT);
  
  // ==== FEELS LIKE ====
  snprintf(buf, sizeof(buf), "Feels like %.0f", current.feels_like);
  drawString(TEMP_X - 10, TEMP_Y_POS + 40, buf, &FONT_12PT, CENTER);
  // Draw degree symbol for feels like
  int feelsX = TEMP_X + 55;
  int feelsY = TEMP_Y_POS + 40;
  Paint_DrawCircle(feelsX + 3, feelsY + 7, 3, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  Paint_DrawString_EN(feelsX + 10, feelsY + 5, "C", &FONT_12PT, WHITE, BLACK);
  
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
  
  // Widget 3: Wind with direction (Open-Meteo returns km/h by default)
  if (current.wind_deg > 0) {
    snprintf(buf, sizeof(buf), "%.0f km/h %s", current.wind_speed, 
             degreesToCardinal(current.wind_deg));
  } else {
    snprintf(buf, sizeof(buf), "%.0f km/h", current.wind_speed);
  }
  drawWidget(3, "Wind", buf, wi_strong_wind_48x48);
  
  // Widget 4: Pressure
  snprintf(buf, sizeof(buf), "%d hPa", current.pressure);
  drawWidget(4, "Pressure", buf, getWidgetIcon(4));
  
  // Widget 5: UV Index
  snprintf(buf, sizeof(buf), "%.1f", current.uvi);
  drawWidget(5, "UV Index", buf, getWidgetIcon(5));
  
  // Widget 6: Visibility
  if (current.visibility > 100) {
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
  
  // Widget 8: Umbrella (Rain Probability) - icon shows level, only show percentage
  float pop = currentHour.pop;  // Probability of precipitation (0.0 to 1.0)
  const uint8_t* umbrellaIcon;
  
  if (pop < 0.30f) {
    umbrellaIcon = wi_day_sunny_48x48;   // ☀️ Low chance
  } else if (pop < 0.70f) {
    umbrellaIcon = wi_umbrella_48x48;    // ☂️ Medium chance
  } else {
    umbrellaIcon = wi_rain_48x48;        // ⛈️ High chance
  }
  
  snprintf(buf, sizeof(buf), "%d%%", (int)(pop * 100));
  drawWidget(8, "Umbrella", buf, umbrellaIcon);
  
  // Widget 9: Cloud Cover
  snprintf(buf, sizeof(buf), "%d%%", current.clouds);
  drawWidget(9, "Clouds", buf, getWidgetIcon(9));
}

/* BUGFIX: drawForecast - Fixed day calculation, moved down */
void drawForecast(const owm_daily_t *daily, tm timeInfo) {
  (void)timeInfo;
  char buf[32];
  
  // MOVED DOWN: icons at y=100, temps at y=170
  const int ICON_Y = 100;
  const int FORECAST_TEMP_Y = 170;
  const int DAY_Y = 80;  // Day name above icon
  
  for (int i = 0; i < 5; i++) {
    int x = 398 + (i * 82);
    
    // Day of week - use the daily[i].dt timestamp
    time_t t = (time_t)daily[i].dt;
    struct tm *tm_info = localtime(&t);
    drawString(x + 32, DAY_Y, getDayName(tm_info->tm_wday), &FONT_11PT, CENTER);
    
    // Weather icon 64x64 - MOVED DOWN
    const uint8_t* icon = getWeatherIcon64(daily[i].weather.id, true);
    drawBitmap(x, ICON_Y, icon, 64, 64);
    
    // High/Low temperatures - MOVED DOWN (with degree symbol)
    // High temp
    snprintf(buf, sizeof(buf), "%.0f", daily[i].temp.max);
    drawString(x + 22, FORECAST_TEMP_Y, buf, &FONT_8PT, RIGHT);
    // Degree symbol for high (x closer, y lower)
    Paint_DrawCircle(x + 26, FORECAST_TEMP_Y + 2, 2, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    // Separator
    drawString(x + 35, FORECAST_TEMP_Y, "|", &FONT_8PT, CENTER);
    // Low temp
    snprintf(buf, sizeof(buf), "%.0f", daily[i].temp.min);
    drawString(x + 42, FORECAST_TEMP_Y, buf, &FONT_8PT, LEFT);
    // Degree symbol for low (x further, y lower)
    Paint_DrawCircle(x + 52, FORECAST_TEMP_Y + 2, 2, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  }
}

/* drawOutlookGraph - Temperature line with precipitation probability bars */
void drawOutlookGraph(const owm_hourly_t *hourly, const owm_daily_t *daily,
                      tm timeInfo) {
  (void)daily;
  (void)timeInfo;
  
  // Layout constants
  const int POP_HEIGHT = 40;      // Height for precipitation bars
  const int POP_Y = GRAPH_BOTTOM - POP_HEIGHT;  // Y where pop area starts
  const int TEMP_BOTTOM = POP_Y - 8;  // Bottom of temp area (gap for separator)
  const int TEMP_H = TEMP_BOTTOM - GRAPH_Y;  // Height for temperature
  
  // Title
  drawString(GRAPH_X + GRAPH_W/2, 208, "24-Hour Outlook", &FONT_12PT, CENTER);
  
  // Draw main border
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
  
  // Draw temperature labels on left axis (no degree symbols, 3 values: max, mid, min)
  char buf[16];
  float midTemp = (minTemp + maxTemp) / 2.0f;
  snprintf(buf, sizeof(buf), "%.0f", maxTemp);
  drawString(GRAPH_X - 5, GRAPH_Y + 5, buf, &FONT_8PT, RIGHT);
  snprintf(buf, sizeof(buf), "%.0f", midTemp);
  drawString(GRAPH_X - 5, (GRAPH_Y + TEMP_BOTTOM) / 2, buf, &FONT_8PT, RIGHT);
  snprintf(buf, sizeof(buf), "%.0f", minTemp);
  drawString(GRAPH_X - 5, TEMP_BOTTOM - 12, buf, &FONT_8PT, RIGHT);
  
  // Calculate temperature points
  int pointsToDraw = 24;
  int xStep = GRAPH_W / (pointsToDraw - 1);
  int xPoints[24];
  int yPoints[24];
  
  for (int i = 0; i < pointsToDraw; i++) {
    xPoints[i] = GRAPH_X + (i * xStep);
    // Map temp to temp area only (not using pop area)
    yPoints[i] = TEMP_BOTTOM - (int)(((hourly[i].temp - minTemp) / tempRange) * (TEMP_H - 10)) - 5;
    // Clamp to temp area
    if (yPoints[i] < GRAPH_Y) yPoints[i] = GRAPH_Y;
    if (yPoints[i] > TEMP_BOTTOM) yPoints[i] = TEMP_BOTTOM;
  }
  
  // Draw temperature line (solid, no fill)
  for (int i = 0; i < pointsToDraw - 1; i++) {
    if (xPoints[i] >= 0 && xPoints[i] < DISP_WIDTH && 
        yPoints[i] >= 0 && yPoints[i] < DISP_HEIGHT &&
        xPoints[i+1] >= 0 && xPoints[i+1] < DISP_WIDTH && 
        yPoints[i+1] >= 0 && yPoints[i+1] < DISP_HEIGHT) {
      Paint_DrawLine(xPoints[i], yPoints[i], xPoints[i+1], yPoints[i+1], 
                     BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    }
  }
  
  // Draw precipitation probability bars (every 3 hours)
  int barWidth = 12;
  for (int i = 0; i < pointsToDraw; i += 3) {
    float pop = hourly[i].pop;  // 0.0 to 1.0
    if (pop > 0.05f) {  // Only draw if > 5%
      int barHeight = (int)(pop * POP_HEIGHT);
      int barX = xPoints[i] - barWidth/2;
      int barY = GRAPH_BOTTOM - barHeight;
      
      // Draw filled bar
      Paint_DrawRectangle(barX, barY, barX + barWidth, GRAPH_BOTTOM - 1,
                          BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    }
  }
  
  // Right axis label for pop (100% at top of graph, 0% at bottom)
  drawString(GRAPH_X + GRAPH_W + 5, GRAPH_Y + 5, "100%", &FONT_7PT, LEFT);
  drawString(GRAPH_X + GRAPH_W + 5, GRAPH_BOTTOM - 7, "0%", &FONT_7PT, LEFT);
  
  // Draw time labels and temp points every 3 hours
  for (int i = 0; i < pointsToDraw; i += 3) {
    // Temperature point
    Paint_DrawPoint(xPoints[i], yPoints[i], BLACK, DOT_PIXEL_2X2, DOT_FILL_AROUND);
    
    // Time label
    time_t t = (time_t)hourly[i].dt;
    struct tm *tm_info = localtime(&t);
#if USE_12H_FORMAT
    strftime(buf, sizeof(buf), "%I %p", tm_info);
#else
    strftime(buf, sizeof(buf), "%Hh", tm_info);
#endif
    drawString(xPoints[i], GRAPH_BOTTOM + 8, buf, &FONT_8PT, CENTER);
    
    // Temp label above point
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

/* Status bar - aligned right but moved left to avoid overlap */
void drawStatusBar(const String &statusStr, const String &refreshTimeStr,
                   int rssi, uint32_t batVoltage) {
  (void)statusStr;
  
  int y = STATUS_BAR_Y;
  char buf[48];
  
  // All elements moved left to prevent battery text overlap
  // and provide better spacing
  
  // Battery: icon 24x24 + percentage + voltage
  // Position: x=650 for icon, text at x=680 (30px gap)
  uint32_t displayVoltage = batVoltage;
  if (batVoltage == UINT32_MAX) {
    displayVoltage = MAX_BATTERY_VOLTAGE;
  }
  uint32_t batteryPercent = calcBatPercent(displayVoltage, MIN_BATTERY_VOLTAGE, MAX_BATTERY_VOLTAGE);
  const uint8_t* batIcon = getBatBitmap24(batteryPercent);
  drawBitmap(650, y - 4, batIcon, 24, 24);
  snprintf(buf, sizeof(buf), "%d%% (%.2fV)", batteryPercent, displayVoltage / 1000.0f);
  drawString(680, y, buf, &FONT_8PT, LEFT);
  
  // WiFi: icon 16x16 + quality text + RSSI (in parentheses)
  // Position: x=460 for icon, text at x=480
  const uint8_t* wifiIcon = getWiFiBitmap16(rssi);
  drawBitmap(460, y - 2, wifiIcon, 16, 16);
  const char* wifiDesc = getWiFidesc(rssi);
  snprintf(buf, sizeof(buf), "%s (%d dBm)", wifiDesc, rssi);
  drawString(480, y, buf, &FONT_8PT, LEFT);
  
  // Refresh: icon 24x24 + time
  // Position: x=350 for icon, text at x=380
  const char* timeOnly = refreshTimeStr.c_str();
  if (strncmp(timeOnly, "Updated ", 8) == 0) {
    timeOnly += 8;
  }
  drawBitmap(350, y - 4, wi_refresh_alt_24x24, 24, 24);
  drawString(380, y, timeOnly, &FONT_8PT, LEFT);
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
