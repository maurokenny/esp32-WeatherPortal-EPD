/* Renderer minimalista para Waveshare 7.5" V2 */
#include "renderer.h"
#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include <stdlib.h>

static UBYTE *imageBuffer = NULL;
static bool displayInitialized = false;

DisplayClass display;

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

void refreshDisplay() {
  if (displayInitialized && imageBuffer != NULL) {
    EPD_7IN5_V2_Display(imageBuffer);
  }
}

void powerOffDisplay() {
  if (displayInitialized) {
    EPD_7IN5_V2_Sleep();
  }
}

// Funções de desenho vazias por enquanto - só para compilar
void drawCurrentConditions(const owm_current_t &current,
                           const owm_daily_t &today,
                           const owm_resp_air_pollution_t &owm_air_pollution,
                           float inTemp, float inHumidity) {
  (void)current; (void)today; (void)owm_air_pollution; (void)inTemp; (void)inHumidity;
  Paint_Clear(WHITE);
  Paint_DrawString_EN(10, 10, "Weather Display", &Font16, BLACK, WHITE);
  Paint_DrawString_EN(10, 30, "Working!", &Font12, BLACK, WHITE);
  refreshDisplay();
}

void drawForecast(const owm_daily_t *daily, tm timeInfo) {
  (void)daily; (void)timeInfo;
}

void drawOutlookGraph(const owm_hourly_t *hourly, const owm_daily_t *daily, tm timeInfo) {
  (void)hourly; (void)daily; (void)timeInfo;
}

void drawLocationDate(const String &city, const String &date) {
  (void)city; (void)date;
}

void drawAlerts(std::vector<owm_alerts_t> &alerts, const String &city, const String &date) {
  (void)alerts; (void)city; (void)date;
}

void drawStatusBar(const String &statusStr, const String &refreshTimeStr,
                   int rssi, uint32_t batVoltage) {
  (void)statusStr; (void)refreshTimeStr; (void)rssi; (void)batVoltage;
}

void drawError(const uint8_t *bitmap_196x196,
               const String &errMsgLn1, const String &errMsgLn2) {
  (void)bitmap_196x196;
  Paint_Clear(WHITE);
  Paint_DrawString_EN(10, 10, "ERROR:", &Font16, BLACK, WHITE);
  Paint_DrawString_EN(10, 30, errMsgLn1.c_str(), &Font12, BLACK, WHITE);
  if (errMsgLn2.length() > 0) {
    Paint_DrawString_EN(10, 50, errMsgLn2.c_str(), &Font12, BLACK, WHITE);
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
