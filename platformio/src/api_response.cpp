/* API response deserialization for esp32-weather-epd.
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

#include <vector>
#include <ArduinoJson.h>
#include "api_response.h"
#include "config.h"

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
    r.hourly[i].dt         = hourly["dt"]        .as<int64_t>();
    r.hourly[i].temp       = hourly["temp"]      .as<float>();
    r.hourly[i].feels_like = hourly["feels_like"].as<float>();
    r.hourly[i].pressure   = hourly["pressure"]  .as<int>();
    r.hourly[i].humidity   = hourly["humidity"]  .as<int>();
    r.hourly[i].dew_point  = hourly["dew_point"] .as<float>();
    r.hourly[i].clouds     = hourly["clouds"]    .as<int>();
    r.hourly[i].uvi        = hourly["uvi"]       .as<float>();
    r.hourly[i].visibility = hourly["visibility"].as<int>();
    r.hourly[i].wind_speed = hourly["wind_speed"].as<float>();
    r.hourly[i].wind_gust  = hourly["wind_gust"] .as<float>();
    r.hourly[i].wind_deg   = hourly["wind_deg"]  .as<int>();
    r.hourly[i].pop        = hourly["pop"]       .as<float>();
    r.hourly[i].rain_1h    = hourly["rain"]["1h"].as<float>();
    r.hourly[i].snow_1h    = hourly["snow"]["1h"].as<float>();
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
    r.daily[i].dt         = daily["dt"]        .as<int64_t>();
    r.daily[i].sunrise    = daily["sunrise"]   .as<int64_t>();
    r.daily[i].sunset     = daily["sunset"]    .as<int64_t>();
    r.daily[i].moonrise   = daily["moonrise"]  .as<int64_t>();
    r.daily[i].moonset    = daily["moonset"]   .as<int64_t>();
    r.daily[i].moon_phase = daily["moon_phase"].as<float>();
    JsonObject daily_temp = daily["temp"];
    r.daily[i].temp.morn  = daily_temp["morn"] .as<float>();
    r.daily[i].temp.day   = daily_temp["day"]  .as<float>();
    r.daily[i].temp.eve   = daily_temp["eve"]  .as<float>();
    r.daily[i].temp.night = daily_temp["night"].as<float>();
    r.daily[i].temp.min   = daily_temp["min"]  .as<float>();
    r.daily[i].temp.max   = daily_temp["max"]  .as<float>();
    JsonObject daily_feels_like = daily["feels_like"];
    r.daily[i].feels_like.morn  = daily_feels_like["morn"] .as<float>();
    r.daily[i].feels_like.day   = daily_feels_like["day"]  .as<float>();
    r.daily[i].feels_like.eve   = daily_feels_like["eve"]  .as<float>();
    r.daily[i].feels_like.night = daily_feels_like["night"].as<float>();
    r.daily[i].pressure   = daily["pressure"]  .as<int>();
    r.daily[i].humidity   = daily["humidity"]  .as<int>();
    r.daily[i].dew_point  = daily["dew_point"] .as<float>();
    r.daily[i].clouds     = daily["clouds"]    .as<int>();
    r.daily[i].uvi        = daily["uvi"]       .as<float>();
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
  for (JsonObject alerts : doc["alerts"].as<JsonArray>())
  {
    owm_alerts_t new_alert = {};
    // new_alert.sender_name = alerts["sender_name"].as<const char *>();
    new_alert.event       = alerts["event"]      .as<const char *>();
    new_alert.start       = alerts["start"]      .as<int64_t>();
    new_alert.end         = alerts["end"]        .as<int64_t>();
    // new_alert.description = alerts["description"].as<const char *>();
    new_alert.tags        = alerts["tags"][0]    .as<const char *>();
    r.alerts.push_back(new_alert);

    if (i == OWM_NUM_ALERTS - 1)
    {
      break;
    }
    ++i;
  }
#endif

  return error;
} // end deserializeOneCall

DeserializationError deserializeAirQuality(WiFiClient &json,
                                           owm_resp_air_pollution_t &r)
{
  int i = 0;

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json);
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

  r.coord.lat = doc["coord"]["lat"].as<float>();
  r.coord.lon = doc["coord"]["lon"].as<float>();

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

/*
 * Open-Meteo API support functions
 */

/**
 * Parse ISO 8601 datetime string to Unix timestamp.
 * Format: "2026-03-24T18:15"
 * Note: This is a simplified parser assuming UTC input from Open-Meteo
 * with timezone offset applied separately.
 */
int64_t parseIso8601(const char* iso8601)
{
  struct tm tm = {0};
  int year, month, day, hour, minute;
  
  // Parse format: YYYY-MM-DDTHH:MM
  if (sscanf(iso8601, "%4d-%2d-%2dT%2d:%2d", 
             &year, &month, &day, &hour, &minute) != 5) {
    return 0;
  }
  
  tm.tm_year = year - 1900;
  tm.tm_mon = month - 1;
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = minute;
  tm.tm_sec = 0;
  tm.tm_isdst = -1;
  
  return mktime(&tm);
} // end parseIso8601

/**
 * Convert WMO Weather Interpretation code to OpenWeatherMap-compatible structure.
 * WMO codes: https://open-meteo.com/en/docs
 * OWM codes: https://openweathermap.org/weather-conditions
 */
void wmoToOwmWeather(int wmoCode, bool isDay, owm_weather_t &weather)
{
  // Default values
  weather.id = 800;
  weather.main = "Clear";
  weather.description = "clear sky";
  weather.icon = isDay ? "01d" : "01n";
  
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
      weather.icon = isDay ? "50d" : "50n";
      break;
    case 51: // Light drizzle
      weather.id = 300;
      weather.main = "Drizzle";
      weather.description = "light drizzle";
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
      weather.description = "heavy drizzle";
      weather.icon = isDay ? "09d" : "09n";
      break;
    case 56: // Light freezing drizzle
      weather.id = 310;
      weather.main = "Rain";
      weather.description = "light freezing rain";
      weather.icon = isDay ? "09d" : "09n";
      break;
    case 57: // Dense freezing drizzle
      weather.id = 310;
      weather.main = "Rain";
      weather.description = "freezing rain";
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
      weather.description = "heavy rain";
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
    case 71: // Slight snow
      weather.id = 600;
      weather.main = "Snow";
      weather.description = "light snow";
      weather.icon = isDay ? "13d" : "13n";
      break;
    case 73: // Moderate snow
      weather.id = 601;
      weather.main = "Snow";
      weather.description = "snow";
      weather.icon = isDay ? "13d" : "13n";
      break;
    case 75: // Heavy snow
      weather.id = 602;
      weather.main = "Snow";
      weather.description = "heavy snow";
      weather.icon = isDay ? "13d" : "13n";
      break;
    case 77: // Snow grains
      weather.id = 600;
      weather.main = "Snow";
      weather.description = "snow grains";
      weather.icon = isDay ? "13d" : "13n";
      break;
    case 80: // Slight rain showers
      weather.id = 520;
      weather.main = "Rain";
      weather.description = "light shower rain";
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
      weather.description = "heavy shower rain";
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
      weather.description = "thunderstorm";
      weather.icon = isDay ? "11d" : "11n";
      break;
    case 96: // Thunderstorm with hail
      weather.id = 201;
      weather.main = "Thunderstorm";
      weather.description = "thunderstorm with hail";
      weather.icon = isDay ? "11d" : "11n";
      break;
    case 99: // Thunderstorm with heavy hail
      weather.id = 202;
      weather.main = "Thunderstorm";
      weather.description = "heavy thunderstorm";
      weather.icon = isDay ? "11d" : "11n";
      break;
    default:
      // Keep default clear sky values
      break;
  }
} // end wmoToOwmWeather

/**
 * Deserialize Open-Meteo API response.
 * Converts Open-Meteo format to the existing owm_resp_onecall_t structure.
 */
DeserializationError deserializeOpenMeteo(WiFiClient &json,
                                          owm_resp_onecall_t &r)
{
  Serial.println("DEBUG: deserializeOpenMeteo started");
  
  // Read entire response into String first for debugging
  Serial.println("DEBUG: Reading HTTP response into buffer...");
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
        if (bytesRead % 1000 == 0) {
          Serial.println("DEBUG: Read " + String(bytesRead) + " bytes so far...");
        }
      }
    } else {
      // Stream empty, wait a bit for more data
      delay(10);
    }
  }
  Serial.println("DEBUG: Received " + String(jsonString.length()) + " bytes");
  
  // Find where JSON actually starts (first '{' character)
  int jsonStart = jsonString.indexOf('{');
  if (jsonStart > 0) {
    Serial.println("DEBUG: Found JSON start at position " + String(jsonStart));
    Serial.println("DEBUG: Skipping " + String(jsonStart) + " bytes of HTTP headers");
    jsonString = jsonString.substring(jsonStart);
  } else if (jsonStart == 0) {
    Serial.println("DEBUG: JSON starts at position 0 (no headers to skip)");
  } else {
    Serial.println("DEBUG: ERROR - No '{' found in response!");
  }
  
  Serial.println("DEBUG: First 200 chars after cleaning: " + jsonString.substring(0, 200));
  
  // Find actual end of JSON (last })
  int lastBrace = jsonString.lastIndexOf('}');
  if (lastBrace > 0 && lastBrace < jsonString.length() - 1) {
    Serial.println("DEBUG: Trimming extra data after position " + String(lastBrace));
    jsonString = jsonString.substring(0, lastBrace + 1);
  }
  
  Serial.println("DEBUG: Last 100 chars: " + jsonString.substring(jsonString.length() - 100));
  
  // Check if JSON is complete
  if (jsonString[jsonString.length() - 1] == '}') {
    Serial.println("DEBUG: JSON appears complete (ends with })");
  } else {
    Serial.println("DEBUG: WARNING - JSON may be truncated!");
  }
  
  Serial.println("DEBUG: Creating JsonDocument...");
  Serial.println("DEBUG: Free heap before JSON: " + String(ESP.getFreeHeap()));
  
  // ArduinoJson 7 - parse without filter
  // Allocate with enough capacity - Open-Meteo response is ~15KB
  JsonDocument doc;

  Serial.println("DEBUG: Starting deserializeJson...");
  Serial.println("DEBUG: JSON string length: " + String(jsonString.length()));
  
  // Check if we got valid JSON
  if (jsonString.length() == 0) {
    Serial.println("DEBUG: ERROR - Empty JSON string!");
  }
  if (jsonString[0] != '{') {
    Serial.println("DEBUG: ERROR - JSON doesn't start with '{'!");
    Serial.println("DEBUG: First char: " + String(jsonString[0]));
  }
  
  // Try to validate JSON by checking brackets
  int openBraces = 0, closeBraces = 0;
  for (int i = 0; i < jsonString.length(); i++) {
    if (jsonString[i] == '{') openBraces++;
    if (jsonString[i] == '}') closeBraces++;
  }
  Serial.println("DEBUG: Open braces: " + String(openBraces) + ", Close braces: " + String(closeBraces));
  
  // Use Stream instead of String to save memory
  Serial.println("DEBUG: Converting to Stream for parsing...");
  Serial.println("DEBUG: Free heap before deserialize: " + String(ESP.getFreeHeap()));
  
  // Temporarily remove filter to test if it's causing issues
  Serial.println("DEBUG: Parsing WITHOUT filter...");
  DeserializationError error = deserializeJson(doc, jsonString);
  Serial.println("DEBUG: deserializeJson completed");
  Serial.println("DEBUG: JSON doc size: " + String(doc.size()));
  Serial.println("DEBUG: Free heap after deserialize: " + String(ESP.getFreeHeap()));
  
  if (error) {
    Serial.println("DEBUG: JSON Error Code: " + String(static_cast<int>(error.code())));
    Serial.println("DEBUG: JSON Error: " + String(error.c_str()));
    
    // Try to print what we can access anyway
    Serial.println("DEBUG: Attempting to access partial data...");
    Serial.println("DEBUG: doc.containsKey('latitude'): " + String(doc.containsKey("latitude") ? "yes" : "no"));
    if (doc.containsKey("latitude")) {
      Serial.println("DEBUG: latitude value: " + String(doc["latitude"].as<float>()));
    }
  }
  
  // Debug: Print some raw JSON values
  Serial.println("DEBUG: Raw latitude from JSON: " + String(doc["latitude"].as<float>()));
  Serial.println("DEBUG: Raw longitude from JSON: " + String(doc["longitude"].as<float>()));
  
  JsonObject current_debug = doc["current"];
  Serial.println("DEBUG: Raw temp from JSON: " + String(current_debug["temperature_2m"].as<float>()));
  Serial.println("DEBUG: Raw humidity from JSON: " + String(current_debug["relative_humidity_2m"].as<int>()));
  Serial.println("DEBUG: Raw weather_code from JSON: " + String(current_debug["weather_code"].as<int>()));
  
#if DEBUG_LEVEL >= 1
  Serial.println("[debug] doc.overflowed() : "
                 + String(doc.overflowed()));
#endif
#if DEBUG_LEVEL >= 2
  serializeJsonPretty(doc, Serial);
#endif
  if (error && doc.size() == 0) {
    Serial.println("DEBUG: Critical error - no data available");
    return error;
  }
  
  if (error) {
    Serial.println("DEBUG: Partial error - using available data");
    // Continue with partial data
  }

  Serial.println("DEBUG: Parsing location data...");
  // Location data
  r.lat = doc["latitude"].as<float>();
  r.lon = doc["longitude"].as<float>();
  r.timezone = doc["timezone"].as<const char *>();
  r.timezone_offset = doc["utc_offset_seconds"].as<int>();

  Serial.println("DEBUG: Parsing current weather...");
  // Parse current weather
  Serial.println("DEBUG: Checking if doc['current'] exists...");
  if (doc["current"].isNull()) {
    Serial.println("DEBUG: ERROR - doc['current'] is null!");
  } else {
    Serial.println("DEBUG: doc['current'] exists");
  }
  JsonObject current = doc["current"];
  
  Serial.println("DEBUG: Getting time...");
  const char* timeStr = current["time"];
  Serial.println("DEBUG: timeStr = " + String(timeStr ? timeStr : "null"));
  if (timeStr != nullptr) {
    r.current.dt = parseIso8601(timeStr);
  } else {
    r.current.dt = 0; // Fallback to 0 if time is not available
  }
  
  Serial.println("DEBUG: Getting temperature...");
  if (current["temperature_2m"].isNull()) {
    Serial.println("DEBUG: WARNING - temperature_2m is null");
  }
  r.current.temp = current["temperature_2m"].as<float>();
  Serial.println("DEBUG: temperature_2m raw: " + String(current["temperature_2m"].as<float>()));
  Serial.println("DEBUG: r.current.temp after assignment: " + String(r.current.temp));
  
  Serial.println("DEBUG: Getting apparent_temperature...");
  r.current.feels_like = current["apparent_temperature"].as<float>();
  
  Serial.println("DEBUG: Getting surface_pressure...");
  r.current.pressure = current["surface_pressure"].as<int>();
  
  Serial.println("DEBUG: Getting relative_humidity_2m...");
  r.current.humidity = current["relative_humidity_2m"].as<int>();
  
  Serial.println("DEBUG: Getting dew_point_2m...");
  r.current.dew_point = current["dew_point_2m"].as<float>();
  
  Serial.println("DEBUG: Getting cloud_cover...");
  r.current.clouds = current["cloud_cover"].as<int>();
  
  Serial.println("DEBUG: Getting uv_index...");
  r.current.uvi = current["uv_index"].as<float>();
  
  Serial.println("DEBUG: Getting visibility...");
  r.current.visibility = current["visibility"].as<int>();
  
  Serial.println("DEBUG: Getting wind_speed_10m...");
  r.current.wind_speed = current["wind_speed_10m"].as<float>();
  
  Serial.println("DEBUG: Getting wind_gusts_10m...");
  r.current.wind_gust = current["wind_gusts_10m"].as<float>();
  
  Serial.println("DEBUG: Getting wind_direction_10m...");
  r.current.wind_deg = current["wind_direction_10m"].as<int>();
  
  Serial.println("DEBUG: Getting rain...");
  r.current.rain_1h = current["rain"].as<float>();
  
  Serial.println("DEBUG: Getting snowfall...");
  r.current.snow_1h = current["snowfall"].as<float>() * 10.0f; // cm to mm
  
  // Debug: Verify current data was stored
  Serial.println("DEBUG: After parsing current:");
  Serial.println("  temp: " + String(r.current.temp));
  Serial.println("  humidity: " + String(r.current.humidity));
  Serial.println("  pressure: " + String(r.current.pressure));
  Serial.println("  wind_speed: " + String(r.current.wind_speed));
  Serial.println("  weather.id: " + String(r.current.weather.id));

  // Sunrise/sunset not in Open-Meteo current, will use first daily value
  r.current.sunrise = 0;
  r.current.sunset = 0;

  // Convert WMO weather code to OWM format
  int wmoCurrent = current["weather_code"].as<int>();
  // Assume daytime for current weather (6:00-18:00)
  bool isDay = true;
  if (r.current.dt > 0) {
    time_t t = r.current.dt;
    struct tm *timeInfo = gmtime(&t);
    int hour = timeInfo->tm_hour;
    isDay = (hour >= 6 && hour < 18);
  }
  Serial.println("DEBUG: Converting WMO weather code...");
  wmoToOwmWeather(wmoCurrent, isDay, r.current.weather);
  Serial.println("DEBUG: Current weather parsed");

  Serial.println("DEBUG: Parsing hourly forecast...");
  
  // Check if hourly exists and is accessible
  Serial.println("DEBUG: Checking hourly object...");
  if (doc["hourly"].isNull()) {
    Serial.println("DEBUG: ERROR - hourly is null!");
  } else {
    Serial.println("DEBUG: hourly object exists");
    Serial.println("DEBUG: hourly type: " + String(doc["hourly"].is<JsonObject>() ? "object" : "other"));
  }
  
  // Parse hourly forecast (arrays)
  Serial.println("DEBUG: Accessing hourly arrays...");
  
  JsonObject hourly_obj = doc["hourly"];
  Serial.println("DEBUG: hourly_obj.size() = " + String(hourly_obj.size()));
  
  // List all keys in hourly
  Serial.println("DEBUG: Keys in hourly:");
  for (JsonPair kv : hourly_obj) {
    Serial.println("  - " + String(kv.key().c_str()));
  }
  
  JsonArray hourly_time = hourly_obj["time"];
  JsonArray hourly_temp = hourly_obj["temperature_2m"];
  
  Serial.println("DEBUG: Checking if temperature_2m exists...");
  if (hourly_obj["temperature_2m"].isNull()) {
    Serial.println("DEBUG: ERROR - temperature_2m is null!");
  } else {
    Serial.println("DEBUG: temperature_2m type: " + String(hourly_obj["temperature_2m"].is<JsonArray>() ? "array" : "other"));
    Serial.println("DEBUG: temperature_2m[0] raw: " + String(hourly_obj["temperature_2m"][0].as<float>()));
  }
  
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
  JsonArray hourly_rain = hourly_obj["rain"];
  JsonArray hourly_snow = hourly_obj["snowfall"];
  JsonArray hourly_weather = hourly_obj["weather_code"];

  Serial.println("DEBUG: hourly_time.size() = " + String(hourly_time.size()));
  
  int hourlyCount = min((int)OWM_NUM_HOURLY, (int)hourly_time.size());
  Serial.println("DEBUG: hourlyCount = " + String(hourlyCount));
  
  for (int i = 0; i < hourlyCount && i < 3; i++) { // Limit to first 3 for debug
    Serial.println("DEBUG: Parsing hourly[" + String(i) + "]...");
    r.hourly[i].dt = parseIso8601(hourly_time[i]);
    Serial.println("DEBUG: hourly[" + String(i) + "].dt = " + String(r.hourly[i].dt));
    r.hourly[i].temp = hourly_temp[i].as<float>();
    Serial.println("DEBUG: hourly[" + String(i) + "].temp = " + String(r.hourly[i].temp));
    r.hourly[i].feels_like = hourly_feels[i].as<float>();
    r.hourly[i].humidity = hourly_humidity[i].as<int>();
    r.hourly[i].pressure = hourly_pressure[i].as<int>();
    r.hourly[i].clouds = hourly_clouds[i].as<int>();
    r.hourly[i].uvi = hourly_uv[i].as<float>();
    r.hourly[i].visibility = hourly_visibility[i].as<int>();
    r.hourly[i].dew_point = hourly_dew[i].as<float>();
    r.hourly[i].wind_speed = hourly_wind_speed[i].as<float>();
    r.hourly[i].wind_gust = hourly_wind_gust[i].as<float>();
    r.hourly[i].wind_deg = hourly_wind_deg[i].as<int>();
    r.hourly[i].pop = hourly_pop[i].as<float>() / 100.0f; // percentage to 0-1
    r.hourly[i].rain_1h = hourly_rain[i].as<float>();
    r.hourly[i].snow_1h = hourly_snow[i].as<float>() * 10.0f; // cm to mm
    
    int wmoHourly = hourly_weather[i].as<int>();
    // Determine if day or night for this hour
    bool hourlyIsDay = true;
    if (r.hourly[i].dt > 0) {
      time_t t = r.hourly[i].dt;
      struct tm *timeInfo = gmtime(&t);
      int hour = timeInfo->tm_hour;
      hourlyIsDay = (hour >= 6 && hour < 18);
    }
    wmoToOwmWeather(wmoHourly, hourlyIsDay, r.hourly[i].weather);
  }
  // Note: hourly sunrise/sunset not available from Open-Meteo

  // Parse daily forecast
  Serial.println("DEBUG: Checking daily object...");
  if (doc["daily"].isNull()) {
    Serial.println("DEBUG: ERROR - daily is null!");
    // Check if 'daily' key exists in doc
    bool hasDaily = false;
    for (JsonPair kv : doc.as<JsonObject>()) {
      if (strcmp(kv.key().c_str(), "daily") == 0) {
        hasDaily = true;
        break;
      }
    }
    Serial.println("DEBUG: 'daily' key exists in doc: " + String(hasDaily ? "yes" : "no"));
    
    // List all keys in doc
    Serial.println("DEBUG: All keys in doc:");
    for (JsonPair kv : doc.as<JsonObject>()) {
      Serial.println("  - " + String(kv.key().c_str()));
    }
  } else {
    Serial.println("DEBUG: daily object exists");
    JsonObject daily_obj = doc["daily"];
    Serial.println("DEBUG: daily.size() = " + String(daily_obj.size()));
    Serial.println("DEBUG: Keys in daily:");
    for (JsonPair kv : daily_obj) {
      Serial.println("  - " + String(kv.key().c_str()));
    }
  }
  
  JsonArray daily_time = doc["daily"]["time"];
  JsonArray daily_weather = doc["daily"]["weather_code"];
  JsonArray daily_max = doc["daily"]["temperature_2m_max"];
  JsonArray daily_min = doc["daily"]["temperature_2m_min"];
  JsonArray daily_sunrise = doc["daily"]["sunrise"];
  JsonArray daily_sunset = doc["daily"]["sunset"];
  JsonArray daily_uv_max = doc["daily"]["uv_index_max"];
  JsonArray daily_precip_sum = doc["daily"]["precipitation_sum"];
  JsonArray daily_precip_prob = doc["daily"]["precipitation_probability_max"];

  Serial.println("DEBUG: daily_time.size() = " + String(daily_time.size()));
  int dailyCount = min((int)OWM_NUM_DAILY, (int)daily_time.size());
  Serial.println("DEBUG: dailyCount = " + String(dailyCount));
  for (int i = 0; i < dailyCount; i++) {
    r.daily[i].dt = parseIso8601(daily_time[i]);
    r.daily[i].sunrise = parseIso8601(daily_sunrise[i]);
    r.daily[i].sunset = parseIso8601(daily_sunset[i]);
    r.daily[i].temp.max = daily_max[i].as<float>();
    r.daily[i].temp.min = daily_min[i].as<float>();
    // Approximate values for missing fields
    r.daily[i].temp.morn = daily_min[i].as<float>();
    r.daily[i].temp.day = (daily_max[i].as<float>() + daily_min[i].as<float>()) / 2.0f;
    r.daily[i].temp.eve = daily_max[i].as<float>();
    r.daily[i].temp.night = daily_min[i].as<float>();
    r.daily[i].uvi = daily_uv_max[i].as<float>();
    r.daily[i].rain = daily_precip_sum[i].as<float>();
    r.daily[i].pop = daily_precip_prob[i].as<float>() / 100.0f;
    
    // Moon data not available from Open-Meteo
    r.daily[i].moonrise = 0;
    r.daily[i].moonset = 0;
    r.daily[i].moon_phase = 0.0f;
    
    // Feels-like not available in daily, use average
    r.daily[i].feels_like.morn = daily_min[i].as<float>();
    r.daily[i].feels_like.day = r.daily[i].temp.day;
    r.daily[i].feels_like.eve = daily_max[i].as<float>();
    r.daily[i].feels_like.night = daily_min[i].as<float>();
    
    // Use max temp day values for other fields
    r.daily[i].pressure = r.current.pressure; // Use current as approximation
    r.daily[i].humidity = r.current.humidity;
    r.daily[i].dew_point = r.current.dew_point;
    r.daily[i].clouds = r.current.clouds;
    r.daily[i].visibility = r.current.visibility;
    r.daily[i].wind_speed = r.current.wind_speed;
    r.daily[i].wind_gust = r.current.wind_gust;
    r.daily[i].wind_deg = r.current.wind_deg;
    r.daily[i].snow = 0.0f;
    
    int wmoDaily = daily_weather[i].as<int>();
    wmoToOwmWeather(wmoDaily, true, r.daily[i].weather); // always day icon for daily
  }

  // Update current sunrise/sunset from first daily entry
  if (dailyCount > 0) {
    r.current.sunrise = r.daily[0].sunrise;
    r.current.sunset = r.daily[0].sunset;
  }

  Serial.println("DEBUG: Clearing alerts...");
  // Clear alerts (not supported by Open-Meteo)
  r.alerts.clear();

  // Debug: Final verification
  Serial.println("DEBUG: Final data verification:");
  Serial.println("  lat: " + String(r.lat));
  Serial.println("  lon: " + String(r.lon));
  Serial.println("  current.temp: " + String(r.current.temp));
  Serial.println("  current.humidity: " + String(r.current.humidity));
  Serial.println("  current.pressure: " + String(r.current.pressure));
  Serial.println("  daily[0].temp.max: " + String(r.daily[0].temp.max));
  Serial.println("  daily[0].temp.min: " + String(r.daily[0].temp.min));
  Serial.println("  hourly[0].temp: " + String(r.hourly[0].temp));

  // Debug: Verify one more time before returning
  Serial.println("DEBUG: Right before returning:");
  Serial.println("  r.current.temp: " + String(r.current.temp));
  Serial.println("  r.current.humidity: " + String(r.current.humidity));
  Serial.println("  r.current.pressure: " + String(r.current.pressure));
  
  Serial.println("DEBUG: deserializeOpenMeteo completed successfully");
  return error;
} // end deserializeOpenMeteo

