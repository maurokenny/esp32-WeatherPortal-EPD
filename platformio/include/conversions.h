/// @file conversions.h
/// @brief Unit conversion functions for weather data
/// @copyright Copyright (C) 2023 Luke Marzen
/// @license GNU General Public License v3.0
///
/// @details
/// Provides conversion between metric, imperial, and other unit systems.
/// All conversions are inline-friendly for performance.
/// Conversions are direction-specific to maintain precision.

#ifndef __CONVERSIONS_H__
#define __CONVERSIONS_H__

// ═══════════════════════════════════════════════════════════════════════════
// TEMPERATURE CONVERSIONS
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Convert Kelvin to Celsius
/// @param kelvin Temperature in Kelvin
/// @return Temperature in Celsius
float kelvin_to_celsius(float kelvin);

/// @brief Convert Kelvin to Fahrenheit
/// @param kelvin Temperature in Kelvin
/// @return Temperature in Fahrenheit
float kelvin_to_fahrenheit(float kelvin);

/// @brief Convert Celsius to Kelvin
/// @param celsius Temperature in Celsius
/// @return Temperature in Kelvin
float celsius_to_kelvin(float celsius);

/// @brief Convert Celsius to Fahrenheit
/// @param celsius Temperature in Celsius
/// @return Temperature in Fahrenheit
float celsius_to_fahrenheit(float celsius);

// ═══════════════════════════════════════════════════════════════════════════
// WIND SPEED CONVERSIONS
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Convert meters per second to feet per second
/// @param meterspersecond Speed in m/s
/// @return Speed in ft/s
float meterspersecond_to_feetpersecond(float meterspersecond);

/// @brief Convert meters per second to kilometers per hour
/// @param meterspersecond Speed in m/s
/// @return Speed in km/h
float meterspersecond_to_kilometersperhour(float meterspersecond);

/// @brief Convert kilometers per hour to meters per second
/// @param kilometersperhour Speed in km/h
/// @return Speed in m/s
float kilometersperhour_to_meterspersecond(float kilometersperhour);

/// @brief Convert meters per second to miles per hour
/// @param meterspersecond Speed in m/s
/// @return Speed in mph
float meterspersecond_to_milesperhour(float meterspersecond);

/// @brief Convert meters per second to knots
/// @param meterspersecond Speed in m/s
/// @return Speed in knots
float meterspersecond_to_knots(float meterspersecond);

/// @brief Convert meters per second to Beaufort scale
/// @param meterspersecond Speed in m/s
/// @return Beaufort number (0-12)
int meterspersecond_to_beaufort(float meterspersecond);

// ═══════════════════════════════════════════════════════════════════════════
// PRESSURE CONVERSIONS
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Convert hectopascals to pascals
/// @param hectopascals Pressure in hPa
/// @return Pressure in Pa
float hectopascals_to_pascals(float hectopascals);

/// @brief Convert hectopascals to millimeters of mercury
/// @param hectopascals Pressure in hPa
/// @return Pressure in mmHg
float hectopascals_to_millimetersofmercury(float hectopascals);

/// @brief Convert hectopascals to inches of mercury
/// @param hectopascals Pressure in hPa
/// @return Pressure in inHg
float hectopascals_to_inchesofmercury(float hectopascals);

/// @brief Convert hectopascals to millibars
/// @param hectopascals Pressure in hPa
/// @return Pressure in mbar
float hectopascals_to_millibars(float hectopascals);

/// @brief Convert hectopascals to atmospheres
/// @param hectopascals Pressure in hPa
/// @return Pressure in atm
float hectopascals_to_atmospheres(float hectopascals);

/// @brief Convert hectopascals to grams per square centimeter
/// @param hectopascals Pressure in hPa
/// @return Pressure in g/cm²
float hectopascals_to_gramspersquarecentimeter(float hectopascals);

/// @brief Convert hectopascals to pounds per square inch
/// @param hectopascals Pressure in hPa
/// @return Pressure in PSI
float hectopascals_to_poundspersquareinch(float hectopascals);

// ═══════════════════════════════════════════════════════════════════════════
// DISTANCE CONVERSIONS
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Convert meters to kilometers
/// @param meters Distance in meters
/// @return Distance in kilometers
float meters_to_kilometers(float meters);

/// @brief Convert meters to miles
/// @param meters Distance in meters
/// @return Distance in miles
float meters_to_miles(float meters);

/// @brief Convert meters to feet
/// @param meters Distance in meters
/// @return Distance in feet
float meters_to_feet(float meters);

// ═══════════════════════════════════════════════════════════════════════════
// PRECIPITATION CONVERSIONS
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Convert millimeters to inches
/// @param millimeters Length in mm
/// @return Length in inches
float millimeters_to_inches(float millimeters);

/// @brief Convert millimeters to centimeters
/// @param millimeters Length in mm
/// @return Length in cm
float millimeters_to_centimeters(float millimeters);

/// @brief Convert centimeters to millimeters
/// @param centimeters Length in cm
/// @return Length in mm
float centimeters_to_millimeters(float centimeters);

#endif // __CONVERSIONS_H__
