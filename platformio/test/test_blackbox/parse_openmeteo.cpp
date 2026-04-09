/* Parse Open-Meteo JSON → owm_resp_onecall_t for Blackbox Testing
 * Copyright (C) 2025
 *
 * Parses Open-Meteo API JSON and converts to firmware's owm_resp_onecall_t format.
 * Converts units: Celsius→Kelvin, km/h→m/s, km→m, %→0.0-1.0, WMO→OWM codes
 */

#include "parse_openmeteo.h"
#include "api_response_compat.h"
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>

// ============================================================================
// WMO to OWM Weather Conversion Implementation
// This provides the implementation for the declaration in api_response.h
// ============================================================================

void wmoToOwmWeather(int wmoCode, bool isDay, owm_weather_t& weather) {
    (void)isDay;
    
    if (wmoCode == 0) {
        weather.id = 800;
        weather.main = "Clear";
        weather.description = "clear sky";
        weather.icon = "01d";
    } else if (wmoCode == 1) {
        weather.id = 801;
        weather.main = "Clouds";
        weather.description = "few clouds";
        weather.icon = "02d";
    } else if (wmoCode == 2) {
        weather.id = 802;
        weather.main = "Clouds";
        weather.description = "scattered clouds";
        weather.icon = "03d";
    } else if (wmoCode == 3) {
        weather.id = 803;
        weather.main = "Clouds";
        weather.description = "overcast clouds";
        weather.icon = "04d";
    } else if (wmoCode >= 51 && wmoCode <= 55) {
        weather.id = 300;
        weather.main = "Drizzle";
        weather.description = "drizzle";
        weather.icon = "09d";
    } else if (wmoCode >= 61 && wmoCode <= 65) {
        weather.id = 500;
        weather.main = "Rain";
        weather.description = "rain";
        weather.icon = "10d";
    } else if (wmoCode >= 71 && wmoCode <= 77) {
        weather.id = 600;
        weather.main = "Snow";
        weather.description = "snow";
        weather.icon = "13d";
    } else if (wmoCode >= 80 && wmoCode <= 82) {
        weather.id = 520;
        weather.main = "Rain";
        weather.description = "rain showers";
        weather.icon = "09d";
    } else if (wmoCode == 95 || wmoCode == 96 || wmoCode == 99) {
        weather.id = 200;
        weather.main = "Thunderstorm";
        weather.description = "thunderstorm";
        weather.icon = "11d";
    } else {
        weather.id = 804;
        weather.main = "Clouds";
        weather.description = "cloudy";
        weather.icon = "04d";
    }
}

namespace blackbox {

// ============================================================================
// String Helpers
// ============================================================================

static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

static std::string extractJsonValue(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";
    
    size_t colonPos = json.find(":", keyPos);
    if (colonPos == std::string::npos) return "";
    
    size_t valueStart = colonPos + 1;
    while (valueStart < json.length() && std::isspace(json[valueStart])) {
        valueStart++;
    }
    
    // Handle different value types
    if (json[valueStart] == '"') {
        // String value
        size_t valueEnd = json.find("\"", valueStart + 1);
        return json.substr(valueStart + 1, valueEnd - valueStart - 1);
    } else if (json[valueStart] == '[') {
        // Array value
        size_t valueEnd = json.find("]", valueStart);
        return json.substr(valueStart, valueEnd - valueStart + 1);
    } else {
        // Number or other value
        size_t valueEnd = valueStart;
        while (valueEnd < json.length() && 
               (std::isdigit(json[valueEnd]) || json[valueEnd] == '.' || 
                json[valueEnd] == '-' || json[valueEnd] == 'e' || json[valueEnd] == 'E')) {
            valueEnd++;
        }
        return json.substr(valueStart, valueEnd - valueStart);
    }
}

static std::string extractObject(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";
    
    size_t colonPos = json.find(":", keyPos);
    if (colonPos == std::string::npos) return "";
    
    size_t objStart = json.find("{", colonPos);
    if (objStart == std::string::npos) return "";
    
    // Find matching closing brace
    int braceCount = 1;
    size_t objEnd = objStart + 1;
    while (objEnd < json.length() && braceCount > 0) {
        if (json[objEnd] == '{') braceCount++;
        else if (json[objEnd] == '}') braceCount--;
        objEnd++;
    }
    
    return json.substr(objStart, objEnd - objStart);
}

// ============================================================================
// Array Parsing
// ============================================================================

std::vector<float> parseFloatArray(const std::string& jsonArray) {
    std::vector<float> result;
    std::string content = trim(jsonArray);
    
    if (content.empty() || content[0] != '[') return result;
    
    // Remove brackets
    content = content.substr(1, content.length() - 2);
    
    std::stringstream ss(content);
    std::string item;
    
    while (std::getline(ss, item, ',')) {
        item = trim(item);
        if (!item.empty() && item != "null") {
            result.push_back(std::stof(item));
        } else {
            result.push_back(0.0f);
        }
    }
    
    return result;
}

std::vector<int> parseIntArray(const std::string& jsonArray) {
    std::vector<int> result;
    std::string content = trim(jsonArray);
    
    if (content.empty() || content[0] != '[') return result;
    
    // Remove brackets
    content = content.substr(1, content.length() - 2);
    
    std::stringstream ss(content);
    std::string item;
    
    while (std::getline(ss, item, ',')) {
        item = trim(item);
        if (!item.empty() && item != "null") {
            result.push_back(std::stoi(item));
        } else {
            result.push_back(0);
        }
    }
    
    return result;
}

std::vector<std::string> parseStringArray(const std::string& jsonArray) {
    std::vector<std::string> result;
    std::string content = trim(jsonArray);
    
    if (content.empty() || content[0] != '[') return result;
    
    // Remove brackets
    content = content.substr(1, content.length() - 2);
    
    std::stringstream ss(content);
    std::string item;
    
    while (std::getline(ss, item, ',')) {
        item = trim(item);
        // Remove quotes if present
        if (item.length() >= 2 && item[0] == '"' && item[item.length()-1] == '"') {
            item = item.substr(1, item.length() - 2);
        }
        result.push_back(item);
    }
    
    return result;
}

// ============================================================================
// ISO 8601 Timestamp Parser
// ============================================================================

static int64_t parseIso8601ToTimestamp(const std::string& iso8601) {
    // Parse "2025-01-15T12:00" format
    if (iso8601.length() < 10) return 0;
    
    struct tm timeinfo = {};
    
    // Parse date part: YYYY-MM-DD
    timeinfo.tm_year = std::stoi(iso8601.substr(0, 4)) - 1900;
    timeinfo.tm_mon = std::stoi(iso8601.substr(5, 2)) - 1;
    timeinfo.tm_mday = std::stoi(iso8601.substr(8, 2));
    
    // Parse time part if present: HH:MM
    if (iso8601.length() >= 16 && iso8601[10] == 'T') {
        timeinfo.tm_hour = std::stoi(iso8601.substr(11, 2));
        timeinfo.tm_min = std::stoi(iso8601.substr(14, 2));
    }
    
    timeinfo.tm_isdst = -1;
    
    return static_cast<int64_t>(mktime(&timeinfo));
}

// ============================================================================
// Data Conversion
// ============================================================================

/**
 * Parse hourly data and convert to owm_hourly_t array
 * Conversions:
 * - Temperature: Celsius → Kelvin
 * - Wind speed: km/h → m/s
 * - Visibility: km → meters
 * - POP: % (0-100) → 0.0-1.0
 * - Weather code: WMO → OWM
 */
static void parseAndConvertHourly(
    const std::string& jsonSection,
    owm_hourly_t* hourlyArray,
    int maxHours) {
    
    std::vector<std::string> times = parseStringArray(extractJsonValue(jsonSection, "time"));
    std::vector<float> temps = parseFloatArray(extractJsonValue(jsonSection, "temperature_2m"));
    std::vector<int> humidity = parseIntArray(extractJsonValue(jsonSection, "relative_humidity_2m"));
    std::vector<int> pop = parseIntArray(extractJsonValue(jsonSection, "precipitation_probability"));
    std::vector<float> precipitation = parseFloatArray(extractJsonValue(jsonSection, "precipitation"));
    std::vector<float> rain = parseFloatArray(extractJsonValue(jsonSection, "rain"));
    std::vector<float> showers = parseFloatArray(extractJsonValue(jsonSection, "showers"));
    std::vector<float> snowfall = parseFloatArray(extractJsonValue(jsonSection, "snowfall"));
    std::vector<int> weatherCode = parseIntArray(extractJsonValue(jsonSection, "weather_code"));
    std::vector<float> windSpeed = parseFloatArray(extractJsonValue(jsonSection, "wind_speed_10m"));
    std::vector<float> pressure = parseFloatArray(extractJsonValue(jsonSection, "surface_pressure"));
    std::vector<float> visibility = parseFloatArray(extractJsonValue(jsonSection, "visibility"));
    
    int numHours = std::min(static_cast<int>(times.size()), maxHours);
    
    for (int i = 0; i < numHours; i++) {
        owm_hourly_t& h = hourlyArray[i];
        
        // Timestamp
        h.dt = parseIso8601ToTimestamp(times[i]);
        
        // Temperature: Celsius → Kelvin
        if (i < temps.size()) {
            h.temp = celsius_to_kelvin(temps[i]);
            h.feels_like = h.temp;  // Open-Meteo doesn't provide feels_like directly
        }
        
        // Humidity: same units
        if (i < humidity.size()) {
            h.humidity = humidity[i];
        }
        
        // Pressure: hPa (same)
        if (i < pressure.size()) {
            h.pressure = static_cast<int>(pressure[i]);
        }
        
        // Visibility: km → meters
        if (i < visibility.size()) {
            h.visibility = static_cast<int>(visibility[i] * 1000);
        }
        
        // Wind speed: km/h → m/s
        if (i < windSpeed.size()) {
            h.wind_speed = kilometersperhour_to_meterspersecond(windSpeed[i]);
            h.wind_gust = h.wind_speed * 1.5f;  // Estimate gust as 1.5x average
        }
        
        // POP: % → 0.0-1.0
        if (i < pop.size()) {
            h.pop = static_cast<float>(pop[i]) / 100.0f;
        }
        
        // Precipitation
        float rainAmount = (i < rain.size()) ? rain[i] : 0.0f;
        float showerAmount = (i < showers.size()) ? showers[i] : 0.0f;
        float snowAmount = (i < snowfall.size()) ? snowfall[i] : 0.0f;
        h.rain_1h = rainAmount + showerAmount;
        h.snow_1h = snowAmount;
        
        // Weather code: WMO → OWM
        if (i < weatherCode.size()) {
            wmoToOwmWeather(weatherCode[i], true, h.weather);
        }
        
        // Estimate other fields
        h.dew_point = h.temp - ((100.0f - h.humidity) / 5.0f);  // Rough estimate
        h.uvi = 0.0f;  // Not directly available in basic Open-Meteo
        h.clouds = 0;  // Not directly available
        h.wind_deg = 0;  // Not available in basic endpoint
    }
}

/**
 * Parse daily data and convert to owm_daily_t array
 */
static void parseAndConvertDaily(
    const std::string& jsonSection,
    owm_daily_t* dailyArray,
    int maxDays) {
    
    std::vector<std::string> times = parseStringArray(extractJsonValue(jsonSection, "time"));
    std::vector<int> weatherCode = parseIntArray(extractJsonValue(jsonSection, "weather_code"));
    std::vector<float> tempMax = parseFloatArray(extractJsonValue(jsonSection, "temperature_2m_max"));
    std::vector<float> tempMin = parseFloatArray(extractJsonValue(jsonSection, "temperature_2m_min"));
    std::vector<int> popMax = parseIntArray(extractJsonValue(jsonSection, "precipitation_probability_max"));
    
    int numDays = std::min(static_cast<int>(times.size()), maxDays);
    
    for (int i = 0; i < numDays; i++) {
        owm_daily_t& d = dailyArray[i];
        
        // Timestamp (date only, set to noon)
        d.dt = parseIso8601ToTimestamp(times[i] + "T12:00");
        
        // Temperature: Celsius → Kelvin
        if (i < tempMax.size()) {
            d.temp.max = celsius_to_kelvin(tempMax[i]);
        }
        if (i < tempMin.size()) {
            d.temp.min = celsius_to_kelvin(tempMin[i]);
        }
        // Estimate other temps
        d.temp.day = d.temp.max;
        d.temp.night = d.temp.min;
        d.temp.morn = (d.temp.max + d.temp.min) / 2.0f;
        d.temp.eve = d.temp.morn;
        
        // Feels like (estimate same as temp)
        d.feels_like.day = d.temp.day;
        d.feels_like.night = d.temp.night;
        d.feels_like.morn = d.temp.morn;
        d.feels_like.eve = d.temp.eve;
        
        // POP: % → 0.0-1.0
        if (i < popMax.size()) {
            d.pop = static_cast<float>(popMax[i]) / 100.0f;
        }
        
        // Weather code: WMO → OWM
        if (i < weatherCode.size()) {
            wmoToOwmWeather(weatherCode[i], true, d.weather);
        }
        
        // Estimate other fields
        d.pressure = 1013;
        d.humidity = 60;
        d.dew_point = d.temp.min - 5.0f;
        d.clouds = 50;
        d.uvi = 5.0f;
        d.visibility = 10000;
        d.wind_speed = 5.0f;
        d.wind_gust = 8.0f;
        d.wind_deg = 180;
        d.rain = 0.0f;
        d.snow = 0.0f;
        d.sunrise = d.dt - 21600;  // 6 hours before noon
        d.sunset = d.dt + 21600;   // 6 hours after noon
        d.moonrise = d.dt + 43200; // Midnight
        d.moonset = d.dt;
        d.moon_phase = 0.25f;
    }
}

// ============================================================================
// Main Parse Function
// ============================================================================

owm_resp_onecall_t parseOpenMeteo(const std::string& jsonString) {
    owm_resp_onecall_t response = {};
    
    // Parse top-level fields
    std::string latStr = extractJsonValue(jsonString, "latitude");
    std::string lonStr = extractJsonValue(jsonString, "longitude");
    
    if (!latStr.empty()) {
        response.lat = std::stof(latStr);
    }
    if (!lonStr.empty()) {
        response.lon = std::stof(lonStr);
    }
    
    response.timezone = extractJsonValue(jsonString, "timezone");
    response.timezone_offset = 0;  // Would need parsing if available
    
    // Parse hourly data
    std::string hourlySection = extractObject(jsonString, "hourly");
    if (!hourlySection.empty()) {
        parseAndConvertHourly(hourlySection, response.hourly, OWM_NUM_HOURLY);
    }
    
    // Parse daily data
    std::string dailySection = extractObject(jsonString, "daily");
    if (!dailySection.empty()) {
        parseAndConvertDaily(dailySection, response.daily, OWM_NUM_DAILY);
    }
    
    // Set current weather from first hourly entry
    response.current.dt = response.hourly[0].dt;
    response.current.temp = response.hourly[0].temp;
    response.current.feels_like = response.hourly[0].feels_like;
    response.current.pressure = response.hourly[0].pressure;
    response.current.humidity = response.hourly[0].humidity;
    response.current.dew_point = response.hourly[0].dew_point;
    response.current.clouds = response.hourly[0].clouds;
    response.current.uvi = response.hourly[0].uvi;
    response.current.visibility = response.hourly[0].visibility;
    response.current.wind_speed = response.hourly[0].wind_speed;
    response.current.wind_gust = response.hourly[0].wind_gust;
    response.current.wind_deg = response.hourly[0].wind_deg;
    response.current.rain_1h = response.hourly[0].rain_1h;
    response.current.snow_1h = response.hourly[0].snow_1h;
    response.current.weather = response.hourly[0].weather;
    
    // Sunrise/sunset from first daily entry
    if (response.daily[0].dt != 0) {
        response.current.sunrise = response.daily[0].sunrise;
        response.current.sunset = response.daily[0].sunset;
    }
    
    return response;
}

} // namespace blackbox
