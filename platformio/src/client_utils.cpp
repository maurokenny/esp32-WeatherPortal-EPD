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
#if DEBUG_LEVEL >= 2
  Serial.printf("[WiFi] Connecting to ESSID: '%s'\n", WIFI_SSID);
#endif
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
               "&current=temperature_2m,apparent_temperature,is_day,weather_code,"
               "precipitation,rain,showers,snowfall";

#ifdef POS_HUMIDITY
  uri += ",relative_humidity_2m";
#endif
#ifdef POS_WIND
  uri += ",wind_speed_10m,wind_direction_10m,wind_gusts_10m";
#endif
#ifdef POS_PRESSURE
  uri += ",pressure_msl,surface_pressure";
#endif
#ifdef POS_VISIBILITY
  uri += ",visibility";
#endif
#ifdef POS_UVI
  uri += ",uv_index";
#endif

  uri += "&hourly=temperature_2m,precipitation_probability,precipitation,"
         "wind_speed_10m,wind_direction_10m,wind_gusts_10m";
#if DISPLAY_HOURLY_ICONS
  uri += ",weather_code,is_day";
#endif

  uri += "&daily=weather_code,temperature_2m_max,temperature_2m_min";
#if defined(POS_SUNRISE) || defined(POS_SUNSET)
  uri += ",sunrise,sunset";
#endif
#if DISPLAY_DAILY_PRECIP
  uri += ",precipitation_probability_max,precipitation_sum";
#endif

  uri += "&timezone=auto"
         "&forecast_days=8";

  String fullUri = "https://" + OPENMETEO_ENDPOINT + uri;
#if DEBUG_LEVEL >= 2
  Serial.println("[Open-Meteo API] Request: " + fullUri);
#endif
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
#if DEBUG_LEVEL >= 2
  Serial.println("[Open-Meteo Air Quality API] Request: " + fullUri);
#endif
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
