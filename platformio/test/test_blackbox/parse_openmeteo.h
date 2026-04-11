/* Parse Open-Meteo JSON for Blackbox Testing
 * Copyright (C) 2025
 *
 * Parses Open-Meteo API JSON string into firmware's owm_resp_onecall_t structure.
 */

#ifndef PARSE_OPENMETEO_H
#define PARSE_OPENMETEO_H

#include "api_response_compat.h"
#include <string>
#include <vector>

namespace blackbox {

/**
 * Parse Open-Meteo JSON response string into owm_resp_onecall_t
 * 
 * @param jsonString Raw JSON string from /v1/forecast endpoint
 * @return Parsed owm_resp_onecall_t structure (firmware format)
 * @throws std::runtime_error if JSON parsing fails
 */
owm_resp_onecall_t parseOpenMeteo(const std::string& jsonString);

/**
 * Parse float array from JSON array string
 */
std::vector<float> parseFloatArray(const std::string& jsonArray);

/**
 * Parse int array from JSON array string
 */
std::vector<int> parseIntArray(const std::string& jsonArray);

/**
 * Parse string array from JSON array string
 */
std::vector<std::string> parseStringArray(const std::string& jsonArray);

} // namespace blackbox

#endif // PARSE_OPENMETEO_H
