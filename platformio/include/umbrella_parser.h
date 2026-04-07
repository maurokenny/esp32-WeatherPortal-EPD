/// @file umbrella_parser.h
/// @brief Umbrella recommendation widget data parser
/// @copyright Copyright (C) 2025 Mauro de Freitas
/// @license GNU General Public License v3.0
///
/// @details
/// Parses hourly weather forecast data to generate umbrella recommendations.
/// Makes intelligent decisions based on:
/// - Probability of precipitation (PoP)
/// - Wind speed (affects umbrella usability)
/// - Time until rain starts
///
/// Decision logic:
/// - POP < 20%: No rain expected, show X over umbrella
/// - POP >= 20% AND wind < 18 km/h: Take compact umbrella
/// - POP >= 20% AND wind >= 18 km/h: Too windy for umbrella
/// - POP >= 70%: Strong recommendation to take umbrella

#ifndef UMBRELLA_PARSER_H
#define UMBRELLA_PARSER_H

#include <Arduino.h>
#include "api_response.h"

/// @brief Default display value for missing/invalid data
#define UMBRELLA_DEFAULT_DISPLAY_VALUE "--"

/// @brief Umbrella recommendation states
/// @details Determined by POP threshold and wind conditions
enum UmbrellaState {
  UMBRELLA_NO_RAIN,    ///< POP < 20% - no rain expected
  UMBRELLA_TAKE,       ///< POP >= 70% - take umbrella
  UMBRELLA_TOO_WINDY,  ///< POP >= 20% but wind >= 18 km/h
  UMBRELLA_COMPACT     ///< POP >= 20% and wind < 18 km/h
};

/// @brief Parsed and validated umbrella data
/// @details All string fields use "--" as fallback for missing values.
/// All numeric fields have explicit validity flags.
struct UmbrellaData {
  // Rain data
  float maxPop;            ///< Maximum POP in forecast window (0.0 - 1.0)
  bool maxPopValid;        ///< true if maxPop was successfully parsed
  int firstRainIndex;      ///< Index of first hour with rain (POP >= 0.20)
  bool rainFound;          ///< true if any rain event found
  int64_t rainTimestamp;   ///< Unix timestamp of first rain event
  bool rainTimestampValid; ///< true if rain timestamp is valid

  // Wind data (optional)
  float maxWindSpeed;      ///< Maximum wind speed in m/s
  bool windValid;          ///< true if wind data available and valid
  String windDisplayStr;   ///< Formatted wind speed (e.g., "15" or "--")
  String windUnitStr;      ///< Wind unit string (e.g., "km/h" or "--")

  // Time data
  int minutesUntilRain;    ///< Minutes until rain starts (-1 if no rain)
  bool minutesUntilRainValid; ///< true if minutesUntilRain is valid

  // Derived state
  UmbrellaState state;     ///< Final recommendation state
  int popPercent;          ///< POP as percentage (0-100)

  // Display strings (pre-formatted with fallbacks)
  String popDisplayStr;    ///< POP percentage string (e.g., "75" or "--")
  String rainTimeDisplayStr; ///< Rain time string (uses input or "--")
};

/// @brief Umbrella recommendation parser
///
/// Parses and validates API data for the umbrella widget.
/// Handles missing/null/undefined values with appropriate fallbacks.
/// Wind data is optional in the decision logic.
class UmbrellaParser {
public:
  /// @brief Parse API data and return validated umbrella data
  /// @param hourly Pointer to hourly forecast array
  /// @param hours Number of hours in array
  /// @param current_dt Current timestamp (Unix epoch)
  /// @param rainTimeStr Pre-formatted rain time from TimeCoordinator (may be null)
  /// @return UmbrellaData structure with validated data and display fallbacks
  static UmbrellaData parse(const owm_hourly_t *hourly,
                            int hours,
                            int64_t current_dt,
                            const char* rainTimeStr);

  /// @brief Check if wind data should be used in decision logic
  /// @param data Reference to parsed UmbrellaData
  /// @return true if wind data is valid and should be considered
  static bool isWindValid(const UmbrellaData& data);

  /// @brief Determine umbrella state based on parsed data
  /// @param data Reference to UmbrellaData (modified with state)
  /// @details Wind is optional - if invalid/missing, decision based only on rain data
  static void determineState(UmbrellaData& data);

private:
  /// @brief Validate a single hourly data point
  /// @param hourly Hourly data point to validate
  /// @return true if has valid timestamp and POP
  static bool isValidHourlyData_(const owm_hourly_t& hourly);

  /// @brief Validate wind speed value
  /// @param windSpeed Wind speed in m/s
  /// @return true if valid (not NaN, not negative, not unreasonably high)
  static bool isValidWindSpeed_(float windSpeed);

  /// @brief Format wind speed for display with unit
  /// @param data Output structure to populate
  /// @param windSpeedMps Wind speed in m/s
  static void formatWindSpeed_(UmbrellaData& data, float windSpeedMps);

  /// @brief Convert wind speed from m/s to display unit
  /// @param windSpeedMps Wind speed in m/s
  /// @return Converted value in configured display units
  static float convertWindSpeed_(float windSpeedMps);
};

#endif // UMBRELLA_PARSER_H
