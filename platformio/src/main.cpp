/// @file main.cpp
/// @brief Main application entry point and weather update orchestration
/// @copyright Copyright (C) 2022-2025 Luke Marzen, 2026 Mauro Freitas
/// @license GNU General Public License v3.0
///
/// @details
/// ESP32 Weather E-Paper Display main application. Implements:
/// - Hardware initialization and power management
/// - State machine integration with WiFi manager
/// - Weather data acquisition (mock, saved, or live API)
/// - Sensor reading (BME280/BME680)
/// - Display rendering with paged mode
/// - Deep sleep scheduling for battery operation
///
/// Application flow:
/// 1. setup(): Initialize hardware, load RTC RAM, check battery
/// 2. loop(): Execute WiFi manager state machine
/// 3. updateWeather(): Fetch data, read sensors, render display
/// 4. beginDeepSleep(): Calculate sleep duration, enter ultra-low-power mode

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
#include "failure_handler.h"
#include "icons/icons_196x196.h"
#include "renderer.h"
#include "wifi_manager.h"
#include "time_coordinator.h"

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

/// @brief Global weather data structure (static to avoid stack overflow)
/// @details OWM_NUM_HOURLY * hourly struct size + OWM_NUM_DAILY * daily struct size
/// exceeds safe stack limits for ESP32 task. Static allocation prevents stack overflow.
static owm_resp_onecall_t       owm_onecall;

/// @brief Global air quality data structure (static allocation)
static owm_resp_air_pollution_t owm_air_pollution;

/// @brief NVS preferences for persistent storage
Preferences prefs;

/// @brief Application start time (millis())
/// @details Used to calculate total awake time for display
unsigned long startTick = 0;

/// @brief Current time information structure
tm currentTimInfo = {};

/// @brief Flag indicating successful NTP time synchronization
bool isTimeConfigured = false;

/// @brief Global status string for status bar
String globalStatus = {};

/// @brief Temporary string for formatting operations
String globalTmpStr = {};

/// @brief Time coordinator instance (Single Source of Truth)
/// @details Centralizes all timezone and time formatting logic
TimeCoordinator timeCoord;

/// @brief Calculated deep sleep duration in seconds
/// @details Set by TimeCoordinator::process(), consumed by beginDeepSleep()
uint64_t calculatedSleepDuration = 0;

/// @brief Fetch weather data and update display
/// @details Main weather update orchestration function
void updateWeather();

/// @brief Populate weather structures with synthetic test data
/// @param owm_onecall Weather data structure to populate
/// @param timeInfo Time structure to initialize
/// @details Generates realistic weather data for testing without WiFi/API.
/// Scenario selection via MOCKUP_CURRENT_WEATHER and MOCKUP_RAIN_WIDGET_STATE.
/// Temperature values kept below 100°C for 2-digit display layout.
///
/// @note Initializes only used fields individually to avoid 171+ String constructor
/// calls from aggregate initialization (stack overflow prevention).
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

  // Timezone offset for TimeCoordinator (UTC base for mock data)
  owm_onecall.timezone_offset = 0;

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
  // In AUTO mode, force UTC as the base timezone because we apply the API offset manually.
  // In MANUAL mode, use the user-selected timezone from RTC RAM or compile-time default.
  const char* tzToUse;
  if (ramTimezoneMode == TIMEZONE_MODE_MANUAL && ramTimezone[0] != '\0') {
    tzToUse = ramTimezone;
  } else {
    tzToUse = "UTC0";
  }
  setenv("TZ", tzToUse, 1);
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
  
  // For mockup data, use default air quality values
  for (int i = 0; i < OWM_NUM_AIR_POLLUTION; i++) {
    owm_air_pollution.main_aqi[i] = 1;
    owm_air_pollution.components.pm2_5[i] = 10.0f;
    owm_air_pollution.components.pm10[i] = 20.0f;
    owm_air_pollution.components.o3[i] = 30.0f;
    owm_air_pollution.components.no2[i] = 15.0f;
    owm_air_pollution.components.so2[i] = 5.0f;
  }
  
  Serial.println("Mockup data filled successfully!");
}

/// @brief Enter ultra-low-power deep sleep mode
/// @param startTime Application start time (millis())
/// @param sleepDurationSeconds Time to sleep in seconds
/// @details Configures ESP32 deep sleep timer and enters hibernation.
/// Current consumption: <11μA during sleep.
///
/// @warning Must call powerOffDisplay() before this function.
/// @note Wake up causes ESP32 reset; execution restarts from setup().
void beginDeepSleep(unsigned long startTime, uint64_t sleepDurationSeconds)
{
#if DEBUG_LEVEL >= 1
  printHeapUsage();
#endif

  // Ensure all serial output is flushed before sleeping
  Serial.flush();
  
  // Small delay to allow background tasks to complete and prevent watchdog issues
  delay(100);

  esp_sleep_enable_timer_wakeup(sleepDurationSeconds * 1000000ULL);
  Serial.print(TXT_AWAKE_FOR);
  Serial.println(" "  + String((millis() - startTime) / 1000.0, 3) + "s");
  Serial.print(TXT_ENTERING_DEEP_SLEEP_FOR);
  Serial.println(" " + String(sleepDurationSeconds) + "s");
  
  // Flush again before the final sleep call
  Serial.flush();
  
  esp_deep_sleep_start();
} // end beginDeepSleep

/// @brief Program entry point
/// @details
/// 1. Initialize serial and disable LED
/// 2. Load RTC RAM variables (WiFi creds, location)
/// 3. Check battery voltage (hibernate if critical)
/// 4. Initialize WiFi manager state machine
///
/// @note This function runs after every wake from deep sleep (reset).

/// @brief Execute complete weather update cycle
/// @details
/// 1. Determine data source (mock, saved header, or live API)
/// 2. Fetch weather and air quality data
/// 3. Synchronize time via NTP (if not mock mode)
/// 4. Read indoor sensor (BME280/BME680)
/// 5. Process time data through TimeCoordinator
/// 6. Render full display with all widgets
/// 7. Calculate and store sleep duration
/// 8. Transition to STATE_SLEEP_PENDING
///
/// @note Error handling through handleFailure() with retry logic
void updateWeather()
{
#if USE_MOCKUP_DATA
  // ═══════════════════════════════════════════════════════════════════════════
  // MOCK MODE: Simulated network stack (no WiFi, no NTP, no API calls)
  // ═══════════════════════════════════════════════════════════════════════════
  int wifiRSSI = -55;  // Mock signal strength for status bar

  // Skip NTP sync - fillMockupData() sets the internal clock via settimeofday()
  isTimeConfigured = true;
  // Skip geolocation - mock data already has fixed coordinates
#else
  // ═══════════════════════════════════════════════════════════════════════════
  // PRODUCTION MODE: Full network stack (unchanged from original)
  // ═══════════════════════════════════════════════════════════════════════════
  int wifiRSSI = 0;
  if (WiFi.status() == WL_CONNECTED) {
      wifiRSSI = WiFi.RSSI();
  }

  // TIME SYNCHRONIZATION
  // In AUTO mode, force UTC as the base timezone because we apply the API offset manually.
  // In MANUAL mode, use the user-selected timezone from RTC RAM or compile-time default.
  const char* tzToUse;
  if (ramTimezoneMode == TIMEZONE_MODE_MANUAL && ramTimezone[0] != '\0') {
    tzToUse = ramTimezone;
  } else {
    tzToUse = "UTC0";
  }
  configTzTime(tzToUse, NTP_SERVER_1, NTP_SERVER_2);
  isTimeConfigured = waitForSNTPSync(&currentTimInfo);
  if (!isTimeConfigured)
  {
    handleFailure(FAILURE_NTP, TXT_TIME_SYNCHRONIZATION_FAILED);
    return;
  }
  
  // NTP Success: Reset counter
  if (ntpFailCycles > 0) {
    Serial.printf("[NTP] Success! Reset counter %d->0\n", ntpFailCycles);
    ntpFailCycles = 0;
  }

  // MAKE API REQUESTS
  if (ramAutoGeo) {
    if (locateByIpAddress()) {
      ramAutoGeo = false; // Only do it once after setting it
    }
  }
#endif

  // ═══════════════════════════════════════════════════════════════════════════
  // LOADING SCREEN: Identical for both mock and production
  // ═══════════════════════════════════════════════════════════════════════════
  if (isFirstBoot || !SILENT_STATUS) {
    drawLoading(wi_cloud_refresh_196x196, "Fetching weather...", ramCity);
  }
  
// DATA SOURCE SELECTION
// Three options for getting weather data (configured in config.h):
// 1. USE_MOCKUP_DATA - Synthetic data generated in code (no WiFi needed)
// 2. USE_SAVED_API_DATA - Load from include/saved_api_response.h (no WiFi needed)
// 3. Default - Fetch from Open-Meteo API (requires WiFi)

#if USE_MOCKUP_DATA
  // Option 1: Use synthetic mockup data with simulated latency
  delay(1500);  // Simulate network fetch time
  Serial.println("[Data Source] Using MOCKUP DATA (no WiFi/API calls)");
  fillMockupData(owm_onecall, currentTimInfo);
#elif USE_SAVED_API_DATA
  // Option 2: Load from saved header file
  Serial.println("[Data Source] Loading from saved_api_response.h");
  DeserializationError error = loadOpenMeteoFromHeader(owm_onecall);
  if (error) {
    globalStatus = "Header Load Error: " + String(error.c_str());
    Serial.println("[Data Source] ERROR: " + globalStatus);
    setFirmwareState(STATE_SLEEP_PENDING);
    return;
  }
  #ifdef POS_AIR_QULITY
    error = loadOpenMeteoAirQualityFromHeader(owm_air_pollution);
    if (error) {
      Serial.println("[Data Source] Air quality load failed, using defaults");
      // Already filled with defaults in the function
    }
  #endif

#else
  #ifdef USE_HTTP
    WiFiClient client;
  #else
    WiFiClientSecure client;
    client.setInsecure();
  #endif

  int rxStatus = getOpenMeteoForecast(client, owm_onecall);
  if (rxStatus != HTTP_CODE_OK)
  {
    String detail = String(rxStatus) + ": " + getHttpResponsePhrase(rxStatus);
    handleFailure(FAILURE_API, "Open-Meteo Forecast API Error", detail);
    return;
  }
  
  // API Success: Reset counter
  if (apiFailCycles > 0) {
    Serial.printf("[API] Success! Reset counter %d->0\n", apiFailCycles);
    apiFailCycles = 0;
  }
  
  #ifdef POS_AIR_QULITY
    rxStatus = getOpenMeteoAirQuality(client, owm_air_pollution);
  #endif
#endif // Data source selection

  // TimeCoordinator: Single Source of Truth for all time processing
  timeCoord.begin();
  TimeDisplayData timeData = timeCoord.process(owm_onecall, startTick);

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

  // RENDER FULL REFRESH
  initDisplay();
  do
  {
    drawCurrentConditions(owm_onecall.current, owm_onecall.daily[0],
                          owm_air_pollution, inTemp, inHumidity, owm_onecall.hourly,
                          timeData.sunriseTime, timeData.sunsetTime,
                          timeData.moonriseTime, timeData.moonsetTime,
                          timeData.rainTime);
    drawOutlookGraph(owm_onecall.hourly, owm_onecall.daily, 
                     timeData.hourlyLabels, timeData.hourlyStartIndex);
    drawForecast(owm_onecall.daily, timeData.todayDayOfWeek);
    String locationStr = String(strlen(ramCity) > 0 ? ramCity : CITY_STRING) + " - " + 
                         String(strlen(ramCountry) > 0 ? ramCountry : COUNTRY_STRING);
    drawLocationDate(locationStr, timeData.displayDate);
#if DISPLAY_ALERTS
    drawAlerts(owm_onecall.alerts, locationStr, timeData.displayDate);
#endif
    drawStatusBar(globalStatus, timeData.updateTime, wifiRSSI, readBatteryVoltage());
  } while (display.nextPage());
  powerOffDisplay();

  // Success - reset all counters
  if (connectionFailCycles > 0) {
    Serial.printf("[WiFi] Reset counter %d->0\n", connectionFailCycles);
    connectionFailCycles = 0;
  }
  if (ntpFailCycles > 0) {
    Serial.printf("[NTP] Reset counter %d->0\n", ntpFailCycles);
    ntpFailCycles = 0;
  }
  if (apiFailCycles > 0) {
    Serial.printf("[API] Reset counter %d->0\n", apiFailCycles);
    apiFailCycles = 0;
  }
  
  calculatedSleepDuration = timeData.sleepDurationSeconds;
  setFirmwareState(STATE_SLEEP_PENDING);
}

/// @brief Arduino framework setup function
void setup()
{
  startTick = millis();
  Serial.begin(115200);
  disableBuiltinLED();

  // Load defaults into RTC RAM variables only if not already initialized (e.g. cold boot)
  // All strncpy calls use sizeof(dest) - 1 to ensure null-termination
  
  // Check for RTC memory corruption: rtcInitialized is true but credentials are empty.
  // This happens after watchdog resets or power glitches during deep sleep.
  bool rtcCorrupted = (rtcInitialized && strlen(ramSSID) == 0);
  if (rtcCorrupted) {
    Serial.println("[ERROR] RTC memory corrupted! WiFi credentials lost.");
    Serial.println("[INFO] Please reconfigure WiFi via the setup portal.");
    // Clear rtcInitialized so defaults are loaded as fallback,
    // but we will force AP mode afterward to ensure user knows reconfiguration is needed.
    rtcInitialized = false;
  }
  
  if (!rtcInitialized) {
    if (WIFI_SSID != nullptr && strlen(WIFI_SSID) > 0) {
      strncpy(ramSSID, WIFI_SSID, sizeof(ramSSID) - 1);
      ramSSID[sizeof(ramSSID) - 1] = '\0';
    }
    if (WIFI_PASSWORD != nullptr && strlen(WIFI_PASSWORD) > 0) {
      strncpy(ramPassword, WIFI_PASSWORD, sizeof(ramPassword) - 1);
      ramPassword[sizeof(ramPassword) - 1] = '\0';
    }
    if (CITY_STRING.length() > 0) {
      strncpy(ramCity, CITY_STRING.c_str(), sizeof(ramCity) - 1);
      ramCity[sizeof(ramCity) - 1] = '\0';
    }
    if (LAT.length() > 0) {
      strncpy(ramLat, LAT.c_str(), sizeof(ramLat) - 1);
      ramLat[sizeof(ramLat) - 1] = '\0';
    }
    if (LON.length() > 0) {
      strncpy(ramLon, LON.c_str(), sizeof(ramLon) - 1);
      ramLon[sizeof(ramLon) - 1] = '\0';
    }
    if (COUNTRY_STRING.length() > 0) {
      strncpy(ramCountry, COUNTRY_STRING.c_str(), sizeof(ramCountry) - 1);
      ramCountry[sizeof(ramCountry) - 1] = '\0';
    }
    rtcInitialized = true;
  }
  // Ensure ramCountry is set if empty (fallback)
  if (strlen(ramCountry) == 0 && COUNTRY_STRING.length() > 0) {
    strncpy(ramCountry, COUNTRY_STRING.c_str(), sizeof(ramCountry) - 1);
    ramCountry[sizeof(ramCountry) - 1] = '\0';
  }
  
  // If RTC was corrupted, force AP mode so user can reconfigure
  // (don't silently use defaults - they may be wrong/outdated)
  if (rtcCorrupted) {
    // Clear the credentials we just loaded to force AP mode
    ramSSID[0] = '\0';
    ramPassword[0] = '\0';
  }

  // ============================================================
  // BATTERY CHECK (ORIGINAL BEHAVIOR - Single threshold, no retries)
  // ============================================================
  // If battery is low, show error once and hibernate indefinitely.
  // Only a manual reset or recharge can wake the device.
  // ============================================================
#if BATTERY_MONITORING
  prefs.begin("storage", false);
  
  uint32_t batteryVoltage = readBatteryVoltage();
  Serial.print(TXT_BATTERY_VOLTAGE);
  Serial.println(": " + String(batteryVoltage) + "mv");
  
  // Load persisted low battery state
  bool lowBat = prefs.getBool("lowBat", false);
  
  // LOW BATTERY DETECTED (Single threshold - ORIGINAL BEHAVIOR)
  if (batteryVoltage <= LOW_BATTERY_VOLTAGE)
  {
    if (!lowBat) {
      // FIRST TIME: Show error screen and persist state
      prefs.putBool("lowBat", true);
      prefs.end();  // Close NVS before sleeping
      
      // Show low battery error screen
      initDisplay();
      do {
        drawError(battery_alert_0deg_196x196, TXT_LOW_BATTERY);
      } while (display.nextPage());
      powerOffDisplay();
      
      Serial.println(TXT_LOW_BATTERY_VOLTAGE);
    } else {
      // ALREADY ALERTED: Just log and sleep
      prefs.end();
      Serial.println("[BATTERY] Low battery condition persists, hibernating...");
    }
    
    // ORIGINAL: Hibernate indefinitely (NO timer wakeup)
    // Device will NOT wake up automatically. Requires manual reset or recharge.
    esp_deep_sleep_start();
  }
  
  // BATTERY RECOVERED (voltage back to normal)
  if (batteryVoltage > LOW_BATTERY_VOLTAGE && lowBat) {
    Serial.println("[BATTERY] Battery recovered, resetting flag");
    prefs.putBool("lowBat", false);
  }
  
  prefs.end();  // Close NVS if not already closed
  
#else
  // USB MODE: Fake full battery
#endif

  wifiManagerSetup();
}

/// @brief Arduino framework main loop
/// @details Non-blocking state machine execution:
/// - STATE_NORMAL_MODE: Call updateWeather()
/// - STATE_SLEEP_PENDING: Enter deep sleep
/// - STATE_ERROR: Enter permanent hibernation (manual reset required)
void loop()
{
  wifiManagerLoop();
  
  if (currentState == STATE_NORMAL_MODE) {
      updateWeather();
  } else if (currentState == STATE_SLEEP_PENDING) {
      // Use sleep duration calculated by TimeCoordinator (no recalculation!)
      beginDeepSleep(startTick, calculatedSleepDuration);
  } else if (currentState == STATE_ERROR) {
      // Indefinite deep sleep - device will not wake up automatically
      // User must manually reset or power cycle to recover
      Serial.println("ERROR_CONNECTION: Entering indefinite deep sleep.");
      Serial.println("Manual reset required to recover.");
      
      // Note: Error screen was displayed by wifi_manager.cpp before transitioning.
      // Wait for e-paper to complete physical refresh before sleeping.
      delay(2000);  // 2 seconds for display to stabilize
      
      // Sleep indefinitely - disable ALL wakeup sources
      // Only EXT0/EXT1 (GPIO) or manual reset can wake the device now
      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
      
      // Ensure no GPIO wakeup is configured
      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TOUCHPAD);
      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ULP);
      
      Serial.println("All automatic wakeups disabled.");
      Serial.flush();
      
      esp_deep_sleep_start();
      
      // Should never reach here
      Serial.println("ERROR: Deep sleep failed!");
  }
}

