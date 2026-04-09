/* Parse Open-Meteo JSON for Blackbox Testing
 * Copyright (C) 2025
 *
 * Parses Open-Meteo API JSON string into structured data.
 * Simulates ArduinoJson parsing on native platform.
 */

#ifndef PARSE_OPENMETEO_H
#define PARSE_OPENMETEO_H

#include "api_response.h"
#include <string>

namespace blackbox {

/**
 * Parse Open-Meteo JSON response string into structured data
 * 
 * @param jsonString Raw JSON string from /v1/forecast endpoint
 * @return Parsed OpenMeteoResponse structure
 * @throws std::runtime_error if JSON parsing fails
 */
OpenMeteoResponse parseOpenMeteo(const std::string& jsonString);

/**
 * Parse hourly data section from JSON
 */
OpenMeteoHourly parseHourlyData(const std::string& jsonSection);

/**
 * Parse daily data section from JSON
 */
OpenMeteoDaily parseDailyData(const std::string& jsonSection);

/**
 * Extract float array from JSON array string
 */
std::vector<float> parseFloatArray(const std::string& jsonArray);

/**
 * Extract int array from JSON array string
 */
std::vector<int> parseIntArray(const std::string& jsonArray);

/**
 * Extract string array from JSON array string
 */
std::vector<std::string> parseStringArray(const std::string& jsonArray);

} // namespace blackbox

#endif // PARSE_OPENMETEO_H
