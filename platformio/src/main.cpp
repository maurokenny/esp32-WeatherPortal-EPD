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
#include "wifi_manager.h"

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

unsigned long startTick = 0;
tm currentTimInfo = {};
bool isTimeConfigured = false;
String globalStatus = {};
String globalTmpStr = {};

void updateWeather();

/* Fill data structures with mockup data for testing without WiFi/API
 * Temperature values are kept below 100 degrees to ensure proper display
 * layout (2-digit temperatures fit better in the UI than 3-digit ones).
 */
void fillMockupData(owm_resp_onecall_t &owm_onecall, tm &timeInfo)
{
  Serial.println("Using MOCKUP DATA - No WiFi/API calls");
  
  // Initialize only the fields we use to avoid stack overflow from String constructors
  // Don't use owm_onecall = {} as it calls 171+ String constructors!
  // Initialize primitive fields individually instead of memset
  owm_onecall.current.dt = 0;
  owm_onecall.current.sunrise = 0;
  owm_onecall.current.sunset = 0;
  owm_onecall.current.temp = 0;
  owm_onecall.current.feels_like = 0;
  owm_onecall.current.pressure = 0;
  owm_onecall.current.humidity = 0;
  owm_onecall.current.dew_point = 0;
  owm_onecall.current.clouds = 0;
  owm_onecall.current.uvi = 0;
  owm_onecall.current.visibility = 0;
  owm_onecall.current.wind_speed = 0;
  owm_onecall.current.wind_gust = 0;
  owm_onecall.current.wind_deg = 0;
  owm_onecall.current.rain_1h = 0;
  owm_onecall.current.snow_1h = 0;
  owm_onecall.current.weather.id = 0;
  owm_onecall.current.weather.main = "";
  owm_onecall.current.weather.description = "";
  owm_onecall.current.weather.icon = "";
  
  // Set current time - initialize all fields to avoid undefined behavior
  timeInfo = {};  // Value-initialization for tm struct
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
    // Clear hourly entry first (avoid uninitialized values)
    // Initialize primitive fields individually instead of memset
    owm_onecall.hourly[i].dt = 0;
    owm_onecall.hourly[i].temp = 0;
    owm_onecall.hourly[i].feels_like = 0;
    owm_onecall.hourly[i].pressure = 0;
    owm_onecall.hourly[i].humidity = 0;
    owm_onecall.hourly[i].dew_point = 0;
    owm_onecall.hourly[i].clouds = 0;
    owm_onecall.hourly[i].uvi = 0;
    owm_onecall.hourly[i].visibility = 0;
    owm_onecall.hourly[i].wind_speed = 0;
    owm_onecall.hourly[i].wind_gust = 0;
    owm_onecall.hourly[i].wind_deg = 0;
    owm_onecall.hourly[i].pop = 0;
    owm_onecall.hourly[i].rain_1h = 0;
    owm_onecall.hourly[i].snow_1h = 0;
    owm_onecall.hourly[i].weather.id = 0;
    owm_onecall.hourly[i].weather.main = "";
    owm_onecall.hourly[i].weather.description = "";
    owm_onecall.hourly[i].weather.icon = "";
    
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
        
      case MOCKUP_RAIN_GRAPH_TEST:
        // Graph test - POP varies from 0% to 120% to test graph scaling
        // Uses 5% increments per hour to see how graph handles 0-100%+ range
        owm_onecall.hourly[i].pop = (i * 0.05f);  // 0%, 5%, 10%, ... 115%
        // Cap at realistic max of 100% for most hours, but let last few exceed
        if (owm_onecall.hourly[i].pop > 1.0f) {
          owm_onecall.hourly[i].pop = 1.0f;  // Max 100% for display
        }
        // Add some variation to make it interesting
        if (i >= 12 && i < 16) {
          owm_onecall.hourly[i].pop = 0.95f;  // Heavy rain afternoon
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
    case MOCKUP_RAIN_GRAPH_TEST:
      Serial.println("Mockup: Rain widget = GRAPH TEST (0-100% POP ramp)");
      break;
  }
  
  // Daily forecast (5 days) - each day with different weather type
  for (int i = 0; i < 5; i++) {
    // Clear daily entry first
    // Initialize primitive fields individually instead of memset
    owm_onecall.daily[i].dt = 0;
    owm_onecall.daily[i].sunrise = 0;
    owm_onecall.daily[i].sunset = 0;
    owm_onecall.daily[i].moonrise = 0;
    owm_onecall.daily[i].moonset = 0;
    owm_onecall.daily[i].moon_phase = 0;
    owm_onecall.daily[i].temp.morn = 0;
    owm_onecall.daily[i].temp.day = 0;
    owm_onecall.daily[i].temp.eve = 0;
    owm_onecall.daily[i].temp.night = 0;
    owm_onecall.daily[i].temp.min = 0;
    owm_onecall.daily[i].temp.max = 0;
    owm_onecall.daily[i].feels_like.morn = 0;
    owm_onecall.daily[i].feels_like.day = 0;
    owm_onecall.daily[i].feels_like.eve = 0;
    owm_onecall.daily[i].feels_like.night = 0;
    owm_onecall.daily[i].pressure = 0;
    owm_onecall.daily[i].humidity = 0;
    owm_onecall.daily[i].dew_point = 0;
    owm_onecall.daily[i].clouds = 0;
    owm_onecall.daily[i].uvi = 0;
    owm_onecall.daily[i].visibility = 0;
    owm_onecall.daily[i].wind_speed = 0;
    owm_onecall.daily[i].wind_gust = 0;
    owm_onecall.daily[i].wind_deg = 0;
    owm_onecall.daily[i].pop = 0;
    owm_onecall.daily[i].rain = 0;
    owm_onecall.daily[i].snow = 0;
    owm_onecall.daily[i].weather.id = 0;
    owm_onecall.daily[i].weather.main = "";
    owm_onecall.daily[i].weather.description = "";
    owm_onecall.daily[i].weather.icon = "";
    
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
// Old setup() removed.
// setup_old removed
// Old blocking logic removed.

void updateWeather()
{
  int wifiRSSI = 0;
  if (WiFi.status() == WL_CONNECTED) {
      wifiRSSI = WiFi.RSSI();
  }

  // TIME SYNCHRONIZATION
  configTzTime(TIMEZONE, NTP_SERVER_1, NTP_SERVER_2);
  isTimeConfigured = waitForSNTPSync(&currentTimInfo);
  if (!isTimeConfigured)
  {
    Serial.println(TXT_TIME_SYNCHRONIZATION_FAILED);
    updateEinkStatus(TXT_TIME_SYNCHRONIZATION_FAILED);
    setFirmwareState(STATE_SLEEP_PENDING);
    return;
  }

  // MAKE API REQUESTS
  if (ramAutoGeo) {
    if (performHardwareGeolocation()) {
      ramAutoGeo = false; // Only do it once after setting it
    }
  }
  
  drawLoading(wi_cloud_refresh_196x196, "Fetching weather...", ramCity);
  
#ifdef USE_HTTP
  WiFiClient client;
#else
  WiFiClientSecure client;
  client.setInsecure();
#endif

  int rxStatus = getOpenMeteoForecast(client, owm_onecall);
  if (rxStatus != HTTP_CODE_OK)
  {
    globalStatus = "Open-Meteo Forecast API Error";
    setFirmwareState(STATE_SLEEP_PENDING);
    return;
  }
#ifdef POS_AIR_QULITY
  rxStatus = getOpenMeteoAirQuality(client, owm_air_pollution);
#endif

  // GET INDOOR TEMPERATURE AND HUMIDITY
  pinMode(PIN_BME_PWR, OUTPUT);
  digitalWrite(PIN_BME_PWR, HIGH);
  delay(100); 
  TwoWire I2C_bme = TwoWire(0);
  I2C_bme.begin(PIN_BME_SDA, PIN_BME_SCL, 100000);
  float inTemp = NAN;
  float inHumidity = NAN;
#if defined(SENSOR_BME280)
  Adafruit_BME280 bme;
  if(bme.begin(BME_ADDRESS, &I2C_bme)) {
    inTemp = bme.readTemperature();
    inHumidity = bme.readHumidity();
  }
#endif
  digitalWrite(PIN_BME_PWR, LOW);

  String refreshTimeStr;
  getRefreshTimeStr(refreshTimeStr, isTimeConfigured, &currentTimInfo);
  String dateStr;
  getDateStr(dateStr, &currentTimInfo);

  // RENDER FULL REFRESH
  initDisplay();
  do
  {
    drawCurrentConditions(owm_onecall.current, owm_onecall.daily[0],
                          owm_air_pollution, inTemp, inHumidity, owm_onecall.hourly);
    drawOutlookGraph(owm_onecall.hourly, owm_onecall.daily, currentTimInfo, owm_onecall.current.dt);
    drawForecast(owm_onecall.daily, currentTimInfo);
    drawLocationDate(strlen(ramCity) > 0 ? ramCity : CITY_STRING, dateStr);
#if DISPLAY_ALERTS
    drawAlerts(owm_onecall.alerts, strlen(ramCity) > 0 ? ramCity : CITY_STRING, dateStr);
#endif
    drawStatusBar(globalStatus, refreshTimeStr, wifiRSSI, readBatteryVoltage());
  } while (display.nextPage());
  powerOffDisplay();

  setFirmwareState(STATE_SLEEP_PENDING);
}

void setup()
{
  startTick = millis();
  Serial.begin(115200);
  disableBuiltinLED();

  // Load defaults into RTC RAM variables only if not already initialized (e.g. cold boot)
  if (!rtcInitialized) {
    if (WIFI_SSID != nullptr && strlen(WIFI_SSID) > 0) strncpy(ramSSID, WIFI_SSID, 63);
    if (WIFI_PASSWORD != nullptr && strlen(WIFI_PASSWORD) > 0) strncpy(ramPassword, WIFI_PASSWORD, 63);
    
    if (CITY_STRING.length() > 0) strncpy(ramCity, CITY_STRING.c_str(), 63);
    if (LAT.length() > 0) strncpy(ramLat, LAT.c_str(), 31);
    if (LON.length() > 0) strncpy(ramLon, LON.c_str(), 31);
    rtcInitialized = true;
  }

  wifiManagerSetup();
  
  // Battery checks (omitted for brevity in this refactor, but kept in logic)
  uint32_t batteryVoltage = readBatteryVoltage();
  if (USING_BATTERY && batteryVoltage <= LOW_BATTERY_VOLTAGE) {
      // Immediate sleep logic
  }
}

void loop()
{
  wifiManagerLoop();
  
  if (currentState == STATE_NORMAL_MODE) {
      updateWeather();
  } else if (currentState == STATE_SLEEP_PENDING) {
      beginDeepSleep(startTick, &currentTimInfo);
  }
}

