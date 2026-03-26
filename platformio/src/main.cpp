/* Main program for esp32-weather-epd.
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

#include "config.h"
#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <Preferences.h>
#include <time.h>
#include <cstring>
#include <WiFi.h>
#include <Wire.h>

#include "_locale.h"
#include "api_response.h"
#include "client_utils.h"
#include "config.h"
#include "display_utils.h"
#include "icons/icons_196x196.h"
#include "renderer.h"

#if defined(SENSOR_BME280)
  #include <Adafruit_BME280.h>
#endif
#if defined(SENSOR_BME680)
  #include <Adafruit_BME680.h>
#endif
#if defined(USE_HTTPS_WITH_CERT_VERIF) || defined(USE_HTTPS_WITH_CERT_VERIF)
  #include <WiFiClientSecure.h>
#endif
#ifdef USE_HTTPS_WITH_CERT_VERIF
  #include "cert.h"
#endif

// too large to allocate locally on stack
static owm_resp_onecall_t       owm_onecall;
static owm_resp_air_pollution_t owm_air_pollution;

Preferences prefs;

/* Fill data structures with mockup data for testing without WiFi/API
 * Temperature values are kept below 100 degrees to ensure proper display
 * layout (2-digit temperatures fit better in the UI than 3-digit ones).
 */
void fillMockupData(owm_resp_onecall_t &owm_onecall, tm &timeInfo)
{
  Serial.println("Using MOCKUP DATA - No WiFi/API calls");
  
  // Initialize entire structure to zero to avoid garbage values
  owm_onecall = {};
  
  // Set current time - initialize all fields to avoid undefined behavior
  memset(&timeInfo, 0, sizeof(tm));
  timeInfo.tm_year = 2025 - 1900;  // Year since 1900
  timeInfo.tm_mon = 2;             // March (0-11)
  timeInfo.tm_mday = 25;           // Day of month
  timeInfo.tm_hour = 14;           // Hour
  timeInfo.tm_min = 30;            // Minute
  timeInfo.tm_sec = 0;
  timeInfo.tm_isdst = -1;          // Let mktime determine DST
  time_t now = mktime(&timeInfo);
  
  // Set ESP32 internal clock so getLocalTime() works
  struct timeval tv = { .tv_sec = now, .tv_usec = 0 };
  settimeofday(&tv, NULL);
  setenv("TZ", TIMEZONE, 1);
  tzset();
  
  // Current weather based on MOCKUP_CURRENT_WEATHER setting
  switch (MOCKUP_CURRENT_WEATHER) {
    case MOCKUP_WEATHER_SUNNY:
      // Sunny - Clear sky, warm
      owm_onecall.current.temp = 298.15f;  // 25°C
      owm_onecall.current.feels_like = 300.15f;  // 27°C
      owm_onecall.current.humidity = 45;
      owm_onecall.current.clouds = 10;
      owm_onecall.current.uvi = 8.0f;
      owm_onecall.current.visibility = 10000;
      owm_onecall.current.weather.id = 800;  // Clear sky
      owm_onecall.current.weather.main = "Clear";
      owm_onecall.current.weather.description = "clear sky";
      owm_onecall.current.weather.icon = "01d";
      Serial.println("Mockup: Sunny weather");
      break;
      
    case MOCKUP_WEATHER_RAINY:
      // Rainy - Moderate rain
      owm_onecall.current.temp = 288.15f;  // 15°C
      owm_onecall.current.feels_like = 285.15f;  // 12°C
      owm_onecall.current.humidity = 85;
      owm_onecall.current.clouds = 85;
      owm_onecall.current.uvi = 2.0f;
      owm_onecall.current.visibility = 8000;
      owm_onecall.current.weather.id = 501;  // Moderate rain
      owm_onecall.current.weather.main = "Rain";
      owm_onecall.current.weather.description = "moderate rain";
      owm_onecall.current.weather.icon = "10d";
      Serial.println("Mockup: Rainy weather");
      break;
      
    case MOCKUP_WEATHER_SNOWY:
      // Snowy - Light snow, freezing
      owm_onecall.current.temp = 273.15f;  // 0°C
      owm_onecall.current.feels_like = 268.15f;  // -5°C
      owm_onecall.current.humidity = 80;
      owm_onecall.current.clouds = 90;
      owm_onecall.current.uvi = 1.0f;
      owm_onecall.current.visibility = 5000;
      owm_onecall.current.weather.id = 600;  // Light snow
      owm_onecall.current.weather.main = "Snow";
      owm_onecall.current.weather.description = "light snow";
      owm_onecall.current.weather.icon = "13d";
      Serial.println("Mockup: Snowy weather");
      break;
      
    case MOCKUP_WEATHER_CLOUDY:
      // Cloudy - Overcast
      owm_onecall.current.temp = 291.15f;  // 18°C
      owm_onecall.current.feels_like = 290.15f;  // 17°C
      owm_onecall.current.humidity = 65;
      owm_onecall.current.clouds = 90;
      owm_onecall.current.uvi = 3.0f;
      owm_onecall.current.visibility = 9000;
      owm_onecall.current.weather.id = 804;  // Overcast
      owm_onecall.current.weather.main = "Clouds";
      owm_onecall.current.weather.description = "overcast clouds";
      owm_onecall.current.weather.icon = "04d";
      Serial.println("Mockup: Cloudy weather");
      break;
      
    case MOCKUP_WEATHER_THUNDER:
      // Thunderstorm
      owm_onecall.current.temp = 293.15f;  // 20°C
      owm_onecall.current.feels_like = 295.15f;  // 22°C
      owm_onecall.current.humidity = 90;
      owm_onecall.current.clouds = 100;
      owm_onecall.current.uvi = 1.0f;
      owm_onecall.current.visibility = 6000;
      owm_onecall.current.weather.id = 211;  // Thunderstorm
      owm_onecall.current.weather.main = "Thunderstorm";
      owm_onecall.current.weather.description = "thunderstorm";
      owm_onecall.current.weather.icon = "11d";
      Serial.println("Mockup: Thunderstorm weather");
      break;
      
    default:
      // Default to rainy
      owm_onecall.current.temp = 288.15f;
      owm_onecall.current.feels_like = 285.15f;
      owm_onecall.current.humidity = 85;
      owm_onecall.current.clouds = 85;
      owm_onecall.current.uvi = 2.0f;
      owm_onecall.current.visibility = 8000;
      owm_onecall.current.weather.id = 501;
      owm_onecall.current.weather.main = "Rain";
      owm_onecall.current.weather.description = "moderate rain";
      owm_onecall.current.weather.icon = "10d";
      Serial.println("Mockup: Default weather (rainy)");
      break;
  }
  
  // Common current weather fields
  owm_onecall.current.dt = now;
  owm_onecall.current.pressure = 1013;
  owm_onecall.current.wind_speed = 10.0f;
  owm_onecall.current.wind_deg = 180;
  
  // Hourly forecast (24 hours) - varied weather icons
  // Configure rain data based on MOCKUP_RAIN_WIDGET_STATE
  for (int i = 0; i < 24; i++) {
    owm_onecall.hourly[i].dt = now + (i * 3600);
    owm_onecall.hourly[i].temp = 293.15f + (i % 5) - 2.0f;  // ~20°C in Kelvin
    owm_onecall.hourly[i].feels_like = owm_onecall.hourly[i].temp - 1.0f;
    owm_onecall.hourly[i].pressure = 1013;
    owm_onecall.hourly[i].humidity = 60 + (i % 10);
    
    // Configure POP (Probability of Precipitation) based on selected widget state
    switch (MOCKUP_RAIN_WIDGET_STATE) {
      case MOCKUP_RAIN_NO_RAIN:
        // POP < 30% - shows "No rain (X%)" with X over icon
        owm_onecall.hourly[i].pop = (i == 0) ? 0.10f : 0.05f;  // 10% max
        break;
        
      case MOCKUP_RAIN_COMPACT:
        // POP 30-70% - shows "Compact (X%)" or "Rain in Ymin (X%)"
        if (i == 1) {
          owm_onecall.hourly[i].dt = now + (45 * 60);  // 45 minutes from now
          owm_onecall.hourly[i].pop = 0.50f;  // 50% rain in 45 minutes
        } else if (i == 3) {
          owm_onecall.hourly[i].pop = 0.40f;  // 40% rain in 3 hours
        } else {
          owm_onecall.hourly[i].pop = 0.10f;  // Low probability
        }
        break;
        
      case MOCKUP_RAIN_TAKE:
        // POP >= 70% - shows "Take (X%)" or "Rain in Ymin (X%)"
        if (i == 1) {
          owm_onecall.hourly[i].dt = now + (45 * 60);  // 45 minutes from now
          owm_onecall.hourly[i].pop = 0.80f;  // 80% rain in 45 minutes
        } else if (i == 0) {
          owm_onecall.hourly[i].pop = 0.75f;  // 75% rain now
        } else {
          owm_onecall.hourly[i].pop = 0.20f;  // Lower probability
        }
        break;
    }
    
    // Varied weather icons for hourly forecast
    switch (i % 8) {
      case 0:  // Sunny
        owm_onecall.hourly[i].weather.id = 800;
        owm_onecall.hourly[i].weather.main = "Clear";
        owm_onecall.hourly[i].weather.description = "clear sky";
        owm_onecall.hourly[i].weather.icon = "01d";
        owm_onecall.hourly[i].rain_1h = 0.0f;
        break;
      case 1:  // Few clouds
        owm_onecall.hourly[i].weather.id = 801;
        owm_onecall.hourly[i].weather.main = "Clouds";
        owm_onecall.hourly[i].weather.description = "few clouds";
        owm_onecall.hourly[i].weather.icon = "02d";
        owm_onecall.hourly[i].rain_1h = 0.0f;
        break;
      case 2:  // Scattered clouds
        owm_onecall.hourly[i].weather.id = 802;
        owm_onecall.hourly[i].weather.main = "Clouds";
        owm_onecall.hourly[i].weather.description = "scattered clouds";
        owm_onecall.hourly[i].weather.icon = "03d";
        owm_onecall.hourly[i].rain_1h = 0.0f;
        break;
      case 3:  // Broken clouds
        owm_onecall.hourly[i].weather.id = 803;
        owm_onecall.hourly[i].weather.main = "Clouds";
        owm_onecall.hourly[i].weather.description = "broken clouds";
        owm_onecall.hourly[i].weather.icon = "04d";
        owm_onecall.hourly[i].rain_1h = 0.0f;
        break;
      case 4:  // Shower rain
        owm_onecall.hourly[i].weather.id = 521;
        owm_onecall.hourly[i].weather.main = "Rain";
        owm_onecall.hourly[i].weather.description = "shower rain";
        owm_onecall.hourly[i].weather.icon = "09d";
        owm_onecall.hourly[i].rain_1h = 2.5f;
        break;
      case 5:  // Rain
        owm_onecall.hourly[i].weather.id = 500;
        owm_onecall.hourly[i].weather.main = "Rain";
        owm_onecall.hourly[i].weather.description = "light rain";
        owm_onecall.hourly[i].weather.icon = "10d";
        owm_onecall.hourly[i].rain_1h = 1.5f;
        break;
      case 6:  // Thunderstorm
        owm_onecall.hourly[i].weather.id = 200;
        owm_onecall.hourly[i].weather.main = "Thunderstorm";
        owm_onecall.hourly[i].weather.description = "thunderstorm with light rain";
        owm_onecall.hourly[i].weather.icon = "11d";
        owm_onecall.hourly[i].rain_1h = 5.0f;
        break;
      case 7:  // Snow
        owm_onecall.hourly[i].weather.id = 600;
        owm_onecall.hourly[i].weather.main = "Snow";
        owm_onecall.hourly[i].weather.description = "light snow";
        owm_onecall.hourly[i].weather.icon = "13d";
        owm_onecall.hourly[i].rain_1h = 0.0f;
        break;
    }
  }
  
  // Log selected rain widget state
  switch (MOCKUP_RAIN_WIDGET_STATE) {
    case MOCKUP_RAIN_NO_RAIN:
      Serial.println("Mockup: Rain widget = NO RAIN (<30% POP)");
      break;
    case MOCKUP_RAIN_COMPACT:
      Serial.println("Mockup: Rain widget = COMPACT (30-70% POP)");
      break;
    case MOCKUP_RAIN_TAKE:
      Serial.println("Mockup: Rain widget = TAKE (>=70% POP)");
      break;
  }
  
  // Daily forecast (5 days) - each day with different weather type
  for (int i = 0; i < 5; i++) {
    owm_onecall.daily[i].dt = now + (i * 86400);
    owm_onecall.daily[i].temp.max = 295.15f + (i % 3);  // ~22°C in Kelvin
    owm_onecall.daily[i].temp.min = 288.15f - (i % 2);  // ~15°C in Kelvin
    owm_onecall.daily[i].pop = 0.0f;  // No rain by default
    
    // Different weather for each day
    switch (i) {
      case 0:  // Day 1: Sunny
        owm_onecall.daily[i].weather.id = 800;
        owm_onecall.daily[i].weather.main = "Clear";
        owm_onecall.daily[i].weather.description = "clear sky";
        owm_onecall.daily[i].weather.icon = "01d";
        break;
      case 1:  // Day 2: Cloudy
        owm_onecall.daily[i].weather.id = 803;
        owm_onecall.daily[i].weather.main = "Clouds";
        owm_onecall.daily[i].weather.description = "broken clouds";
        owm_onecall.daily[i].weather.icon = "04d";
        break;
      case 2:  // Day 3: Rain
        owm_onecall.daily[i].weather.id = 500;
        owm_onecall.daily[i].weather.main = "Rain";
        owm_onecall.daily[i].weather.description = "light rain";
        owm_onecall.daily[i].weather.icon = "10d";
        owm_onecall.daily[i].pop = 0.70f;  // 70% rain chance
        break;
      case 3:  // Day 4: Thunderstorm
        owm_onecall.daily[i].weather.id = 200;
        owm_onecall.daily[i].weather.main = "Thunderstorm";
        owm_onecall.daily[i].weather.description = "thunderstorm with light rain";
        owm_onecall.daily[i].weather.icon = "11d";
        owm_onecall.daily[i].pop = 0.80f;  // 80% rain chance
        break;
      case 4:  // Day 5: Snow
        owm_onecall.daily[i].weather.id = 600;
        owm_onecall.daily[i].weather.main = "Snow";
        owm_onecall.daily[i].weather.description = "light snow";
        owm_onecall.daily[i].weather.icon = "13d";
        owm_onecall.daily[i].pop = 0.50f;  // 50% snow chance
        break;
    }
  }
  
  Serial.println("Mockup data filled successfully!");
}

/* Put esp32 into ultra low-power deep sleep (<11μA).
 * Aligns wake time to the minute. Sleep times defined in config.cpp.
 */
void beginDeepSleep(unsigned long startTime, tm *timeInfo)
{
  if (!getLocalTime(timeInfo))
  {
    Serial.println(TXT_REFERENCING_OLDER_TIME_NOTICE);
  }

  // To simplify sleep time calculations, the current time stored by timeInfo
  // will be converted to time relative to the WAKE_TIME. This way if a
  // SLEEP_DURATION is not a multiple of 60 minutes it can be more trivially,
  // aligned and it can easily be deterimined whether we must sleep for
  // additional time due to bedtime.
  // i.e. when curHour == 0, then timeInfo->tm_hour == WAKE_TIME
  int bedtimeHour = INT_MAX;
  if (BED_TIME != WAKE_TIME)
  {
    bedtimeHour = (BED_TIME - WAKE_TIME + 24) % 24;
  }

  // time is relative to wake time
  int curHour = (timeInfo->tm_hour - WAKE_TIME + 24) % 24;
  const int curMinute = curHour * 60 + timeInfo->tm_min;
  const int curSecond = curHour * 3600
                      + timeInfo->tm_min * 60
                      + timeInfo->tm_sec;
  const int desiredSleepSeconds = SLEEP_DURATION * 60;
  const int offsetMinutes = curMinute % SLEEP_DURATION;
  const int offsetSeconds = curSecond % desiredSleepSeconds;

  // align wake time to nearest multiple of SLEEP_DURATION
  int sleepMinutes = SLEEP_DURATION - offsetMinutes;
  if (desiredSleepSeconds - offsetSeconds < 120
   || offsetSeconds / (float)desiredSleepSeconds > 0.95f)
  { // if we have a sleep time less than 2 minutes OR less 5% SLEEP_DURATION,
    // skip to next alignment
    sleepMinutes += SLEEP_DURATION;
  }

  // estimated wake time, if this falls in a sleep period then sleepDuration
  // must be adjusted
  const int predictedWakeHour = ((curMinute + sleepMinutes) / 60) % 24;

  uint64_t sleepDuration;
  if (predictedWakeHour < bedtimeHour)
  {
    sleepDuration = sleepMinutes * 60 - timeInfo->tm_sec;
  }
  else
  {
    const int hoursUntilWake = 24 - curHour;
    sleepDuration = hoursUntilWake * 3600ULL
                    - (timeInfo->tm_min * 60ULL + timeInfo->tm_sec);
  }

  // add extra delay to compensate for esp32's with fast RTCs.
  sleepDuration += 3ULL;
  sleepDuration *= 1.0015f;

#if DEBUG_LEVEL >= 1
  printHeapUsage();
#endif

  esp_sleep_enable_timer_wakeup(sleepDuration * 1000000ULL);
  Serial.print(TXT_AWAKE_FOR);
  Serial.println(" "  + String((millis() - startTime) / 1000.0, 3) + "s");
  Serial.print(TXT_ENTERING_DEEP_SLEEP_FOR);
  Serial.println(" " + String(sleepDuration) + "s");
  esp_deep_sleep_start();
} // end beginDeepSleep

/* Program entry point.
 */
void setup()
{
  unsigned long startTime = millis();
  Serial.begin(115200);

#if DEBUG_LEVEL >= 1
  printHeapUsage();
#endif

  disableBuiltinLED();

  // Open namespace for read/write to non-volatile storage
  prefs.begin(NVS_NAMESPACE, false);

#if BATTERY_MONITORING
  uint32_t batteryVoltage = readBatteryVoltage();
  Serial.print(TXT_BATTERY_VOLTAGE);
  Serial.println(": " + String(batteryVoltage) + "mv");

  // When the battery is low, the display should be updated to reflect that, but
  // only the first time we detect low voltage. The next time the display will
  // refresh is when voltage is no longer low. To keep track of that we will
  // make use of non-volatile storage.
  bool lowBat = prefs.getBool("lowBat", false);

  // low battery, deep sleep now (only if using battery)
  if (USING_BATTERY && batteryVoltage <= LOW_BATTERY_VOLTAGE)
  {
    if (lowBat == false)
    { // battery is now low for the first time
      prefs.putBool("lowBat", true);
      prefs.end();
      initDisplay();
      do
      {
        drawError(battery_alert_0deg_196x196, TXT_LOW_BATTERY);
      } while (display.nextPage());
      powerOffDisplay();
    }

    if (batteryVoltage <= CRIT_LOW_BATTERY_VOLTAGE)
    { // critically low battery
      // don't set esp_sleep_enable_timer_wakeup();
      // We won't wake up again until someone manually presses the RST button.
      Serial.println(TXT_CRIT_LOW_BATTERY_VOLTAGE);
      Serial.println(TXT_HIBERNATING_INDEFINITELY_NOTICE);
    }
    else if (batteryVoltage <= VERY_LOW_BATTERY_VOLTAGE)
    { // very low battery
      esp_sleep_enable_timer_wakeup(VERY_LOW_BATTERY_SLEEP_INTERVAL
                                    * 60ULL * 1000000ULL);
      Serial.println(TXT_VERY_LOW_BATTERY_VOLTAGE);
      Serial.print(TXT_ENTERING_DEEP_SLEEP_FOR);
      Serial.println(" " + String(VERY_LOW_BATTERY_SLEEP_INTERVAL) + "min");
    }
    else
    { // low battery
      esp_sleep_enable_timer_wakeup(LOW_BATTERY_SLEEP_INTERVAL
                                    * 60ULL * 1000000ULL);
      Serial.println(TXT_LOW_BATTERY_VOLTAGE);
      Serial.print(TXT_ENTERING_DEEP_SLEEP_FOR);
      Serial.println(" " + String(LOW_BATTERY_SLEEP_INTERVAL) + "min");
    }
    esp_deep_sleep_start();
  }
  // battery is no longer low, reset variable in non-volatile storage
  if (lowBat == true)
  {
    prefs.putBool("lowBat", false);
  }
#else
  uint32_t batteryVoltage = UINT32_MAX;
#endif

  // All data should have been loaded from NVS. Close filesystem.
  prefs.end();

  String statusStr = {};
  String tmpStr = {};
  tm timeInfo = {};
  bool timeConfigured = false;

#if USE_MOCKUP_DATA
  // Use mockup data instead of WiFi/API
  int wifiRSSI = -50; // Fake good signal
  timeConfigured = true; // Mockup always has valid time
  fillMockupData(owm_onecall, timeInfo);
  
#else
  // START WIFI
  // Show connecting screen
  initDisplay();
  do
  {
    drawLoading(wifi_196x196, "Connecting WiFi...", "Please wait");
  } while (display.nextPage());
  
  int wifiRSSI = 0; // "Received Signal Strength Indicator"
  wl_status_t wifiStatus = startWiFi(wifiRSSI);
  if (wifiStatus != WL_CONNECTED)
  { // WiFi Connection Failed
    killWiFi();
    initDisplay();
    if (wifiStatus == WL_NO_SSID_AVAIL)
    {
      Serial.println(TXT_NETWORK_NOT_AVAILABLE);
      do
      {
        drawError(wifi_x_196x196, TXT_NETWORK_NOT_AVAILABLE);
      } while (display.nextPage());
    }
    else
    {
      Serial.println(TXT_WIFI_CONNECTION_FAILED);
      do
      {
        drawError(wifi_x_196x196, TXT_WIFI_CONNECTION_FAILED);
      } while (display.nextPage());
    }
    powerOffDisplay();
    beginDeepSleep(startTime, &timeInfo);
  }

  // TIME SYNCHRONIZATION
  configTzTime(TIMEZONE, NTP_SERVER_1, NTP_SERVER_2);
  timeConfigured = waitForSNTPSync(&timeInfo);
  if (!timeConfigured)
  {
    Serial.println(TXT_TIME_SYNCHRONIZATION_FAILED);
    killWiFi();
    initDisplay();
    do
    {
      drawError(wi_time_4_196x196, TXT_TIME_SYNCHRONIZATION_FAILED);
    } while (display.nextPage());
    powerOffDisplay();
    beginDeepSleep(startTime, &timeInfo);
  }

  // MAKE API REQUESTS
  // Show fetching data screen
  initDisplay();
  do
  {
    drawLoading(wi_cloud_down_196x196, "Fetching weather...", "Please wait");
  } while (display.nextPage());
  
#ifdef USE_HTTP
  WiFiClient client;
#elif defined(USE_HTTPS_NO_CERT_VERIF)
  WiFiClientSecure client;
  client.setInsecure();
#elif defined(USE_HTTPS_WITH_CERT_VERIF)
  WiFiClientSecure client;
  client.setCACert(cert_Sectigo_RSA_Organization_Validation_Secure_Server_CA);
#endif
  int rxStatus = getOpenMeteoForecast(client, owm_onecall);
  if (rxStatus != HTTP_CODE_OK)
  {
    killWiFi();
    statusStr = "Open-Meteo Forecast API";
    tmpStr = String(rxStatus, DEC) + ": " + getHttpResponsePhrase(rxStatus);
    initDisplay();
    do
    {
      drawError(wi_cloud_down_196x196, statusStr, tmpStr);
    } while (display.nextPage());
    powerOffDisplay();
    beginDeepSleep(startTime, &timeInfo);
  }
  rxStatus = getOpenMeteoAirQuality(client, owm_air_pollution);
  if (rxStatus != HTTP_CODE_OK)
  {
    killWiFi();
    statusStr = "Open-Meteo Air Quality API";
    tmpStr = String(rxStatus, DEC) + ": " + getHttpResponsePhrase(rxStatus);
    initDisplay();
    do
    {
      drawError(wi_cloud_down_196x196, statusStr, tmpStr);
    } while (display.nextPage());
    powerOffDisplay();
    beginDeepSleep(startTime, &timeInfo);
  }
  killWiFi(); // WiFi no longer needed
#endif

  // GET INDOOR TEMPERATURE AND HUMIDITY, start BMEx80...
  pinMode(PIN_BME_PWR, OUTPUT);
  digitalWrite(PIN_BME_PWR, HIGH);
#if defined(SENSOR_INIT_DELAY_MS) && SENSOR_INIT_DELAY_MS > 0
  delay(SENSOR_INIT_DELAY_MS);
#endif
  TwoWire I2C_bme = TwoWire(0);
  I2C_bme.begin(PIN_BME_SDA, PIN_BME_SCL, 100000); // 100kHz
  float inTemp     = NAN;
  float inHumidity = NAN;
#if defined(SENSOR_BME280)
  Serial.print(String(TXT_READING_FROM) + " BME280... ");
  Adafruit_BME280 bme;

  if(bme.begin(BME_ADDRESS, &I2C_bme))
  {
#endif
#if defined(SENSOR_BME680)
  Serial.print(String(TXT_READING_FROM) + " BME680... ");
  Adafruit_BME680 bme(&I2C_bme);

  if(bme.begin(BME_ADDRESS))
  {
#endif
    inTemp     = bme.readTemperature(); // Celsius
    inHumidity = bme.readHumidity();    // %

    // check if BME readings are valid
    // note: readings are checked again before drawing to screen. If a reading
    //       is not a number (NAN) then an error occurred, a dash '-' will be
    //       displayed.
    if (std::isnan(inTemp) || std::isnan(inHumidity))
    {
      statusStr = "BME " + String(TXT_READ_FAILED);
      Serial.println(statusStr);
    }
    else
    {
      Serial.println(TXT_SUCCESS);
    }
  }
  else
  {
    statusStr = "BME " + String(TXT_NOT_FOUND); // check wiring
    Serial.println(statusStr);
  }
  digitalWrite(PIN_BME_PWR, LOW);

  String refreshTimeStr;
  getRefreshTimeStr(refreshTimeStr, timeConfigured, &timeInfo);
  String dateStr;
  getDateStr(dateStr, &timeInfo);

  // RENDER FULL REFRESH
  initDisplay();
  do
  {
    drawCurrentConditions(owm_onecall.current, owm_onecall.daily[0],
                          owm_air_pollution, inTemp, inHumidity, owm_onecall.hourly);
    drawOutlookGraph(owm_onecall.hourly, owm_onecall.daily, timeInfo);
    drawForecast(owm_onecall.daily, timeInfo);
    drawLocationDate(CITY_STRING, dateStr);
#if DISPLAY_ALERTS
    drawAlerts(owm_onecall.alerts, CITY_STRING, dateStr);
#endif
    drawStatusBar(statusStr, refreshTimeStr, wifiRSSI, batteryVoltage);
  } while (display.nextPage());
  powerOffDisplay();

  // DEEP SLEEP
  beginDeepSleep(startTime, &timeInfo);
} // end setup

/* This will never run
 */
void loop()
{
} // end loop

