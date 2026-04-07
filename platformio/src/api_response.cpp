/// @file api_response.cpp
/// @brief JSON deserialization for OpenWeatherMap and Open-Meteo APIs
/// @copyright Copyright (C) 2022-2024 Luke Marzen, 2026 Mauro Freitas
/// @license GNU General Public License v3.0
///
/// @details
/// Implements JSON parsing for weather API responses:
/// - OpenWeatherMap OneCall API (legacy support)
/// - Open-Meteo Forecast API (primary)
/// - Open-Meteo Air Quality API
///
/// WMO to OWM code conversion for icon compatibility.
/// Handles chunked HTTP transfer encoding.
///
/// @note Temperatures converted from Celsius (Open-Meteo) to Kelvin (internal format)

#include <vector>
#include <cstring>
#include <ArduinoJson.h>
#include "api_response.h"
#include "conversions.h"
#include "config.h"
#include "wifi_manager.h"

/// @brief Deserialize OpenWeatherMap OneCall API response
/// @param json WiFi client stream
/// @param r Output structure to populate
/// @return ArduinoJson deserialization error
DeserializationError deserializeOneCall(WiFiClient &json,
                                        owm_resp_onecall_t &r)
{
  int i;

  JsonDocument filter;
  filter["current"]  = true;
  filter["minutely"] = false;
  filter["hourly"]   = true;
  filter["daily"]    = true;
#if !DISPLAY_ALERTS
  filter["alerts"]   = false;
#else
  // description can be very long so they are filtered out to save on memory
  // along with sender_name
  for (int i = 0; i < OWM_NUM_ALERTS; ++i)
  {
    filter["alerts"][i]["sender_name"] = false;
    filter["alerts"][i]["event"]       = true;
    filter["alerts"][i]["start"]       = true;
    filter["alerts"][i]["end"]         = true;
    filter["alerts"][i]["description"] = false;
    filter["alerts"][i]["tags"]        = true;
  }
#endif

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json,
                                         DeserializationOption::Filter(filter));
#if DEBUG_LEVEL >= 1
  Serial.println("[debug] doc.overflowed() : "
                 + String(doc.overflowed()));
#endif
#if DEBUG_LEVEL >= 2
  serializeJsonPretty(doc, Serial);
#endif
  if (error) {
    return error;
  }

  r.lat             = doc["lat"]            .as<float>();
  r.lon             = doc["lon"]            .as<float>();
  r.timezone        = doc["timezone"]       .as<const char *>();
  r.timezone_offset = doc["timezone_offset"].as<int>();

  JsonObject current = doc["current"];
  r.current.dt         = current["dt"]        .as<int64_t>();
  r.current.sunrise    = current["sunrise"]   .as<int64_t>();
  r.current.sunset     = current["sunset"]    .as<int64_t>();
  r.current.temp       = current["temp"]      .as<float>();
  r.current.feels_like = current["feels_like"].as<float>();
  r.current.pressure   = current["pressure"]  .as<int>();
  r.current.humidity   = current["humidity"]  .as<int>();
  r.current.dew_point  = current["dew_point"] .as<float>();
  r.current.clouds     = current["clouds"]    .as<int>();
  r.current.uvi        = current["uvi"]       .as<float>();
  r.current.visibility = current["visibility"].as<int>();
  r.current.wind_speed = current["wind_speed"].as<float>();
  r.current.wind_gust  = current["wind_gust"] .as<float>();
  r.current.wind_deg   = current["wind_deg"]  .as<int>();
  r.current.rain_1h    = current["rain"]["1h"].as<float>();
  r.current.snow_1h    = current["snow"]["1h"].as<float>();
  JsonObject current_weather = current["weather"][0];
  r.current.weather.id          = current_weather["id"]         .as<int>();
  r.current.weather.main        = current_weather["main"]       .as<const char *>();
  r.current.weather.description = current_weather["description"].as<const char *>();
  r.current.weather.icon        = current_weather["icon"]       .as<const char *>();

  // minutely forecast is currently unused
  // i = 0;
  // for (JsonObject minutely : doc["minutely"].as<JsonArray>())
  // {
  //   r.minutely[i].dt            = minutely["dt"]           .as<int64_t>();
  //   r.minutely[i].precipitation = minutely["precipitation"].as<float>();

  //   if (i == OWM_NUM_MINUTELY - 1)
  //   {
  //     break;
  //   }
  //   ++i;
  // }

  i = 0;
  for (JsonObject hourly : doc["hourly"].as<JsonArray>())
  {
    r.hourly[i].dt         = hourly["dt"]         .as<int64_t>();
    r.hourly[i].temp       = hourly["temp"]       .as<float>();
    r.hourly[i].feels_like = hourly["feels_like"].as<float>();
    r.hourly[i].pressure   = hourly["pressure"]   .as<int>();
    r.hourly[i].humidity   = hourly["humidity"]   .as<int>();
    r.hourly[i].dew_point  = hourly["dew_point"]  .as<float>();
    r.hourly[i].clouds     = hourly["clouds"]     .as<int>();
    r.hourly[i].uvi        = hourly["uvi"]        .as<float>();
    r.hourly[i].visibility = hourly["visibility"] .as<int>();
    r.hourly[i].wind_speed = hourly["wind_speed"].as<float>();
    r.hourly[i].wind_gust  = hourly["wind_gust"] .as<float>();
    r.hourly[i].wind_deg   = hourly["wind_deg"]  .as<int>();
    r.hourly[i].pop        = hourly["pop"]       .as<float>();
    r.hourly[i].rain_1h    = hourly["rain"]["1h"] .as<float>();
    r.hourly[i].snow_1h    = hourly["snow"]["1h"] .as<float>();
    JsonObject hourly_weather = hourly["weather"][0];
    r.hourly[i].weather.id          = hourly_weather["id"]         .as<int>();
    r.hourly[i].weather.main        = hourly_weather["main"]       .as<const char *>();
    r.hourly[i].weather.description = hourly_weather["description"].as<const char *>();
    r.hourly[i].weather.icon        = hourly_weather["icon"]       .as<const char *>();

    if (i == OWM_NUM_HOURLY - 1)
    {
      break;
    }
    ++i;
  }

  i = 0;
  for (JsonObject daily : doc["daily"].as<JsonArray>())
  {
    r.daily[i].dt       = daily["dt"]      .as<int64_t>();
    r.daily[i].sunrise  = daily["sunrise"] .as<int64_t>();
    r.daily[i].sunset   = daily["sunset"]  .as<int64_t>();
    // r.daily[i].moonrise = daily["moonrise"].as<int64_t>();
    // r.daily[i].moonset  = daily["moonset"] .as<int64_t>();
    // r.daily[i].moon_phase = daily["moon_phase"].as<float>();

    r.daily[i].temp.morn  = daily["temp"]["morn"].as<float>();
    r.daily[i].temp.day   = daily["temp"]["day"] .as<float>();
    r.daily[i].temp.eve   = daily["temp"]["eve"] .as<float>();
    r.daily[i].temp.night = daily["temp"]["night"].as<float>();
    r.daily[i].temp.min   = daily["temp"]["min"] .as<float>();
    r.daily[i].temp.max   = daily["temp"]["max"] .as<float>();

    r.daily[i].feels_like.morn  = daily["feels_like"]["morn"].as<float>();
    r.daily[i].feels_like.day   = daily["feels_like"]["day"] .as<float>();
    r.daily[i].feels_like.eve   = daily["feels_like"]["eve"] .as<float>();
    r.daily[i].feels_like.night = daily["feels_like"]["night"].as<float>();

    r.daily[i].pressure  = daily["pressure"] .as<int>();
    r.daily[i].humidity  = daily["humidity"] .as<int>();
    r.daily[i].dew_point = daily["dew_point"].as<float>();
    r.daily[i].clouds    = daily["clouds"]   .as<int>();
    r.daily[i].uvi       = daily["uvi"]      .as<float>();
    r.daily[i].visibility = daily["visibility"].as<int>();
    r.daily[i].wind_speed = daily["wind_speed"].as<float>();
    r.daily[i].wind_gust  = daily["wind_gust"] .as<float>();
    r.daily[i].wind_deg   = daily["wind_deg"]  .as<int>();
    r.daily[i].pop        = daily["pop"]       .as<float>();
    r.daily[i].rain       = daily["rain"]      .as<float>();
    r.daily[i].snow       = daily["snow"]      .as<float>();
    JsonObject daily_weather = daily["weather"][0];
    r.daily[i].weather.id          = daily_weather["id"]         .as<int>();
    r.daily[i].weather.main        = daily_weather["main"]       .as<const char *>();
    r.daily[i].weather.description = daily_weather["description"].as<const char *>();
    r.daily[i].weather.icon        = daily_weather["icon"]       .as<const char *>();

    if (i == OWM_NUM_DAILY - 1)
    {
      break;
    }
    ++i;
  }

#if DISPLAY_ALERTS
  i = 0;
  for (JsonObject alert : doc["alerts"].as<JsonArray>())
  {
    r.alerts[i].sender_name = alert["sender_name"].as<const char *>();
    r.alerts[i].event       = alert["event"]      .as<const char *>();
    r.alerts[i].start       = alert["start"]      .as<int64_t>();
    r.alerts[i].end         = alert["end"]        .as<int64_t>();
    r.alerts[i].description = alert["description"].as<const char *>();
    r.alerts[i].tags        = alert["tags"]       .as<const char *>();

    if (i == OWM_NUM_ALERTS - 1)
    {
      break;
    }
    ++i;
  }
#endif

  return error;
} // end deserializeOneCall

/// @brief Deserialize OpenWeatherMap Air Pollution API response
/// @param json WiFi client stream
/// @param r Output structure to populate
/// @return ArduinoJson deserialization error
DeserializationError deserializeAirQuality(WiFiClient &json,
                                           owm_resp_air_pollution_t &r)
{
  int i;

  JsonDocument filter;
  filter["coord"] = true;
  filter["list"]  = true;

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json,
                                         DeserializationOption::Filter(filter));
#if DEBUG_LEVEL >= 1
  Serial.println("[debug] doc.overflowed() : "
                 + String(doc.overflowed()));
#endif
#if DEBUG_LEVEL >= 2
  serializeJsonPretty(doc, Serial);
#endif
  if (error) {
    return error;
  }

  JsonObject coord = doc["coord"];
  r.coord.lat = coord["lat"].as<float>();
  r.coord.lon = coord["lon"].as<float>();

  i = 0;
  for (JsonObject list : doc["list"].as<JsonArray>())
  {

    r.main_aqi[i] = list["main"]["aqi"].as<int>();

    JsonObject list_components = list["components"];
    r.components.co[i]    = list_components["co"].as<float>();
    r.components.no[i]    = list_components["no"].as<float>();
    r.components.no2[i]   = list_components["no2"].as<float>();
    r.components.o3[i]    = list_components["o3"].as<float>();
    r.components.so2[i]   = list_components["so2"].as<float>();
    r.components.pm2_5[i] = list_components["pm2_5"].as<float>();
    r.components.pm10[i]  = list_components["pm10"].as<float>();
    r.components.nh3[i]   = list_components["nh3"].as<float>();

    r.dt[i] = list["dt"].as<int64_t>();

    if (i == OWM_NUM_AIR_POLLUTION - 1)
    {
      break;
    }
    ++i;
  }

  return error;
} // end deserializeAirQuality

/// ═══════════════════════════════════════════════════════════════════════════
/// Open-Meteo API Support
/// ═══════════════════════════════════════════════════════════════════════════

/// @brief Convert UTC time structure to Unix epoch
/// @param timeinfo Time structure (assumed UTC)
/// @return Unix timestamp in seconds
/// @details Compatibility function for environments without timegm()
static int64_t timegm_compat(struct tm *timeinfo)
{
  char *oldTz = nullptr;
  bool hadOldTz = false;
  if (getenv("TZ") != nullptr) {
    const char *currentTz = getenv("TZ");
    oldTz = (char *)malloc(strlen(currentTz) + 1);
    if (oldTz != nullptr) {
      strcpy(oldTz, currentTz);
      hadOldTz = true;
    }
  }

  setenv("TZ", "UTC0", 1);
  tzset();
  timeinfo->tm_isdst = 0;
  int64_t result = mktime(timeinfo);

  if (hadOldTz) {
    setenv("TZ", oldTz, 1);
    free(oldTz);
  } else {
    unsetenv("TZ");
  }
  tzset();
  return result;
}

/**
 * Parse ISO 8601 datetime string to Unix timestamp.
 * Format: "2026-03-24T18:15"
 * @param iso8601 ISO 8601 datetime string
 * @param inputIsUtc true when the input string is UTC (timezone=GMT), false when local time (timezone=auto)
 * @param tzOffsetSeconds local timezone offset in seconds when inputIsUtc is false
 */
int64_t parseIso8601(const char* iso8601, bool inputIsUtc, int tzOffsetSeconds)
{
  if (iso8601 == nullptr || strlen(iso8601) < 16) {
    return 0;
  }
  
  struct tm timeinfo = {};
  int year, month, day, hour, minute;
  if (sscanf(iso8601, "%d-%d-%dT%d:%d", &year, &month, &day, &hour, &minute) != 5) {
    return 0;
  }
  
  timeinfo.tm_year = year - 1900;
  timeinfo.tm_mon = month - 1;
  timeinfo.tm_mday = day;
  timeinfo.tm_hour = hour;
  timeinfo.tm_min = minute;
  timeinfo.tm_sec = 0;

  if (inputIsUtc) {
    return timegm_compat(&timeinfo);
  }

  if (tzOffsetSeconds != 0) {
    int64_t utcEpoch = timegm_compat(&timeinfo);
    return utcEpoch - tzOffsetSeconds;
  }

  timeinfo.tm_isdst = -1;
  return mktime(&timeinfo);
}

/// @brief Convert WMO code to OpenWeatherMap-compatible structure
/// @param wmoCode WMO Weather Interpretation code (0-99)
/// @param isDay true for daytime icon selection
/// @param weather Output structure to populate
/// @details Maps WMO codes to OWM-compatible descriptions and icon IDs.
/// Allows reusing existing icon assets designed for OWM codes.
/// @see https://open-meteo.com/en/docs
/// @see https://openweathermap.org/weather-conditions
void wmoToOwmWeather(int wmoCode, bool isDay, owm_weather_t &weather)
{
  // Map WMO code to OWM-compatible values
  switch (wmoCode) {
    case 0: // Clear sky
      weather.id = 800;
      weather.main = "Clear";
      weather.description = "clear sky";
      weather.icon = isDay ? "01d" : "01n";
      break;
    case 1: // Mainly clear
      weather.id = 801;
      weather.main = "Clouds";
      weather.description = "few clouds";
      weather.icon = isDay ? "02d" : "02n";
      break;
    case 2: // Partly cloudy
      weather.id = 802;
      weather.main = "Clouds";
      weather.description = "scattered clouds";
      weather.icon = isDay ? "03d" : "03n";
      break;
    case 3: // Overcast
      weather.id = 804;
      weather.main = "Clouds";
      weather.description = "overcast clouds";
      weather.icon = isDay ? "04d" : "04n";
      break;
    case 45: // Fog
    case 48: // Depositing rime fog
      weather.id = 741;
      weather.main = "Fog";
      weather.description = "fog";
      weather.icon = "50d";
      break;
    case 51: // Light drizzle
      weather.id = 300;
      weather.main = "Drizzle";
      weather.description = "light intensity drizzle";
      weather.icon = isDay ? "09d" : "09n";
      break;
    case 53: // Moderate drizzle
      weather.id = 301;
      weather.main = "Drizzle";
      weather.description = "drizzle";
      weather.icon = isDay ? "09d" : "09n";
      break;
    case 55: // Dense drizzle
      weather.id = 302;
      weather.main = "Drizzle";
      weather.description = "heavy intensity drizzle";
      weather.icon = isDay ? "09d" : "09n";
      break;
    case 56: // Light freezing drizzle
      weather.id = 311;
      weather.main = "Drizzle";
      weather.description = "light intensity drizzle rain";
      weather.icon = isDay ? "09d" : "09n";
      break;
    case 57: // Dense freezing drizzle
      weather.id = 312;
      weather.main = "Drizzle";
      weather.description = "heavy intensity drizzle rain";
      weather.icon = isDay ? "09d" : "09n";
      break;
    case 61: // Slight rain
      weather.id = 500;
      weather.main = "Rain";
      weather.description = "light rain";
      weather.icon = isDay ? "10d" : "10n";
      break;
    case 63: // Moderate rain
      weather.id = 501;
      weather.main = "Rain";
      weather.description = "moderate rain";
      weather.icon = isDay ? "10d" : "10n";
      break;
    case 65: // Heavy rain
      weather.id = 502;
      weather.main = "Rain";
      weather.description = "heavy intensity rain";
      weather.icon = isDay ? "10d" : "10n";
      break;
    case 66: // Light freezing rain
      weather.id = 511;
      weather.main = "Rain";
      weather.description = "freezing rain";
      weather.icon = isDay ? "13d" : "13n";
      break;
    case 67: // Heavy freezing rain
      weather.id = 511;
      weather.main = "Rain";
      weather.description = "heavy freezing rain";
      weather.icon = isDay ? "13d" : "13n";
      break;
    case 71: // Slight snow fall
      weather.id = 600;
      weather.main = "Snow";
      weather.description = "light snow";
      weather.icon = isDay ? "13d" : "13n";
      break;
    case 73: // Moderate snow fall
      weather.id = 601;
      weather.main = "Snow";
      weather.description = "snow";
      weather.icon = isDay ? "13d" : "13n";
      break;
    case 75: // Heavy snow fall
      weather.id = 602;
      weather.main = "Snow";
      weather.description = "heavy snow";
      weather.icon = isDay ? "13d" : "13n";
      break;
    case 77: // Snow grains
      weather.id = 600;
      weather.main = "Snow";
      weather.description = "light snow";
      weather.icon = isDay ? "13d" : "13n";
      break;
    case 80: // Slight rain showers
      weather.id = 520;
      weather.main = "Rain";
      weather.description = "light intensity shower rain";
      weather.icon = isDay ? "09d" : "09n";
      break;
    case 81: // Moderate rain showers
      weather.id = 521;
      weather.main = "Rain";
      weather.description = "shower rain";
      weather.icon = isDay ? "09d" : "09n";
      break;
    case 82: // Violent rain showers
      weather.id = 522;
      weather.main = "Rain";
      weather.description = "heavy intensity shower rain";
      weather.icon = isDay ? "09d" : "09n";
      break;
    case 85: // Slight snow showers
      weather.id = 620;
      weather.main = "Snow";
      weather.description = "light shower snow";
      weather.icon = isDay ? "13d" : "13n";
      break;
    case 86: // Heavy snow showers
      weather.id = 621;
      weather.main = "Snow";
      weather.description = "shower snow";
      weather.icon = isDay ? "13d" : "13n";
      break;
    case 95: // Thunderstorm
      weather.id = 200;
      weather.main = "Thunderstorm";
      weather.description = "thunderstorm with light rain";
      weather.icon = isDay ? "11d" : "11n";
      break;
    case 96: // Thunderstorm with slight hail
    case 99: // Thunderstorm with heavy hail
      weather.id = 202;
      weather.main = "Thunderstorm";
      weather.description = "thunderstorm with heavy rain";
      weather.icon = isDay ? "11d" : "11n";
      break;
    default: // Unknown
      weather.id = 800;
      weather.main = "Clear";
      weather.description = "clear sky";
      weather.icon = isDay ? "01d" : "01n";
      break;
  }
}

/// @brief Deserialize Open-Meteo Forecast API response
/// @param json WiFi client stream
/// @param r Output structure to populate
/// @return ArduinoJson deserialization error
/// @details Handles chunked transfer encoding, parses ISO 8601 timestamps.
/// Temperatures converted from Celsius to Kelvin for internal consistency.
DeserializationError deserializeOpenMeteo(WiFiClient &json,
                                          owm_resp_onecall_t &r)
{
  
  // Read entire response into String first for debugging
  String jsonString = "";
  int bytesRead = 0;
  unsigned long startTime = millis();
  
  // Keep reading until stream is empty or timeout
  while (bytesRead < 30000 && (millis() - startTime) < 10000) {
    if (json.available()) {
      char c = json.read();
      if (c >= 0) {
        jsonString += c;
        bytesRead++;
      }
    } else {
      delay(10);
    }
  }
  
  
  // Remove HTTP chunked encoding markers (hex size + \r\n before each chunk)
  // Chunked format: "<hex-size>\r\n<data>\r\n...0\r\n\r\n"
  String cleanedJson = "";
  int pos = 0;
  int chunksProcessed = 0;
  
  
  while (pos < (int)jsonString.length()) {
    // Find end of chunk size line (\r\n)
    int lineEnd = jsonString.indexOf("\r\n", pos);
    if (lineEnd < 0) {
      break;
    }
    
    // Extract chunk size (hex string)
    String chunkSizeStr = jsonString.substring(pos, lineEnd);
    chunkSizeStr.trim();
    
    if (chunkSizeStr.length() == 0) {
      break;
    }
    
    // Check if it looks like a hex number
    bool isHex = true;
    for (int i = 0; i < chunkSizeStr.length(); i++) {
      char c = chunkSizeStr.charAt(i);
      if (!isxdigit(c)) {
        isHex = false;
        break;
      }
    }
    
    if (!isHex) {
      break;
    }
    
    // Convert hex to int
    int chunkSize = (int)strtol(chunkSizeStr.c_str(), NULL, 16);
    
    if (chunkSize == 0) {
      break; // Last chunk
    }
    
    // Move to start of chunk data
    pos = lineEnd + 2; // Skip \r\n
    
    // Append chunk data
    if (pos + chunkSize <= (int)jsonString.length()) {
      cleanedJson += jsonString.substring(pos, pos + chunkSize);
      pos += chunkSize + 2; // Skip data + \r\n
      chunksProcessed++;
    } else {
      break;
    }
  }
  
  
  // If no chunked encoding detected, use original string
  if (cleanedJson.length() == 0) {
    cleanedJson = jsonString;
  }
  
  
  // Find where JSON actually starts
  int jsonStart = cleanedJson.indexOf('{');
  if (jsonStart >= 0) {
    cleanedJson = cleanedJson.substring(jsonStart);
  } else {
    return DeserializationError::InvalidInput;
  }
  
  // Find actual end of JSON
  int lastBrace = cleanedJson.lastIndexOf('}');
  if (lastBrace > 0 && lastBrace < (int)cleanedJson.length() - 1) {
    cleanedJson = cleanedJson.substring(0, lastBrace + 1);
  }
  
  jsonString = cleanedJson;
  
  // Check braces balance
  int openBraces = 0, closeBraces = 0;
  for (int i = 0; i < (int)jsonString.length(); i++) {
    if (jsonString[i] == '{') openBraces++;
    if (jsonString[i] == '}') closeBraces++;
  }
  
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonString);
  
  
  if (error) {
  }
  
  if (error && doc.size() == 0) {
    return error;
  }
  
  if (error) {
  }
  
  // Location data
  r.lat = doc["latitude"].as<float>();
  r.lon = doc["longitude"].as<float>();
  r.timezone = doc["timezone"].as<const char *>();
  r.timezone_offset = doc["utc_offset_seconds"].as<int>();
// When timezone mode is MANUAL, API returns times in UTC (we use timezone=GMT)
  // When AUTO, API returns times in local timezone
  const bool openMeteoTimeStringsAreUtc = (ramTimezoneMode == TIMEZONE_MODE_MANUAL);

  
  // Parse current weather
  JsonObject current = doc["current"];
  const char* timeStr = current["time"];
  if (timeStr != nullptr) {
    r.current.dt = parseIso8601(timeStr, openMeteoTimeStringsAreUtc, r.timezone_offset);
  } else {
    r.current.dt = 0;
  }
  
  // Open-Meteo returns temperatures in Celsius, convert to Kelvin
  r.current.temp = celsius_to_kelvin(current["temperature_2m"].as<float>());
  r.current.feels_like = celsius_to_kelvin(current["apparent_temperature"].as<float>());
  r.current.pressure = current["surface_pressure"].as<int>();
  r.current.humidity = current["relative_humidity_2m"].as<int>();
  r.current.dew_point = celsius_to_kelvin(current["dew_point_2m"].as<float>());
  r.current.clouds = current["cloud_cover"].as<int>();
  r.current.uvi = current["uv_index"].as<float>();
  r.current.visibility = current["visibility"].as<int>();
  r.current.wind_speed = kilometersperhour_to_meterspersecond(current["wind_speed_10m"].as<float>());
  r.current.wind_gust = kilometersperhour_to_meterspersecond(current["wind_gusts_10m"].as<float>());
  r.current.wind_deg = current["wind_direction_10m"].as<int>();
  r.current.rain_1h = current["rain"].as<float>();
  r.current.snow_1h = centimeters_to_millimeters(current["snowfall"].as<float>()); // cm to mm
  
  // Determine if day or night
  bool isDay = current["is_day"].as<int>() == 1;
  
  // Convert WMO weather code to OWM format
  int wmoCode = current["weather_code"].as<int>();
  wmoToOwmWeather(wmoCode, isDay, r.current.weather);
  
  // Debug: Show received values for location comparison
#if DEBUG_LEVEL >= 2
  Serial.println("[Open-Meteo] Location: " + String(r.lat, 2) + ", " + String(r.lon, 2));
#endif
  Serial.println("[Open-Meteo] Current temp: " + String(kelvin_to_celsius(r.current.temp), 1) + "C, Weather: " + String(wmoCode) + ", Day: " + String(isDay));
  
  // Parse daily forecast first (needed for sunrise/sunset which are not in current)
  JsonObject daily = doc["daily"];
  JsonArray daily_time = daily["time"];
  JsonArray daily_max = daily["temperature_2m_max"];
  JsonArray daily_min = daily["temperature_2m_min"];
  JsonArray daily_sunrise = daily["sunrise"];
  JsonArray daily_sunset = daily["sunset"];
  JsonArray daily_code = daily["weather_code"];
  JsonArray daily_pop = daily["precipitation_probability_max"];
  JsonArray daily_precip = daily["precipitation_sum"];
  
  int dailyCount = min((int)daily_time.size(), OWM_NUM_DAILY);
  for (int i = 0; i < dailyCount; i++) {
    r.daily[i].dt = parseIso8601(daily_time[i], openMeteoTimeStringsAreUtc, r.timezone_offset);
    // Convert Celsius to Kelvin
    r.daily[i].temp.max = celsius_to_kelvin(daily_max[i].as<float>());
    r.daily[i].temp.min = celsius_to_kelvin(daily_min[i].as<float>());
    r.daily[i].sunrise = parseIso8601(daily_sunrise[i], openMeteoTimeStringsAreUtc, r.timezone_offset);
    r.daily[i].sunset = parseIso8601(daily_sunset[i], openMeteoTimeStringsAreUtc, r.timezone_offset);
    r.daily[i].pop = daily_pop[i].as<float>() / 100.0f;
    
    // Read daily precipitation (mm)
    r.daily[i].rain = daily_precip[i].as<float>();
    r.daily[i].snow = 0.0f;
    
    // Daily always uses day icons
    wmoToOwmWeather(daily_code[i].as<int>(), true, r.daily[i].weather);
    
    // Default values
    r.daily[i].moonrise = 0;
    r.daily[i].moonset = 0;
    r.daily[i].moon_phase = 0.0f;
    r.daily[i].temp.morn = r.daily[i].temp.min;
    r.daily[i].temp.day = (r.daily[i].temp.min + r.daily[i].temp.max) / 2.0f;
    r.daily[i].temp.eve = r.daily[i].temp.max;
    r.daily[i].temp.night = r.daily[i].temp.min;
    r.daily[i].feels_like.morn = r.daily[i].temp.morn;
    r.daily[i].feels_like.day = r.daily[i].temp.day;
    r.daily[i].feels_like.eve = r.daily[i].temp.eve;
    r.daily[i].feels_like.night = r.daily[i].temp.night;
    r.daily[i].pressure = 1013;
    r.daily[i].humidity = 60;
    r.daily[i].dew_point = r.daily[i].temp.min - 2.0f;
    r.daily[i].clouds = 50;
    r.daily[i].uvi = 0.0f;
    r.daily[i].visibility = 10000;
    r.daily[i].wind_speed = 5.0f;
    r.daily[i].wind_gust = 10.0f;
    r.daily[i].wind_deg = 0;
  }
  
  // Copy sunrise/sunset from daily[0] to current (Open-Meteo doesn't provide these in current)
  if (dailyCount > 0) {
    r.current.sunrise = r.daily[0].sunrise;
    r.current.sunset = r.daily[0].sunset;
    Serial.println("[Open-Meteo] Sunrise: " + String(r.current.sunrise) + ", Sunset: " + String(r.current.sunset));
  } else {
    r.current.sunrise = 0;
    r.current.sunset = 0;
  }
  
  // Parse hourly forecast
  JsonObject hourly = doc["hourly"];
  JsonArray hourly_time = hourly["time"];
  JsonArray hourly_temp = hourly["temperature_2m"];
  JsonArray hourly_feels = hourly["apparent_temperature"];
  JsonArray hourly_humidity = hourly["relative_humidity_2m"];
  JsonArray hourly_pressure = hourly["surface_pressure"];
  JsonArray hourly_clouds = hourly["cloud_cover"];
  JsonArray hourly_wind = hourly["wind_speed_10m"];
  JsonArray hourly_gust = hourly["wind_gusts_10m"];
  JsonArray hourly_deg = hourly["wind_direction_10m"];
  JsonArray hourly_pop = hourly["precipitation_probability"];
  JsonArray hourly_precip = hourly["precipitation"];
  JsonArray hourly_code = hourly["weather_code"];
  JsonArray hourly_is_day = hourly["is_day"];
  
  int hourlyCount = min((int)hourly_time.size(), OWM_NUM_HOURLY);
  for (int i = 0; i < hourlyCount; i++) {
    r.hourly[i].dt = parseIso8601(hourly_time[i], openMeteoTimeStringsAreUtc, r.timezone_offset);
    // Convert Celsius to Kelvin
    r.hourly[i].temp = celsius_to_kelvin(hourly_temp[i].as<float>());
    r.hourly[i].feels_like = celsius_to_kelvin(hourly_feels[i].as<float>());
    r.hourly[i].humidity = hourly_humidity[i].as<int>();
    r.hourly[i].pressure = hourly_pressure[i].as<int>();
    r.hourly[i].clouds = hourly_clouds[i].as<int>();
    r.hourly[i].wind_speed = kilometersperhour_to_meterspersecond(hourly_wind[i].as<float>());
    r.hourly[i].wind_gust = kilometersperhour_to_meterspersecond(hourly_gust[i].as<float>());
    r.hourly[i].wind_deg = hourly_deg[i].as<int>();
    
    float pop_val = hourly_pop[i].as<float>();
    r.hourly[i].pop = pop_val / 100.0f; // Convert % to 0-1
    r.hourly[i].rain_1h = hourly_precip[i].as<float>();
    r.hourly[i].snow_1h = 0.0f; // Open-Meteo doesn't separate snow in basic endpoint
    
    bool hourIsDay = hourly_is_day[i].as<int>() == 1;
    wmoToOwmWeather(hourly_code[i].as<int>(), hourIsDay, r.hourly[i].weather);
    
    // Default values for fields not provided by Open-Meteo
    r.hourly[i].dew_point = r.hourly[i].temp - 2.0f; // Estimate (already in Kelvin)
    r.hourly[i].uvi = 0.0f;
    r.hourly[i].visibility = 10000;
  }
  
  // Clear alerts (not provided by Open-Meteo)
  r.alerts.clear();
  
  // Debug: Show summary for comparing locations
  float tempMin = r.hourly[0].temp, tempMax = r.hourly[0].temp;
  float popMin = r.hourly[0].pop, popMax = r.hourly[0].pop;
  for (int i = 1; i < hourlyCount; i++) {
    tempMin = min(tempMin, r.hourly[i].temp);
    tempMax = max(tempMax, r.hourly[i].temp);
    popMin = min(popMin, r.hourly[i].pop);
    popMax = max(popMax, r.hourly[i].pop);
  }
  Serial.println("[Open-Meteo] Summary: Temp range " + String(kelvin_to_celsius(tempMin), 1) + "C to " + String(kelvin_to_celsius(tempMax), 1) + "C");
  Serial.println("[Open-Meteo] Summary: POP range " + String(popMin * 100, 0) + "% to " + String(popMax * 100, 0) + "%");
  
  return error;
} // end deserializeOpenMeteo

/// @brief Deserialize Open-Meteo Air Quality API response
/// @param json WiFi client stream
/// @param r Output structure to populate
/// @return ArduinoJson deserialization error
DeserializationError deserializeOpenMeteoAirQuality(WiFiClient &json,
                                                    owm_resp_air_pollution_t &r)
{
  // Read entire response into buffer with timeout
  String jsonString = "";
  int bytesRead = 0;
  unsigned long startTime = millis();
  
  // Keep reading until stream is empty or timeout (10 seconds)
  while (bytesRead < 15000 && (millis() - startTime) < 10000) {
    if (json.available()) {
      char c = json.read();
      if (c >= 0) {
        jsonString += c;
        bytesRead++;
      }
    } else {
      // Stream empty, wait a bit for more data
      delay(10);
    }
  }
  
  // Remove HTTP chunked encoding markers (hex size + \r\n before each chunk)
  String cleanedJson = "";
  int pos = 0;
  while (pos < (int)jsonString.length()) {
    int lineEnd = jsonString.indexOf("\r\n", pos);
    if (lineEnd < 0) break;
    
    String chunkSizeStr = jsonString.substring(pos, lineEnd);
    chunkSizeStr.trim();
    if (chunkSizeStr.length() == 0) break;
    
    int chunkSize = (int)strtol(chunkSizeStr.c_str(), NULL, 16);
    if (chunkSize == 0) break;
    
    pos = lineEnd + 2;
    
    if (pos + chunkSize <= (int)jsonString.length()) {
      cleanedJson += jsonString.substring(pos, pos + chunkSize);
      pos += chunkSize + 2;
    } else {
      break;
    }
  }
  
  if (cleanedJson.length() == 0) {
    cleanedJson = jsonString;
  }
  
  // Find where JSON actually starts
  int jsonStart = cleanedJson.indexOf('{');
  if (jsonStart >= 0) {
    cleanedJson = cleanedJson.substring(jsonStart);
  }
  
  // Find actual end of JSON
  int lastBrace = cleanedJson.lastIndexOf('}');
  if (lastBrace > 0 && lastBrace < (int)cleanedJson.length() - 1) {
    cleanedJson = cleanedJson.substring(0, lastBrace + 1);
  }
  
  jsonString = cleanedJson;
  
  // Validate JSON is not empty and starts with {
  if (jsonString.length() == 0 || jsonString[0] != '{') {
    return DeserializationError::InvalidInput;
  }
  
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonString);
  
  if (error) {
    return error;
  }
  
  // Location
  r.coord.lat = doc["latitude"].as<float>();
  r.coord.lon = doc["longitude"].as<float>();
// When timezone mode is MANUAL, API returns times in UTC (we use timezone=GMT)
  // When AUTO, API returns times in local timezone
  const bool openMeteoTimeStringsAreUtc = (ramTimezoneMode == TIMEZONE_MODE_MANUAL);

  
  // Parse hourly air quality data
  JsonObject hourly = doc["hourly"];
  JsonArray hourly_time = hourly["time"];
  JsonArray hourly_pm10 = hourly["pm10"];
  JsonArray hourly_pm25 = hourly["pm2_5"];
  JsonArray hourly_co = hourly["carbon_monoxide"];
  JsonArray hourly_no2 = hourly["nitrogen_dioxide"];
  JsonArray hourly_so2 = hourly["sulphur_dioxide"];
  JsonArray hourly_o3 = hourly["ozone"];
  
  int count = min((int)hourly_time.size(), OWM_NUM_AIR_POLLUTION);
  for (int i = 0; i < count; i++) {
    r.dt[i] = parseIso8601(hourly_time[i], openMeteoTimeStringsAreUtc, 0);
    // AQI is calculated locally by calc_aqi() using pollutant concentrations
    // based on the configured AQI_SCALE for the locale
    r.main_aqi[i] = 0;  // Will be calculated from components
    r.components.pm10[i] = hourly_pm10[i].as<float>();
    r.components.pm2_5[i] = hourly_pm25[i].as<float>();
    r.components.co[i] = hourly_co[i].as<float>();
    r.components.no2[i] = hourly_no2[i].as<float>();
    r.components.so2[i] = hourly_so2[i].as<float>();
    r.components.o3[i] = hourly_o3[i].as<float>();
    
    // Default values for unused components (not provided by Open-Meteo)
    r.components.no[i] = 0.0f;
    r.components.nh3[i] = 0.0f;
  }
  
  return error;
} // end deserializeOpenMeteoAirQuality

/*
 * Load API response from saved header file (for offline testing)
 */
#if USE_SAVED_API_DATA
  #include "saved_api_response.h"
#endif

/**
 * Load Open-Meteo forecast from saved header file.
 * Only available when LOAD_API_FROM_HEADER is enabled.
 */
DeserializationError loadOpenMeteoFromHeader(owm_resp_onecall_t &r)
{
#if !USE_SAVED_API_DATA
  return DeserializationError::EmptyInput;
#else
  Serial.println("[Header Loader] Loading forecast from saved_api_response.h");
  
  const char* jsonData = SAVED_API_JSON;
  if (strlen(jsonData) == 0) {
    Serial.println("[Header Loader] ERROR: SAVED_API_JSON is empty!");
    Serial.println("[Header Loader] Please generate the header file using the external script");
    return DeserializationError::EmptyInput;
  }
  
  Serial.println("[Header Loader] JSON size: " + String(strlen(jsonData)) + " bytes");
  
  // Create a temporary stream from the string
  String jsonString = String(jsonData);
  
  // Reuse the existing parsing logic by converting String to a "stream-like" approach
  // We'll parse directly using the String
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonString);
  
  if (error) {
    Serial.println("[Header Loader] JSON parse error: " + String(error.c_str()));
    return error;
  }
  
  // Now populate the response structure using same logic as deserializeOpenMeteo
  // Location data
  r.lat = doc["latitude"].as<float>();
  r.lon = doc["longitude"].as<float>();
  r.timezone = doc["timezone"].as<const char *>();
  r.timezone_offset = doc["utc_offset_seconds"].as<int>();
// When timezone mode is MANUAL, API returns times in UTC (we use timezone=GMT)
  // When AUTO, API returns times in local timezone
  const bool openMeteoTimeStringsAreUtc = (ramTimezoneMode == TIMEZONE_MODE_MANUAL);

  
  // Parse current weather
  JsonObject current = doc["current"];
  const char* timeStr = current["time"];
  if (timeStr != nullptr) {
    r.current.dt = parseIso8601(timeStr, openMeteoTimeStringsAreUtc, r.timezone_offset);
  } else {
    r.current.dt = 0;
  }
  
  // Open-Meteo returns temperatures in Celsius, convert to Kelvin
  r.current.temp = celsius_to_kelvin(current["temperature_2m"].as<float>());
  r.current.feels_like = celsius_to_kelvin(current["apparent_temperature"].as<float>());
  r.current.pressure = current["surface_pressure"].as<int>();
  r.current.humidity = current["relative_humidity_2m"].as<int>();
  r.current.dew_point = celsius_to_kelvin(current["dew_point_2m"].as<float>());
  r.current.clouds = current["cloud_cover"].as<int>();
  r.current.uvi = current["uv_index"].as<float>();
  r.current.visibility = current["visibility"].as<int>();
  r.current.wind_speed = kilometersperhour_to_meterspersecond(current["wind_speed_10m"].as<float>());
  r.current.wind_gust = kilometersperhour_to_meterspersecond(current["wind_gusts_10m"].as<float>());
  r.current.wind_deg = current["wind_direction_10m"].as<int>();
  r.current.rain_1h = current["rain"].as<float>();
  r.current.snow_1h = centimeters_to_millimeters(current["snowfall"].as<float>());
  
  bool isDay = current["is_day"].as<int>() == 1;
  int wmoCode = current["weather_code"].as<int>();
  wmoToOwmWeather(wmoCode, isDay, r.current.weather);
  
  // Parse hourly forecast
  JsonObject hourly = doc["hourly"];
  JsonArray hourly_time = hourly["time"];
  JsonArray hourly_temp = hourly["temperature_2m"];
  JsonArray hourly_feels = hourly["apparent_temperature"];
  JsonArray hourly_humidity = hourly["relative_humidity_2m"];
  JsonArray hourly_pressure = hourly["surface_pressure"];
  JsonArray hourly_clouds = hourly["cloud_cover"];
  JsonArray hourly_wind = hourly["wind_speed_10m"];
  JsonArray hourly_gust = hourly["wind_gusts_10m"];
  JsonArray hourly_deg = hourly["wind_direction_10m"];
#ifdef UNITS_HOURLY_PRECIP_POP
  JsonArray hourly_pop = hourly["precipitation_probability"];
#else
  JsonArray hourly_precip = hourly["precipitation"];
#endif
  JsonArray hourly_code = hourly["weather_code"];
  JsonArray hourly_is_day = hourly["is_day"];
  
  int hourlyCount = min((int)hourly_time.size(), OWM_NUM_HOURLY);
  for (int i = 0; i < hourlyCount; i++) {
    r.hourly[i].dt = parseIso8601(hourly_time[i], openMeteoTimeStringsAreUtc, r.timezone_offset);
    r.hourly[i].temp = celsius_to_kelvin(hourly_temp[i].as<float>());
    r.hourly[i].feels_like = celsius_to_kelvin(hourly_feels[i].as<float>());
    r.hourly[i].humidity = hourly_humidity[i].as<int>();
    r.hourly[i].pressure = hourly_pressure[i].as<int>();
    r.hourly[i].clouds = hourly_clouds[i].as<int>();
    r.hourly[i].wind_speed = kilometersperhour_to_meterspersecond(hourly_wind[i].as<float>());
    r.hourly[i].wind_gust = kilometersperhour_to_meterspersecond(hourly_gust[i].as<float>());
    r.hourly[i].wind_deg = hourly_deg[i].as<int>();
    
#ifdef UNITS_HOURLY_PRECIP_POP
    float pop_val = hourly_pop[i].as<float>();
    r.hourly[i].pop = pop_val / 100.0f;
    r.hourly[i].rain_1h = 0.0f;
    r.hourly[i].snow_1h = 0.0f;
#else
    r.hourly[i].pop = 0.0f;
    r.hourly[i].rain_1h = hourly_precip[i].as<float>();
    r.hourly[i].snow_1h = 0.0f;
#endif
    
    bool hourIsDay = hourly_is_day[i].as<int>() == 1;
    wmoToOwmWeather(hourly_code[i].as<int>(), hourIsDay, r.hourly[i].weather);
    
    r.hourly[i].dew_point = r.hourly[i].temp - 2.0f;
    r.hourly[i].uvi = 0.0f;
    r.hourly[i].visibility = 10000;
  }
  
  // Parse daily forecast
  JsonObject daily = doc["daily"];
  JsonArray daily_time = daily["time"];
  JsonArray daily_max = daily["temperature_2m_max"];
  JsonArray daily_min = daily["temperature_2m_min"];
  JsonArray daily_sunrise = daily["sunrise"];
  JsonArray daily_sunset = daily["sunset"];
  JsonArray daily_code = daily["weather_code"];
  JsonArray daily_pop = daily["precipitation_probability_max"];
  JsonArray daily_precip = daily["precipitation_sum"];
  
  int dailyCount = min((int)daily_time.size(), OWM_NUM_DAILY);
  for (int i = 0; i < dailyCount; i++) {
    r.daily[i].dt = parseIso8601(daily_time[i], openMeteoTimeStringsAreUtc, r.timezone_offset);
    r.daily[i].temp.max = celsius_to_kelvin(daily_max[i].as<float>());
    r.daily[i].temp.min = celsius_to_kelvin(daily_min[i].as<float>());
    r.daily[i].sunrise = parseIso8601(daily_sunrise[i], openMeteoTimeStringsAreUtc, r.timezone_offset);
    r.daily[i].sunset = parseIso8601(daily_sunset[i], openMeteoTimeStringsAreUtc, r.timezone_offset);
    r.daily[i].pop = daily_pop[i].as<float>() / 100.0f;
    
    // Read daily precipitation (mm)
    r.daily[i].rain = daily_precip[i].as<float>();
    r.daily[i].snow = 0.0f; // Open-Meteo separates snowfall if needed
    
    wmoToOwmWeather(daily_code[i].as<int>(), true, r.daily[i].weather);
    
    r.daily[i].moonrise = 0;
    r.daily[i].moonset = 0;
    r.daily[i].moon_phase = 0.0f;
    r.daily[i].temp.morn = r.daily[i].temp.min;
    r.daily[i].temp.day = (r.daily[i].temp.min + r.daily[i].temp.max) / 2.0f;
    r.daily[i].temp.eve = r.daily[i].temp.max;
    r.daily[i].temp.night = r.daily[i].temp.min;
    r.daily[i].feels_like.morn = r.daily[i].temp.morn;
    r.daily[i].feels_like.day = r.daily[i].temp.day;
    r.daily[i].feels_like.eve = r.daily[i].temp.eve;
    r.daily[i].feels_like.night = r.daily[i].temp.night;
    r.daily[i].pressure = 1013;
    r.daily[i].humidity = 60;
    r.daily[i].dew_point = r.daily[i].temp.min - 2.0f;
    r.daily[i].clouds = 50;
    r.daily[i].uvi = 0.0f;
    r.daily[i].visibility = 10000;
    r.daily[i].wind_speed = 5.0f;
    r.daily[i].wind_gust = 10.0f;
    r.daily[i].wind_deg = 0;
  }
  
  r.alerts.clear();
  
  Serial.println("[Header Loader] Successfully loaded data from header");
  Serial.println("[Header Loader] Location: " + String(r.lat, 2) + ", " + String(r.lon, 2));
  
  return error;
#endif // USE_SAVED_API_DATA
}

/**
 * Load Open-Meteo air quality from saved header file.
 * Only available when LOAD_API_FROM_HEADER is enabled.
 */
DeserializationError loadOpenMeteoAirQualityFromHeader(owm_resp_air_pollution_t &r)
{
#if !LOAD_API_FROM_HEADER
  return DeserializationError::EmptyInput;
#else
  Serial.println("[Header Loader] Loading air quality from saved_api_response.h");
  
  const char* jsonData = SAVED_API_JSON;
  if (strlen(jsonData) == 0) {
    Serial.println("[Header Loader] WARNING: SAVED_API_JSON (air quality) is empty, using defaults");
    // Fill with default values
    r.coord.lat = 0;
    r.coord.lon = 0;
    for (int i = 0; i < OWM_NUM_AIR_POLLUTION; i++) {
      r.main_aqi[i] = 1;
      r.components.pm2_5[i] = 10.0f;
      r.components.pm10[i] = 20.0f;
      r.components.o3[i] = 30.0f;
      r.components.no2[i] = 15.0f;
      r.components.so2[i] = 5.0f;
      r.components.co[i] = 400.0f;
      r.components.no[i] = 0.0f;
      r.components.nh3[i] = 0.0f;
      r.dt[i] = 0;
    }
    return DeserializationError::Ok;
  }
  
  String jsonString = String(jsonData);
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonString);
  
  if (error) {
    Serial.println("[Header Loader] JSON parse error: " + String(error.c_str()));
    return error;
  }
  
  // Location
  r.coord.lat = doc["latitude"].as<float>();
  r.coord.lon = doc["longitude"].as<float>();
// When timezone mode is MANUAL, API returns times in UTC (we use timezone=GMT)
  // When AUTO, API returns times in local timezone
  const bool openMeteoTimeStringsAreUtc = (ramTimezoneMode == TIMEZONE_MODE_MANUAL);

  
  // Parse hourly air quality data
  JsonObject hourly = doc["hourly"];
  JsonArray hourly_time = hourly["time"];
  JsonArray hourly_pm10 = hourly["pm10"];
  JsonArray hourly_pm25 = hourly["pm2_5"];
  JsonArray hourly_co = hourly["carbon_monoxide"];
  JsonArray hourly_no2 = hourly["nitrogen_dioxide"];
  JsonArray hourly_so2 = hourly["sulphur_dioxide"];
  JsonArray hourly_o3 = hourly["ozone"];
  
  int count = min((int)hourly_time.size(), OWM_NUM_AIR_POLLUTION);
  for (int i = 0; i < count; i++) {
    r.dt[i] = parseIso8601(hourly_time[i], openMeteoTimeStringsAreUtc, 0);
    // AQI is calculated locally by calc_aqi() using pollutant concentrations
    r.main_aqi[i] = 0;
    r.components.pm10[i] = hourly_pm10[i].as<float>();
    r.components.pm2_5[i] = hourly_pm25[i].as<float>();
    r.components.co[i] = hourly_co[i].as<float>();
    r.components.no2[i] = hourly_no2[i].as<float>();
    r.components.so2[i] = hourly_so2[i].as<float>();
    r.components.o3[i] = hourly_o3[i].as<float>();
    r.components.no[i] = 0.0f;
    r.components.nh3[i] = 0.0f;
  }
  
  Serial.println("[Header Loader] Successfully loaded air quality from header");
  return error;
#endif // USE_SAVED_API_DATA
}
