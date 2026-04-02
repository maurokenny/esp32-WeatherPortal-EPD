/* Umbrella Widget Data Parser Implementation
 * Copyright (C) 2025 Mauro de Freitas
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

#include "umbrella_parser.h"
#include "conversions.h"
#include "config.h"
#include "_locale.h"
#include <cmath>

// Thresholds matching the existing renderer.cpp logic
static const float POP_THRESHOLD_NO_RAIN = 0.20f;    // Below this: no rain (X icon)
static const float POP_THRESHOLD_TAKE = 0.70f;       // Above this: take umbrella
static const float POP_THRESHOLD_MIN_RAIN = 0.20f;   // Minimum POP to consider as rain event
static const float WIND_THRESHOLD_KMH = 18.0f;       // Wind threshold for umbrella decision
static const int MAX_ANALYSIS_HOURS = 24;            // Limit to 24 hours (1 day)

UmbrellaData UmbrellaParser::parse(const owm_hourly_t *hourly, 
                                   int hours,
                                   int64_t current_dt,
                                   const char* rainTimeStr)
{
  UmbrellaData data = {};
  
  // Initialize with default/invalid values
  data.maxPop = 0.0f;
  data.maxPopValid = false;
  data.firstRainIndex = -1;
  data.rainFound = false;
  data.rainTimestamp = 0;
  data.rainTimestampValid = false;
  data.maxWindSpeed = 0.0f;
  data.windValid = false;
  data.minutesUntilRain = -1;
  data.minutesUntilRainValid = false;
  data.state = UMBRELLA_NO_RAIN;
  data.popPercent = 0;
  
  // Set display fallbacks
  data.popDisplayStr = UMBRELLA_DEFAULT_DISPLAY_VALUE;
  data.windDisplayStr = UMBRELLA_DEFAULT_DISPLAY_VALUE;
  data.windUnitStr = UMBRELLA_DEFAULT_DISPLAY_VALUE;
  data.rainTimeDisplayStr = UMBRELLA_DEFAULT_DISPLAY_VALUE;
  
  // Validate input
  if (hourly == nullptr || hours <= 0 || current_dt <= 0) {
    return data;
  }
  
  // Set rain time display string (with fallback)
  if (rainTimeStr != nullptr && rainTimeStr[0] != '\0') {
    data.rainTimeDisplayStr = String(rainTimeStr);
  }
  
  // Limit analysis hours
  int analysisHours = (hours < MAX_ANALYSIS_HOURS) ? hours : MAX_ANALYSIS_HOURS;
  
  // Find starting index - skip hours that have already passed
  int startIndex = 0;
  for (int i = 0; i < analysisHours; i++) {
    if (isValidHourlyData_(hourly[i]) && hourly[i].dt >= current_dt) {
      startIndex = i;
      break;
    }
  }
  
  // Scan hourly data for rain and wind
  for (int i = startIndex; i < analysisHours; i++) {
    if (!isValidHourlyData_(hourly[i])) {
      continue;
    }
    
    // Track max POP
    if (hourly[i].pop > data.maxPop) {
      data.maxPop = hourly[i].pop;
      data.maxPopValid = true;
    }
    
    // Track max wind speed
    if (isValidWindSpeed_(hourly[i].wind_speed)) {
      if (hourly[i].wind_speed > data.maxWindSpeed) {
        data.maxWindSpeed = hourly[i].wind_speed;
        data.windValid = true;
      }
    }
    
    // Find first rain event
    if (!data.rainFound && hourly[i].pop >= POP_THRESHOLD_MIN_RAIN) {
      data.firstRainIndex = i;
      data.rainFound = true;
      data.rainTimestamp = hourly[i].dt;
      data.rainTimestampValid = true;
    }
  }
  
  // Calculate POP percentage for display
  if (data.maxPopValid) {
    data.popPercent = static_cast<int>(std::round(data.maxPop * 100));
    data.popDisplayStr = String(data.popPercent);
  }
  
  // Format wind speed for display (even if invalid, shows "--")
  if (data.windValid) {
    formatWindSpeed_(data, data.maxWindSpeed);
  }
  
  // Calculate minutes until rain
  if (data.rainFound && data.rainTimestampValid && data.rainTimestamp > current_dt) {
    data.minutesUntilRain = static_cast<int>((data.rainTimestamp - current_dt) / 60);
    data.minutesUntilRainValid = true;
  }
  
  // Determine final state
  determineState(data);
  
  return data;
}

bool UmbrellaParser::isWindValid(const UmbrellaData& data)
{
  return data.windValid && data.maxWindSpeed >= 0.0f;
}

void UmbrellaParser::determineState(UmbrellaData& data)
{
  // State 1: No rain (POP < 0.20f)
  // Note: POP < 20% is always "No rain", regardless of wind
  if (!data.maxPopValid || data.maxPop < POP_THRESHOLD_NO_RAIN) {
    data.state = UMBRELLA_NO_RAIN;
    return;
  }
  
  // State 2: Take umbrella (POP >= 0.70f)
  if (data.maxPop >= POP_THRESHOLD_TAKE) {
    data.state = UMBRELLA_TAKE;
    return;
  }
  
  // For intermediate POP (0.20f - 0.69f), consider wind if available
  // State 3: Too windy - only if wind data is valid AND exceeds threshold
  if (isWindValid(data)) {
    float windKmh = meterspersecond_to_kilometersperhour(data.maxWindSpeed);
    if (windKmh >= WIND_THRESHOLD_KMH) {
      data.state = UMBRELLA_TOO_WINDY;
      return;
    }
  }
  
  // State 4: Compact umbrella
  // - POP between 20% and 69%
  // - Either no wind data, or wind < 18 km/h
  data.state = UMBRELLA_COMPACT;
}

bool UmbrellaParser::isValidHourlyData_(const owm_hourly_t& hourly)
{
  // Check timestamp is valid (after year 2020)
  if (hourly.dt < 1577836800) { // Jan 1, 2020 00:00:00 UTC
    return false;
  }
  
  // Check POP is within valid range [0.0, 1.0]
  if (std::isnan(hourly.pop) || hourly.pop < 0.0f || hourly.pop > 1.0f) {
    return false;
  }
  
  return true;
}

bool UmbrellaParser::isValidWindSpeed_(float windSpeed)
{
  // Check for NaN
  if (std::isnan(windSpeed)) {
    return false;
  }
  
  // Check for negative (invalid)
  if (windSpeed < 0.0f) {
    return false;
  }
  
  // Check for unreasonably high values (likely data error)
  // 100 m/s = 360 km/h, well above any reasonable weather
  if (windSpeed > 100.0f) {
    return false;
  }
  
  return true;
}

void UmbrellaParser::formatWindSpeed_(UmbrellaData& data, float windSpeedMps)
{
  float displayValue = convertWindSpeed_(windSpeedMps);
  
  // Round to integer for display
  int roundedValue = static_cast<int>(std::round(displayValue));
  data.windDisplayStr = String(roundedValue);
  
  // Set unit string based on configuration
#if defined(UNITS_SPEED_METERSPERSECOND)
  data.windUnitStr = String(" ") + TXT_UNITS_SPEED_METERSPERSECOND;
#elif defined(UNITS_SPEED_FEETPERSECOND)
  data.windUnitStr = String(" ") + TXT_UNITS_SPEED_FEETPERSECOND;
#elif defined(UNITS_SPEED_KILOMETERSPERHOUR)
  data.windUnitStr = String(" ") + TXT_UNITS_SPEED_KILOMETERSPERHOUR;
#elif defined(UNITS_SPEED_MILESPERHOUR)
  data.windUnitStr = String(" ") + TXT_UNITS_SPEED_MILESPERHOUR;
#elif defined(UNITS_SPEED_KNOTS)
  data.windUnitStr = String(" ") + TXT_UNITS_SPEED_KNOTS;
#elif defined(UNITS_SPEED_BEAUFORT)
  data.windDisplayStr = String(meterspersecond_to_beaufort(windSpeedMps));
  data.windUnitStr = String(" ") + TXT_UNITS_SPEED_BEAUFORT;
#else
  data.windUnitStr = " km/h";
#endif
}

float UmbrellaParser::convertWindSpeed_(float windSpeedMps)
{
#if defined(UNITS_SPEED_METERSPERSECOND)
  return windSpeedMps;
#elif defined(UNITS_SPEED_FEETPERSECOND)
  return meterspersecond_to_feetpersecond(windSpeedMps);
#elif defined(UNITS_SPEED_KILOMETERSPERHOUR)
  return meterspersecond_to_kilometersperhour(windSpeedMps);
#elif defined(UNITS_SPEED_MILESPERHOUR)
  return meterspersecond_to_milesperhour(windSpeedMps);
#elif defined(UNITS_SPEED_KNOTS)
  return meterspersecond_to_knots(windSpeedMps);
#elif defined(UNITS_SPEED_BEAUFORT)
  return static_cast<float>(meterspersecond_to_beaufort(windSpeedMps));
#else
  // Default to km/h
  return meterspersecond_to_kilometersperhour(windSpeedMps);
#endif
}
