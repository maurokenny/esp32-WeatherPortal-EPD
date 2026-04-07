/// @file client_utils.h
/// @brief Network utilities: WiFi, NTP, and HTTP API client functions
/// @copyright Copyright (C) 2022-2023 Luke Marzen
/// @license GNU General Public License v3.0
///
/// @details
/// Provides network connectivity and time synchronization:
/// - WiFi connection management with signal strength reporting
/// - NTP time synchronization with timezone support
/// - Open-Meteo API HTTP/HTTPS requests with retry logic
/// - Heap usage diagnostics
///
/// @note All functions are blocking except where noted. Timeout values
/// configured in config.cpp.

#ifndef __CLIENT_UTILS_H__
#define __CLIENT_UTILS_H__

#include <Arduino.h>
#include "api_response.h"
#include "config.h"

#ifdef USE_HTTP
  #include <WiFiClient.h>
#else
  #include <WiFiClientSecure.h>
#endif

/// @defgroup wifi_funcs WiFi Management
/// @{

/// @brief Power on and connect to WiFi network
/// @param wifiRSSI Output parameter for signal strength (dBm)
/// @return WiFi connection status
/// @details Uses credentials from ramSSID/ramPassword (RTC RAM).
/// Blocks until connected or WIFI_TIMEOUT expires.
/// @note WiFi is powered off after use to save power; RSSI must be read now
wl_status_t startWiFi(int &wifiRSSI);

/// @brief Disconnect and power off WiFi
/// @details Shuts down WiFi radio to minimize power consumption
void killWiFi();

/// @}

/// @defgroup time_funcs Time Synchronization
/// @{

/// @brief Wait for NTP synchronization and print local time
/// @param timeInfo Output tm structure with local time
/// @return true if time synchronized successfully
/// @details Blocks until SNTP sync complete or NTP_TIMEOUT expires
bool waitForSNTPSync(tm *timeInfo);

/// @brief Print current local time to serial
/// @param timeInfo Output tm structure
/// @return true if local time retrieved successfully
bool printLocalTime(tm *timeInfo);

/// @}

/// @defgroup api_funcs Open-Meteo API Functions
/// @brief HTTP client functions for weather data retrieval
/// @{

#ifdef USE_HTTP
  /// @brief Fetch weather forecast from Open-Meteo API
  /// @param client WiFi client (HTTP mode)
  /// @param r Output structure to populate
  /// @return HTTP status code (200=OK) or negative for error
  int getOpenMeteoForecast(WiFiClient &client, owm_resp_onecall_t &r);

  /// @brief Fetch air quality data from Open-Meteo API
  /// @param client WiFi client (HTTP mode)
  /// @param r Output structure to populate
  /// @return HTTP status code
  int getOpenMeteoAirQuality(WiFiClient &client, owm_resp_air_pollution_t &r);
#else
  /// @brief Fetch weather forecast from Open-Meteo API (HTTPS)
  /// @param client Secure WiFi client
  /// @param r Output structure to populate
  /// @return HTTP status code (200=OK) or negative for error
  int getOpenMeteoForecast(WiFiClientSecure &client, owm_resp_onecall_t &r);

  /// @brief Fetch air quality data from Open-Meteo API (HTTPS)
  /// @param client Secure WiFi client
  /// @param r Output structure to populate
  /// @return HTTP status code
  int getOpenMeteoAirQuality(WiFiClientSecure &client, owm_resp_air_pollution_t &r);
#endif

/// @}

/// @defgroup debug_funcs Debugging Utilities
/// @{

/// @brief Print heap usage statistics to serial
/// @details Outputs heap size, free heap, min free heap, and max allocatable
/// @note Only active when DEBUG_LEVEL >= 1
void printHeapUsage();

/// @}

#endif // __CLIENT_UTILS_H__
