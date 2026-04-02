/* Umbrella Widget Data Parser
 * Copyright (C) 2025  Mauro de Freitas
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
 */

#ifndef UMBRELLA_PARSER_H
#define UMBRELLA_PARSER_H

#include <Arduino.h>
#include "api_response.h"

// Default display value for missing/invalid data
#define UMBRELLA_DEFAULT_DISPLAY_VALUE "--"

/* Umbrella recommendation states - matching existing renderer logic */
enum UmbrellaState {
  UMBRELLA_NO_RAIN,        // maxPop < 0.20f - shows X over umbrella
  UMBRELLA_TAKE,           // maxPop >= 0.70f - take umbrella
  UMBRELLA_TOO_WINDY,      // POP >= 0.20f AND wind >= 18 km/h
  UMBRELLA_COMPACT         // POP >= 0.20f AND wind < 18 km/h (or no wind data)
};

/* Parsed and validated umbrella data
 * All string fields use "--" as fallback for missing/invalid values
 * All numeric fields have explicit validity flags
 */
struct UmbrellaData {
  // Rain data
  float maxPop;                    // Maximum probability of precipitation (0.0 - 1.0)
  bool maxPopValid;                // true if maxPop was successfully parsed
  int firstRainIndex;              // Index of first hour with rain (>= 0.20f POP)
  bool rainFound;                  // true if any rain event found (pop >= 0.20f)
  int64_t rainTimestamp;           // Unix timestamp of first rain event
  bool rainTimestampValid;         // true if rain timestamp is valid
  
  // Wind data (optional)
  float maxWindSpeed;              // Maximum wind speed in m/s
  bool windValid;                  // true if wind data is available and valid
  String windDisplayStr;           // Formatted wind speed string (e.g., "15" or "--")
  String windUnitStr;              // Wind unit string (e.g., "km/h" or "--")
  
  // Time data
  int minutesUntilRain;            // Minutes until rain starts (-1 if no rain or invalid)
  bool minutesUntilRainValid;      // true if minutesUntilRain is valid
  
  // Derived state
  UmbrellaState state;             // Final recommendation state
  int popPercent;                  // POP as percentage (0-100)
  
  // Display strings (pre-formatted with fallbacks)
  String popDisplayStr;            // POP percentage string (e.g., "75" or "--")
  String rainTimeDisplayStr;       // Time of rain string (uses input rainTimeStr or "--")
};

/* UmbrellaParser class
 * 
 * Parses and validates API data for the umbrella widget.
 * Handles missing/null/undefined values with appropriate fallbacks.
 * Makes wind data optional in the decision logic.
 */
class UmbrellaParser {
public:
  /* Parse API data and return validated umbrella data
   * 
   * @param hourly Pointer to hourly forecast array
   * @param hours Number of hours in the array
   * @param current_dt Current timestamp (Unix epoch)
   * @param rainTimeStr Pre-formatted rain time string from TimeCoordinator (may be null/empty)
   * @return UmbrellaData structure with validated data and display fallbacks
   */
  static UmbrellaData parse(const owm_hourly_t *hourly, 
                            int hours,
                            int64_t current_dt,
                            const char* rainTimeStr);
  
  /* Check if wind data is valid and should be used in decision logic
   * 
   * @param data Reference to parsed UmbrellaData
   * @return true if wind data is valid and should be considered
   */
  static bool isWindValid(const UmbrellaData& data);
  
  /* Determine umbrella state based on parsed data
   * Wind is optional - if invalid/missing, decision is based only on rain data
   * 
   * @param data Reference to parsed UmbrellaData (will be modified with state)
   */
  static void determineState(UmbrellaData& data);

private:
  /* Validate a single hourly data point
   * Returns true if the data point has valid timestamp and POP
   */
  static bool isValidHourlyData_(const owm_hourly_t& hourly);
  
  /* Check if wind speed value is valid (not NaN, not negative, not unreasonably high)
   */
  static bool isValidWindSpeed_(float windSpeed);
  
  /* Format wind speed for display with unit
   */
  static void formatWindSpeed_(UmbrellaData& data, float windSpeedMps);
  
  /* Convert wind speed from m/s to display unit
   */
  static float convertWindSpeed_(float windSpeedMps);
};

#endif // UMBRELLA_PARSER_H
