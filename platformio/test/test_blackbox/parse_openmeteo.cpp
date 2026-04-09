/* Parse Open-Meteo JSON Implementation
 * Copyright (C) 2025
 *
 * Simple JSON parser for Open-Meteo responses.
 * Does not use external JSON libraries to keep dependencies minimal.
 */

#include "parse_openmeteo.h"
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace blackbox {

// ============================================================================
// Helper Functions
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
// Data Section Parsing
// ============================================================================

OpenMeteoHourly parseHourlyData(const std::string& jsonSection) {
    OpenMeteoHourly hourly;
    
    hourly.time = parseStringArray(extractJsonValue(jsonSection, "time"));
    hourly.temperature_2m = parseFloatArray(extractJsonValue(jsonSection, "temperature_2m"));
    hourly.relative_humidity_2m = parseIntArray(extractJsonValue(jsonSection, "relative_humidity_2m"));
    hourly.precipitation_probability = parseIntArray(extractJsonValue(jsonSection, "precipitation_probability"));
    hourly.precipitation = parseFloatArray(extractJsonValue(jsonSection, "precipitation"));
    hourly.rain = parseFloatArray(extractJsonValue(jsonSection, "rain"));
    hourly.showers = parseFloatArray(extractJsonValue(jsonSection, "showers"));
    hourly.snowfall = parseFloatArray(extractJsonValue(jsonSection, "snowfall"));
    hourly.weather_code = parseIntArray(extractJsonValue(jsonSection, "weather_code"));
    hourly.wind_speed_10m = parseFloatArray(extractJsonValue(jsonSection, "wind_speed_10m"));
    hourly.surface_pressure = parseFloatArray(extractJsonValue(jsonSection, "surface_pressure"));
    
    return hourly;
}

OpenMeteoDaily parseDailyData(const std::string& jsonSection) {
    OpenMeteoDaily daily;
    
    daily.time = parseStringArray(extractJsonValue(jsonSection, "time"));
    daily.weather_code = parseIntArray(extractJsonValue(jsonSection, "weather_code"));
    daily.temperature_2m_max = parseFloatArray(extractJsonValue(jsonSection, "temperature_2m_max"));
    daily.temperature_2m_min = parseFloatArray(extractJsonValue(jsonSection, "temperature_2m_min"));
    daily.precipitation_probability_max = parseIntArray(extractJsonValue(jsonSection, "precipitation_probability_max"));
    
    return daily;
}

// ============================================================================
// Main Parse Function
// ============================================================================

OpenMeteoResponse parseOpenMeteo(const std::string& jsonString) {
    OpenMeteoResponse response;
    
    // Parse top-level fields
    std::string latStr = extractJsonValue(jsonString, "latitude");
    std::string lonStr = extractJsonValue(jsonString, "longitude");
    
    if (!latStr.empty()) {
        response.latitude = std::stof(latStr);
    }
    if (!lonStr.empty()) {
        response.longitude = std::stof(lonStr);
    }
    
    response.timezone = extractJsonValue(jsonString, "timezone");
    
    // Parse hourly data
    std::string hourlySection = extractObject(jsonString, "hourly");
    if (!hourlySection.empty()) {
        response.hourly = parseHourlyData(hourlySection);
    }
    
    // Parse daily data
    std::string dailySection = extractObject(jsonString, "daily");
    if (!dailySection.empty()) {
        response.daily = parseDailyData(dailySection);
    }
    
    return response;
}

} // namespace blackbox
