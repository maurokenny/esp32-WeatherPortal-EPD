/// @file api_response.h
/// @brief Weather API response data structures and deserialization functions
/// @copyright Copyright (C) 2022-2023 Luke Marzen, 2026 Mauro Freitas
/// @license GNU General Public License v3.0
///
/// @details
/// Defines data structures compatible with both OpenWeatherMap OneCall API
/// and Open-Meteo API. All API responses are parsed into these structures
/// for uniform handling throughout the application.
///
/// Memory layout is optimized for ESP32 with limited RAM:
/// - Fixed-size arrays for hourly/daily data (no heap fragmentation)
/// - std::vector only for alerts (variable count)
/// - All timestamps as int64_t Unix epoch

#ifndef __API_RESPONSE_H__
#define __API_RESPONSE_H__

#include <cstdint>
#include <vector>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

/// @brief Number of minutely forecast data points (currently unused)
#define OWM_NUM_MINUTELY       1

/// @brief Number of hourly forecast data points (48 hours = 2 days)
#define OWM_NUM_HOURLY        48

/// @brief Number of daily forecast data points (8 days)
#define OWM_NUM_DAILY          8

/// @brief Maximum number of weather alerts
/// @note OpenWeatherMap does not specify a limit; 8 is reasonable for most regions
#define OWM_NUM_ALERTS         8

/// @brief Number of air quality data points (24 hours)
#define OWM_NUM_AIR_POLLUTION 24

/// ═══════════════════════════════════════════════════════════════════════════
/// OpenWeatherMap API Data Structures
/// ═══════════════════════════════════════════════════════════════════════════

/// @brief Weather condition information
/// @details Group, description, and icon ID for display
typedef struct owm_weather
{
  int     id;               ///< Weather condition ID (OWM code)
  String  main;             ///< Weather group (Rain, Snow, etc.)
  String  description;      ///< Detailed description
  String  icon;             ///< Icon ID (e.g., "01d", "10n")
} owm_weather_t;

/// @brief Temperature data for daily forecast
/// @note Units: Kelvin (default), Celsius (metric), Fahrenheit (imperial)
typedef struct owm_temp
{
  float   morn;             ///< Morning temperature
  float   day;              ///< Day temperature
  float   eve;              ///< Evening temperature
  float   night;            ///< Night temperature
  float   min;              ///< Minimum daily temperature
  float   max;              ///< Maximum daily temperature
} owm_temp_t;

/// @brief "Feels like" temperature for daily forecast
/// @details Accounts for human perception of weather
typedef struct owm_feels_like
{
  float   morn;             ///< Morning feels-like
  float   day;              ///< Day feels-like
  float   eve;              ///< Evening feels-like
  float   night;            ///< Night feels-like
} owm_owm_feels_like_t;

/// @brief Current weather conditions
/// @details Contains all current observation data from API
typedef struct owm_current
{
  int64_t dt;               ///< Current time, Unix epoch UTC
  int64_t sunrise;          ///< Sunrise time, Unix epoch UTC
  int64_t sunset;           ///< Sunset time, Unix epoch UTC
  float   temp;             ///< Temperature (Kelvin)
  float   feels_like;       ///< "Feels like" temperature
  int     pressure;         ///< Atmospheric pressure at sea level, hPa
  int     humidity;         ///< Humidity percentage (0-100)
  float   dew_point;        ///< Dew point temperature (Kelvin)
  int     clouds;           ///< Cloudiness percentage (0-100)
  float   uvi;              ///< UV index
  int     visibility;       ///< Average visibility in meters (max 10km)
  float   wind_speed;       ///< Wind speed (m/s)
  float   wind_gust;        ///< Wind gust speed (m/s)
  int     wind_deg;         ///< Wind direction in degrees (meteorological)
  float   rain_1h;          ///< Rain volume last hour (mm)
  float   snow_1h;          ///< Snow volume last hour (mm)
  owm_weather_t  weather;   ///< Weather condition details
} owm_current_t;

/// @brief Minutely precipitation forecast
/// @note Currently unused in application
typedef struct owm_minutely
{
  int64_t dt;               ///< Forecast time, Unix epoch UTC
  float   precipitation;    ///< Precipitation volume (mm)
} owm_minutely_t;

/// @brief Hourly forecast data point
typedef struct owm_hourly
{
  int64_t dt;               ///< Forecast time, Unix epoch UTC
  float   temp;             ///< Temperature (Kelvin)
  float   feels_like;       ///< "Feels like" temperature
  int     pressure;         ///< Pressure (hPa)
  int     humidity;         ///< Humidity percentage (0-100)
  float   dew_point;        ///< Dew point (Kelvin)
  int     clouds;           ///< Cloudiness percentage (0-100)
  float   uvi;              ///< UV index
  int     visibility;       ///< Visibility in meters
  float   wind_speed;       ///< Wind speed (m/s)
  float   wind_gust;        ///< Wind gust (m/s)
  int     wind_deg;         ///< Wind direction (degrees)
  float   pop;              ///< Probability of precipitation (0.0 - 1.0)
  float   rain_1h;          ///< Rain volume (mm)
  float   snow_1h;          ///< Snow volume (mm)
  owm_weather_t  weather;   ///< Weather condition
} owm_hourly_t;

/// @brief Daily forecast data point
typedef struct owm_daily
{
  int64_t dt;               ///< Forecast date, Unix epoch UTC
  int64_t sunrise;          ///< Sunrise time, Unix epoch UTC
  int64_t sunset;           ///< Sunset time, Unix epoch UTC
  int64_t moonrise;         ///< Moonrise time, Unix epoch UTC
  int64_t moonset;          ///< Moonset time, Unix epoch UTC
  float   moon_phase;       ///< Moon phase (0-1): 0=new, 0.25=first quarter, 0.5=full, 0.75=last quarter
  owm_temp_t            temp;        ///< Temperature ranges
  owm_owm_feels_like_t  feels_like;  ///< Feels-like temperatures
  int     pressure;         ///< Pressure (hPa)
  int     humidity;         ///< Humidity percentage (0-100)
  float   dew_point;        ///< Dew point (Kelvin)
  int     clouds;           ///< Cloudiness (0-100)
  float   uvi;              ///< UV index
  int     visibility;       ///< Visibility (meters)
  float   wind_speed;       ///< Wind speed (m/s)
  float   wind_gust;        ///< Wind gust (m/s)
  int     wind_deg;         ///< Wind direction (degrees)
  float   pop;              ///< Probability of precipitation (0.0 - 1.0)
  float   rain;             ///< Rain volume (mm)
  float   snow;             ///< Snow volume (mm)
  owm_weather_t  weather;   ///< Weather condition
} owm_daily_t;

/// @brief Weather alert from national warning systems
typedef struct owm_alerts
{
  String  sender_name;      ///< Alert source name
  String  event;            ///< Alert event name
  int64_t start;            ///< Alert start time, Unix epoch UTC
  int64_t end;              ///< Alert end time, Unix epoch UTC
  String  description;      ///< Alert description
  String  tags;             ///< Alert type/category tags
} owm_alerts_t;

/// @brief Complete OneCall API response structure
/// @details Contains all weather data from a single API call
typedef struct owm_resp_onecall
{
  float   lat;                    ///< Geographical latitude
  float   lon;                    ///< Geographical longitude
  String  timezone;               ///< Timezone name
  int     timezone_offset;        ///< Offset from UTC in seconds
  owm_current_t   current;        ///< Current conditions
  owm_hourly_t    hourly[OWM_NUM_HOURLY];  ///< Hourly forecasts
  owm_daily_t     daily[OWM_NUM_DAILY];    ///< Daily forecasts
  std::vector<owm_alerts_t> alerts;        ///< Weather alerts
} owm_resp_onecall_t;

/// @brief Geographical coordinates
typedef struct owm_coord
{
  float   lat;
  float   lon;
} owm_coord_t;

/// @brief Air pollutant concentrations
/// @note All values in μg/m³
typedef struct owm_components
{
  float   co[OWM_NUM_AIR_POLLUTION];     ///< Carbon monoxide
  float   no[OWM_NUM_AIR_POLLUTION];     ///< Nitrogen monoxide
  float   no2[OWM_NUM_AIR_POLLUTION];    ///< Nitrogen dioxide
  float   o3[OWM_NUM_AIR_POLLUTION];     ///< Ozone
  float   so2[OWM_NUM_AIR_POLLUTION];    ///< Sulphur dioxide
  float   pm2_5[OWM_NUM_AIR_POLLUTION];  ///< Fine particulate matter (PM2.5)
  float   pm10[OWM_NUM_AIR_POLLUTION];   ///< Coarse particulate matter (PM10)
  float   nh3[OWM_NUM_AIR_POLLUTION];    ///< Ammonia
} owm_components_t;

/// @brief Air quality API response
/// @details Hourly pollutant concentrations for AQI calculation
typedef struct owm_resp_air_pollution
{
  owm_coord_t      coord;                           ///< Location coordinates
  int              main_aqi[OWM_NUM_AIR_POLLUTION]; ///< AQI values (1-5)
  owm_components_t components;                      ///< Pollutant concentrations
  int64_t          dt[OWM_NUM_AIR_POLLUTION];       ///< Timestamps
} owm_resp_air_pollution_t;

/// ═══════════════════════════════════════════════════════════════════════════
/// OpenWeatherMap API Deserialization (legacy support)
/// ═══════════════════════════════════════════════════════════════════════════

/// @brief Deserialize OpenWeatherMap OneCall API response
/// @param json WiFi client stream containing JSON
/// @param r Output structure to populate
/// @return ArduinoJson deserialization error code
DeserializationError deserializeOneCall(WiFiClient &json, owm_resp_onecall_t &r);

/// @brief Deserialize OpenWeatherMap Air Pollution API response
/// @param json WiFi client stream containing JSON
/// @param r Output structure to populate
/// @return ArduinoJson deserialization error code
DeserializationError deserializeAirQuality(WiFiClient &json, owm_resp_air_pollution_t &r);

/// ═══════════════════════════════════════════════════════════════════════════
/// Open-Meteo API Support
/// ═══════════════════════════════════════════════════════════════════════════

/// @brief Convert WMO Weather Interpretation code to OpenWeatherMap-compatible
/// @details Maps WMO codes (0-99) to OWM-style weather descriptions and icons.
/// Allows reusing existing icon mappings designed for OWM codes.
/// @param wmoCode WMO code from Open-Meteo (0-99)
/// @param isDay true for daytime, false for nighttime (affects icon selection)
/// @param weather Output structure populated with OWM-compatible values
/// @see https://open-meteo.com/en/docs for WMO codes
/// @see https://openweathermap.org/weather-conditions for OWM codes
void wmoToOwmWeather(int wmoCode, bool isDay, owm_weather_t &weather);

/// @brief Parse ISO 8601 datetime string to Unix timestamp
/// @details Format: "2026-03-24T18:15". Handles both UTC and local time inputs.
/// @param iso8601 ISO 8601 datetime string
/// @param inputIsUtc true if input is UTC (timezone=GMT), false if local time
/// @param tzOffsetSeconds Local timezone offset in seconds (used when inputIsUtc=false)
/// @return Unix timestamp (seconds since epoch), or 0 on parse error
int64_t parseIso8601(const char* iso8601, bool inputIsUtc = true, int tzOffsetSeconds = 0);

/// @brief Deserialize Open-Meteo API response
/// @details Converts Open-Meteo format to owm_resp_onecall_t for compatibility.
/// Temperatures are converted from Celsius (Open-Meteo) to Kelvin (OWM format).
/// @param json WiFi client stream containing JSON
/// @param r Output structure to populate
/// @return ArduinoJson deserialization error code
DeserializationError deserializeOpenMeteo(WiFiClient &json, owm_resp_onecall_t &r);

/// @brief Deserialize Open-Meteo Air Quality API response
/// @param json WiFi client stream containing JSON
/// @param r Air quality structure to populate
/// @return ArduinoJson deserialization error code
DeserializationError deserializeOpenMeteoAirQuality(WiFiClient &json, owm_resp_air_pollution_t &r);

/// @brief Load weather data from compiled-in header file
/// @details Only available when USE_SAVED_API_DATA is enabled. Allows offline
/// testing with captured API responses without network requests.
/// @param r Output structure to populate
/// @return Deserialization error code
DeserializationError loadOpenMeteoFromHeader(owm_resp_onecall_t &r);

/// @brief Load air quality data from compiled-in header file
/// @param r Air quality structure to populate
/// @return Deserialization error code
DeserializationError loadOpenMeteoAirQualityFromHeader(owm_resp_air_pollution_t &r);

#endif // __API_RESPONSE_H__
