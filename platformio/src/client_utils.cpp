/* Client side utilities for esp32-weather-epd.
 * Copyright (C) 2022-2024  Luke Marzen
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

// built-in C++ libraries
#include <cstring>
#include <vector>

// arduino/esp32 libraries
#include <Arduino.h>
#include <esp_sntp.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <time.h>
#include <WiFi.h>

// additional libraries
#include <Adafruit_BusIO_Register.h>
#include <ArduinoJson.h>

// header files
#include "_locale.h"
#include "api_response.h"
#include "aqi.h"
#include "client_utils.h"
#include "config.h"
#include "display_utils.h"
#include "renderer.h"
#ifndef USE_HTTP
  #include <WiFiClientSecure.h>
#endif

#ifdef USE_HTTP
  static const uint16_t OWM_PORT = 80;
#else
  static const uint16_t OWM_PORT = 443;
#endif

/* Power-on and connect WiFi.
 * Takes int parameter to store WiFi RSSI, or “Received Signal Strength
 * Indicator"
 *
 * Returns WiFi status.
 */
wl_status_t startWiFi(int &wifiRSSI)
{
  WiFi.mode(WIFI_STA);
  Serial.printf("[WiFi] Connecting to ESSID: '%s'\n", WIFI_SSID);
  Serial.printf("%s '%s'", TXT_CONNECTING_TO, WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // timeout if WiFi does not connect in WIFI_TIMEOUT ms from now
  unsigned long timeout = millis() + WIFI_TIMEOUT;
  wl_status_t connection_status = WiFi.status();

  while ((connection_status != WL_CONNECTED) && (millis() < timeout))
  {
    Serial.print(".");
    delay(50);
    connection_status = WiFi.status();
  }
  Serial.println();

  if (connection_status == WL_CONNECTED)
  {
    wifiRSSI = WiFi.RSSI(); // get WiFi signal strength now, because the WiFi
                            // will be turned off to save power!
    Serial.printf("[WiFi] Connected to ESSID: '%s'\n", WIFI_SSID);
    Serial.println("IP: " + WiFi.localIP().toString());
  }
  else
  {
    Serial.printf("[WiFi] Failed to connect to ESSID: '%s'\n", WIFI_SSID);
    Serial.printf("%s '%s'\n", TXT_COULD_NOT_CONNECT_TO, WIFI_SSID);
  }
  return connection_status;
} // startWiFi

/* Disconnect and power-off WiFi.
 */
void killWiFi()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
} // killWiFi

/* Prints the local time to serial monitor.
 *
 * Returns true if getting local time was a success, otherwise false.
 */
bool printLocalTime(tm *timeInfo)
{
  int attempts = 0;
  while (!getLocalTime(timeInfo) && attempts++ < 3)
  {
    Serial.println(TXT_FAILED_TO_GET_TIME);
    return false;
  }
  Serial.println(timeInfo, "%A, %B %d, %Y %H:%M:%S");
  return true;
} // printLocalTime

/* Waits for NTP server time sync, adjusted for the time zone specified in
 * config.cpp.
 *
 * Returns true if time was set successfully, otherwise false.
 *
 * Note: Must be connected to WiFi to get time from NTP server.
 */
bool waitForSNTPSync(tm *timeInfo)
{
  // Wait for SNTP synchronization to complete
  unsigned long timeout = millis() + NTP_TIMEOUT;
  if ((sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET)
      && (millis() < timeout))
  {
    Serial.print(TXT_WAITING_FOR_SNTP);
    delay(100); // ms
    while ((sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET)
        && (millis() < timeout))
    {
      Serial.print(".");
      delay(100); // ms
    }
    Serial.println();
  }
  return printLocalTime(timeInfo);
} // waitForSNTPSync

/* Perform an HTTP GET request to Open-Meteo Forecast API
 * If data is received, it will be parsed and stored in the global variable
 * owm_onecall.
 *
 * Returns the HTTP Status Code.
 */
#ifdef USE_HTTP
  int getOpenMeteoForecast(WiFiClient &client, owm_resp_onecall_t &r)
#else
  int getOpenMeteoForecast(WiFiClientSecure &client, owm_resp_onecall_t &r)
#endif
{
  int attempts = 0;
  bool rxSuccess = false;
  DeserializationError jsonErr = {};
  
  // Open-Meteo API endpoint
  String uri = "/v1/forecast"
               "?latitude=" + LAT + 
               "&longitude=" + LON +
               "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
               "is_day,precipitation,rain,showers,snowfall,weather_code,"
               "cloud_cover,pressure_msl,surface_pressure,wind_speed_10m,"
               "wind_direction_10m,wind_gusts_10m,visibility"
               "&hourly=temperature_2m,relative_humidity_2m,apparent_temperature,"
               "precipitation_probability,weather_code,cloud_cover,surface_pressure,"
               "wind_speed_10m,wind_direction_10m,wind_gusts_10m,is_day"
               "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
               "sunrise,sunset,precipitation_probability_max,precipitation_sum"
               "&timezone=auto"
               "&forecast_days=8";

  String fullUri = "https://" + OPENMETEO_ENDPOINT + uri;
  Serial.println("[Open-Meteo API] Request: " + fullUri);
  Serial.flush();
  
  int httpResponse = 0;
  while (!rxSuccess && attempts < 3)
  {
    wl_status_t connection_status = WiFi.status();
    if (connection_status != WL_CONNECTED)
    {
      // -512 offset distinguishes these errors from httpClient errors
      return -512 - static_cast<int>(connection_status);
    }

    HTTPClient http;
    http.setConnectTimeout(HTTP_CLIENT_TCP_TIMEOUT); // default 5000ms
    http.setTimeout(HTTP_CLIENT_TCP_TIMEOUT); // default 5000ms
    http.begin(client, OPENMETEO_ENDPOINT, OPENMETEO_PORT, uri);
    
    Serial.println("  [HTTP] GET attempt " + String(attempts + 1) + "/3...");
    httpResponse = http.GET();
    Serial.println("  [HTTP] Response code: " + String(httpResponse));
    
    if (httpResponse == HTTP_CODE_OK)
    {
      jsonErr = deserializeOpenMeteo(http.getStream(), r);
      if (jsonErr)
      {
        // -256 offset distinguishes these errors from httpClient errors
        httpResponse = -256 - static_cast<int>(jsonErr.code());
        Serial.println("  [JSON] Parse error: " + String(jsonErr.c_str()));
      }
      else
      {
        rxSuccess = true;
        Serial.println("  [HTTP] Success!");
      }
      http.end();
      client.stop();
      Serial.println("  " + String(httpResponse, DEC) + " "
                     + getHttpResponsePhrase(httpResponse));
      return httpResponse;
    }
    http.end();
    
    Serial.println("  " + String(httpResponse, DEC) + " "
                   + getHttpResponsePhrase(httpResponse));
    
    if (attempts < 2)
    {
      Serial.println("  [HTTP] Retrying in 500ms...");
      delay(500);
    }
    
    ++attempts;
  }

  return httpResponse;
} // getOpenMeteoForecast

/* Perform an HTTP GET request to Open-Meteo Air Quality API
 * If data is received, it will be parsed and stored in the global variable
 * owm_air_pollution.
 *
 * Returns the HTTP Status Code.
 */
#ifdef USE_HTTP
  int getOpenMeteoAirQuality(WiFiClient &client, owm_resp_air_pollution_t &r)
#else
  int getOpenMeteoAirQuality(WiFiClientSecure &client, owm_resp_air_pollution_t &r)
#endif
{
  int attempts = 0;
  bool rxSuccess = false;
  DeserializationError jsonErr = {};

  // Open-Meteo Air Quality API endpoint
  String uri = "/v1/air-quality"
               "?latitude=" + LAT + 
               "&longitude=" + LON +
               "&hourly=pm10,pm2_5,carbon_monoxide,nitrogen_dioxide,"
               "sulphur_dioxide,ozone"
               "&timezone=auto";

  String fullUri = "https://" + OPENMETEO_AIRQUALITY_ENDPOINT + uri;
  Serial.println("[Open-Meteo Air Quality API] Request: " + fullUri);
  Serial.flush();
  
  int httpResponse = 0;
  while (!rxSuccess && attempts < 3)
  {
    wl_status_t connection_status = WiFi.status();
    if (connection_status != WL_CONNECTED)
    {
      // -512 offset distinguishes these errors from httpClient errors
      return -512 - static_cast<int>(connection_status);
    }

    HTTPClient http;
    http.setConnectTimeout(HTTP_CLIENT_TCP_TIMEOUT); // default 5000ms
    http.setTimeout(HTTP_CLIENT_TCP_TIMEOUT); // default 5000ms
    http.begin(client, OPENMETEO_AIRQUALITY_ENDPOINT, OPENMETEO_PORT, uri);
    
    Serial.println("  [HTTP] GET attempt " + String(attempts + 1) + "/3...");
    httpResponse = http.GET();
    Serial.println("  [HTTP] Response code: " + String(httpResponse));
    
    if (httpResponse == HTTP_CODE_OK)
    {
      jsonErr = deserializeOpenMeteoAirQuality(http.getStream(), r);
      if (jsonErr)
      {
        // -256 offset to distinguishes these errors from httpClient errors
        httpResponse = -256 - static_cast<int>(jsonErr.code());
        Serial.println("  [JSON] Parse error: " + String(jsonErr.c_str()));
      }
      else
      {
        rxSuccess = true;
        Serial.println("  [HTTP] Success!");
      }
      http.end();
      client.stop();
      Serial.println("  " + String(httpResponse, DEC) + " "
                     + getHttpResponsePhrase(httpResponse));
      return httpResponse;
    }
    http.end();
    
    Serial.println("  " + String(httpResponse, DEC) + " "
                   + getHttpResponsePhrase(httpResponse));
    
    if (attempts < 2)
    {
      Serial.println("  [HTTP] Retrying in 500ms...");
      delay(500);
    }
    
    ++attempts;
  }

  return httpResponse;
} // getOpenMeteoAirQuality

/* Prints debug information about heap usage.
 */
void printHeapUsage() {
  Serial.println("[debug] Heap Size       : "
                 + String(ESP.getHeapSize()) + " B");
  Serial.println("[debug] Available Heap  : "
                 + String(ESP.getFreeHeap()) + " B");
  Serial.println("[debug] Min Free Heap   : "
                 + String(ESP.getMinFreeHeap()) + " B");
  Serial.println("[debug] Max Allocatable : "
                 + String(ESP.getMaxAllocHeap()) + " B");
  return;
}

/*
 * Open-Meteo API support functions
 */

/* Get current weather only - small JSON (~2KB) */
int getOpenMeteoCurrent(WiFiClientSecure &client, owm_current_t &current)
{
  String uri = "/v1/forecast"
               "?latitude=" + LAT + 
               "&longitude=" + LON +
               "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,cloud_cover,surface_pressure,wind_speed_10m,wind_direction_10m,wind_gusts_10m,uv_index,visibility,dew_point_2m,rain,snowfall" +
               "&timezone=Europe/Paris";

  
  int attempts = 0;
  while (attempts < 3) {
    if (WiFi.status() != WL_CONNECTED) return -512 - static_cast<int>(WiFi.status());
    
    HTTPClient http;
    http.setConnectTimeout(HTTP_CLIENT_TCP_TIMEOUT);
    http.setTimeout(HTTP_CLIENT_TCP_TIMEOUT);
    http.begin(client, OPENMETEO_ENDPOINT, OPENMETEO_PORT, uri);
    
    int httpResponse = http.GET();
    if (httpResponse == HTTP_CODE_OK) {
      String jsonString = "";
      WiFiClient *stream = http.getStreamPtr();
      while (stream->available()) {
        char c = stream->read();
        if (c >= 0) jsonString += c;
        if (jsonString.length() > 5000) break;
      }
      
      int jsonStart = jsonString.indexOf('{');
      if (jsonStart >= 0) {
        jsonString = jsonString.substring(jsonStart);
        int lastBrace = jsonString.lastIndexOf('}');
        if (lastBrace > 0) jsonString = jsonString.substring(0, lastBrace + 1);
      }
      
      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, jsonString);
      
      if (!error || doc.size() > 0) {
        JsonObject curr = doc["current"];
        current.dt = parseIso8601(curr["time"]);
        current.temp = curr["temperature_2m"].as<float>();
        current.feels_like = curr["apparent_temperature"].as<float>();
        current.humidity = curr["relative_humidity_2m"].as<int>();
        current.pressure = curr["surface_pressure"].as<int>();
        current.dew_point = curr["dew_point_2m"].as<float>();
        current.clouds = curr["cloud_cover"].as<int>();
        current.uvi = curr["uv_index"].as<float>();
        current.visibility = curr["visibility"].as<int>();
        current.wind_speed = curr["wind_speed_10m"].as<float>();
        current.wind_gust = curr["wind_gusts_10m"].as<float>();
        current.wind_deg = curr["wind_direction_10m"].as<int>();
        current.rain_1h = curr["rain"].as<float>();
        current.snow_1h = curr["snowfall"].as<float>() * 10.0f;
        int wmo = curr["weather_code"].as<int>();
        wmoToOwmWeather(wmo, true, current.weather);
        current.sunrise = 0;
        current.sunset = 0;
        http.end();
        client.stop();
        return HTTP_CODE_OK;
      }
    }
    http.end();
    client.stop();
    ++attempts;
    delay(100);
  }
  return -1;
}

/* Get daily forecast - medium JSON (~3KB) */
int getOpenMeteoDaily(WiFiClientSecure &client, owm_daily_t *daily, int &count)
{
  Serial.println("DEBUG: getOpenMeteoDaily started");
  
  String uri = "/v1/forecast"
               "?latitude=" + LAT + 
               "&longitude=" + LON +
               "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,uv_index_max,precipitation_sum,precipitation_probability_max,wind_speed_10m_max" +
               "&timezone=Europe/Paris" +
               "&forecast_days=" + String(OWM_NUM_DAILY);
  
  int attempts = 0;
  while (attempts < 3) {
    if (WiFi.status() != WL_CONNECTED) return -512 - static_cast<int>(WiFi.status());
    
    HTTPClient http;
    http.setConnectTimeout(HTTP_CLIENT_TCP_TIMEOUT);
    http.setTimeout(HTTP_CLIENT_TCP_TIMEOUT);
    http.begin(client, OPENMETEO_ENDPOINT, OPENMETEO_PORT, uri);
    
    int httpResponse = http.GET();
    if (httpResponse == HTTP_CODE_OK) {
      String jsonString = "";
      WiFiClient *stream = http.getStreamPtr();
      while (stream->available()) {
        char c = stream->read();
        if (c >= 0) jsonString += c;
        if (jsonString.length() > 8000) break;
      }
      
      int jsonStart = jsonString.indexOf('{');
      if (jsonStart >= 0) {
        jsonString = jsonString.substring(jsonStart);
        int lastBrace = jsonString.lastIndexOf('}');
        if (lastBrace > 0) jsonString = jsonString.substring(0, lastBrace + 1);
      }
      
      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, jsonString);
      
      if (!error || doc.size() > 0) {
        JsonObject daily_obj = doc["daily"];
        JsonArray daily_time = daily_obj["time"];
        JsonArray daily_max = daily_obj["temperature_2m_max"];
        JsonArray daily_min = daily_obj["temperature_2m_min"];
        JsonArray daily_sunrise = daily_obj["sunrise"];
        JsonArray daily_sunset = daily_obj["sunset"];
        JsonArray daily_uv = daily_obj["uv_index_max"];
        JsonArray daily_rain = daily_obj["precipitation_sum"];
        JsonArray daily_pop = daily_obj["precipitation_probability_max"];
        JsonArray daily_wind = daily_obj["wind_speed_10m_max"];
        JsonArray daily_weather = daily_obj["weather_code"];
        
        count = min((int)OWM_NUM_DAILY, (int)daily_time.size());
        for (int i = 0; i < count; i++) {
          daily[i].dt = parseIso8601(daily_time[i]);
          daily[i].sunrise = parseIso8601(daily_sunrise[i]);
          daily[i].sunset = parseIso8601(daily_sunset[i]);
          daily[i].temp.max = daily_max[i].as<float>();
          daily[i].temp.min = daily_min[i].as<float>();
          daily[i].temp.morn = daily[i].temp.min;
          daily[i].temp.day = (daily[i].temp.max + daily[i].temp.min) / 2.0f;
          daily[i].temp.eve = daily[i].temp.max;
          daily[i].temp.night = daily[i].temp.min;
          daily[i].uvi = daily_uv[i].as<float>();
          daily[i].rain = daily_rain[i].as<float>();
          daily[i].pop = daily_pop[i].as<float>() / 100.0f;
          daily[i].wind_speed = daily_wind[i].as<float>();
          int wmo = daily_weather[i].as<int>();
          wmoToOwmWeather(wmo, true, daily[i].weather);
          daily[i].pressure = 0;
          daily[i].humidity = 0;
          daily[i].moonrise = 0;
          daily[i].moonset = 0;
          daily[i].moon_phase = 0;
        }
        http.end();
        client.stop();
        return HTTP_CODE_OK;
      }
    }
    http.end();
    client.stop();
    ++attempts;
    delay(100);
  }
  return -1;
}

/* Get hourly forecast - limited to 48h (~5KB) */
int getOpenMeteoHourly(WiFiClientSecure &client, owm_hourly_t *hourly, int &count)
{
  Serial.println("DEBUG: getOpenMeteoHourly started");
  
  String uri = "/v1/forecast"
               "?latitude=" + LAT + 
               "&longitude=" + LON +
               "&hourly=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation_probability,weather_code,cloud_cover,surface_pressure,wind_speed_10m,wind_direction_10m,wind_gusts_10m,uv_index,visibility,dew_point_2m" +
               "&timezone=Europe/Paris" +
               "&forecast_days=2";
  
  int attempts = 0;
  while (attempts < 3) {
    if (WiFi.status() != WL_CONNECTED) return -512 - static_cast<int>(WiFi.status());
    
    HTTPClient http;
    http.setConnectTimeout(HTTP_CLIENT_TCP_TIMEOUT);
    http.setTimeout(HTTP_CLIENT_TCP_TIMEOUT);
    http.begin(client, OPENMETEO_ENDPOINT, OPENMETEO_PORT, uri);
    
    int httpResponse = http.GET();
    if (httpResponse == HTTP_CODE_OK) {
      String jsonString = "";
      WiFiClient *stream = http.getStreamPtr();
      while (stream->available()) {
        char c = stream->read();
        if (c >= 0) jsonString += c;
        if (jsonString.length() > 10000) break;
      }
      
      int jsonStart = jsonString.indexOf('{');
      if (jsonStart >= 0) {
        jsonString = jsonString.substring(jsonStart);
        int lastBrace = jsonString.lastIndexOf('}');
        if (lastBrace > 0) jsonString = jsonString.substring(0, lastBrace + 1);
      }
      
      DynamicJsonDocument doc(8192);
      DeserializationError error = deserializeJson(doc, jsonString);
      
      if (!error || doc.size() > 0) {
        JsonObject hourly_obj = doc["hourly"];
        JsonArray hourly_time = hourly_obj["time"];
        JsonArray hourly_temp = hourly_obj["temperature_2m"];
        JsonArray hourly_feels = hourly_obj["apparent_temperature"];
        JsonArray hourly_humidity = hourly_obj["relative_humidity_2m"];
        JsonArray hourly_pressure = hourly_obj["surface_pressure"];
        JsonArray hourly_clouds = hourly_obj["cloud_cover"];
        JsonArray hourly_uv = hourly_obj["uv_index"];
        JsonArray hourly_visibility = hourly_obj["visibility"];
        JsonArray hourly_dew = hourly_obj["dew_point_2m"];
        JsonArray hourly_wind_speed = hourly_obj["wind_speed_10m"];
        JsonArray hourly_wind_gust = hourly_obj["wind_gusts_10m"];
        JsonArray hourly_wind_deg = hourly_obj["wind_direction_10m"];
        JsonArray hourly_pop = hourly_obj["precipitation_probability"];
        JsonArray hourly_weather = hourly_obj["weather_code"];
        
        count = min((int)OWM_NUM_HOURLY, (int)hourly_time.size());
        for (int i = 0; i < count; i++) {
          hourly[i].dt = parseIso8601(hourly_time[i]);
          hourly[i].temp = hourly_temp[i].as<float>();
          hourly[i].feels_like = hourly_feels[i].as<float>();
          hourly[i].humidity = hourly_humidity[i].as<int>();
          hourly[i].pressure = hourly_pressure[i].as<int>();
          hourly[i].clouds = hourly_clouds[i].as<int>();
          hourly[i].uvi = hourly_uv[i].as<float>();
          hourly[i].visibility = hourly_visibility[i].as<int>();
          hourly[i].dew_point = hourly_dew[i].as<float>();
          hourly[i].wind_speed = hourly_wind_speed[i].as<float>();
          hourly[i].wind_gust = hourly_wind_gust[i].as<float>();
          hourly[i].wind_deg = hourly_wind_deg[i].as<int>();
          hourly[i].pop = hourly_pop[i].as<float>() / 100.0f;
          hourly[i].rain_1h = 0;
          hourly[i].snow_1h = 0;
          int wmo = hourly_weather[i].as<int>();
          time_t t = hourly[i].dt;
          struct tm *tm_info = localtime(&t);
          bool isDay = (tm_info->tm_hour >= 6 && tm_info->tm_hour < 18);
          wmoToOwmWeather(wmo, isDay, hourly[i].weather);
        }
        http.end();
        client.stop();
        return HTTP_CODE_OK;
      }
    }
    http.end();
    client.stop();
    ++attempts;
    delay(100);
  }
  return -1;
}

/* Main function: Get all Open-Meteo data in 3 separate requests */
int getOpenMeteoForecast(WiFiClientSecure &client, owm_resp_onecall_t &r)
{
  Serial.println("DEBUG: getOpenMeteoForecast started (3 requests)");
  
  int status = getOpenMeteoCurrent(client, r.current);
  if (status != HTTP_CODE_OK) {
    Serial.println("DEBUG: getOpenMeteoCurrent failed: " + String(status));
    return status;
  }
  Serial.println("DEBUG: Current weather OK");
  
  int dailyCount = 0;
  status = getOpenMeteoDaily(client, r.daily, dailyCount);
  if (status != HTTP_CODE_OK) {
    Serial.println("DEBUG: getOpenMeteoDaily failed: " + String(status));
    return status;
  }
  Serial.println("DEBUG: Daily forecast OK, count=" + String(dailyCount));
  
  int hourlyCount = 0;
  status = getOpenMeteoHourly(client, r.hourly, hourlyCount);
  if (status != HTTP_CODE_OK) {
    Serial.println("DEBUG: getOpenMeteoHourly failed: " + String(status));
    return status;
  }
  Serial.println("DEBUG: Hourly forecast OK, count=" + String(hourlyCount));
  
  // Update current sunrise/sunset from first daily
  if (dailyCount > 0) {
    r.current.sunrise = r.daily[0].sunrise;
    r.current.sunset = r.daily[0].sunset;
  }
  
  // Set location data
  r.lat = LAT.toFloat();
  r.lon = LON.toFloat();
  r.timezone = "Europe/Paris";
  r.timezone_offset = 3600;
  r.alerts.clear();
  
  Serial.println("DEBUG: All Open-Meteo data fetched successfully");
  return HTTP_CODE_OK;
} // end getOpenMeteoForecast

