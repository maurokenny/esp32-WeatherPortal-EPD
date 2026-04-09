/* API Response Data Structures for Blackbox Testing
 * Copyright (C) 2025
 *
 * Mirrors the firmware's api_response.h structures for native testing.
 * Used by parseOpenMeteo() to store parsed JSON data.
 */

#ifndef BLACKBOX_API_RESPONSE_H
#define BLACKBOX_API_RESPONSE_H

#include <string>
#include <vector>

namespace blackbox {

// ============================================================================
// Open-Meteo API Response Structures
// ============================================================================

/**
 * Hourly weather data from Open-Meteo API
 * Corresponds to the hourly forecast array
 */
struct OpenMeteoHourly {
    std::vector<std::string> time;           // ISO timestamps
    std::vector<float> temperature_2m;       // Temperature at 2m (°C)
    std::vector<int> relative_humidity_2m;   // Relative humidity (%)
    std::vector<int> precipitation_probability; // Precipitation probability (%)
    std::vector<float> precipitation;        // Precipitation amount (mm)
    std::vector<float> rain;                 // Rain amount (mm)
    std::vector<float> showers;              // Showers amount (mm)
    std::vector<float> snowfall;             // Snowfall amount (cm)
    std::vector<int> weather_code;           // WMO weather code
    std::vector<float> wind_speed_10m;       // Wind speed at 10m (km/h)
    std::vector<float> surface_pressure;     // Atmospheric pressure (hPa)
    
    // Get max values for the forecast period
    int getMaxPop() const;
    float getMaxPrecipitation() const;
    float getMaxRain() const;
    float getMaxShowers() const;
    int getDominantWeatherCode() const;
    float getCurrentPressure() const;
};

/**
 * Daily weather data from Open-Meteo API
 */
struct OpenMeteoDaily {
    std::vector<std::string> time;           // Date strings
    std::vector<int> weather_code;           // WMO weather code
    std::vector<float> temperature_2m_max;   // Max temperature (°C)
    std::vector<float> temperature_2m_min;   // Min temperature (°C)
    std::vector<int> precipitation_probability_max; // Max POP (%)
};

/**
 * Complete Open-Meteo API response
 * Mirrors the JSON structure from /v1/forecast endpoint
 */
struct OpenMeteoResponse {
    float latitude = 0.0f;
    float longitude = 0.0f;
    std::string timezone;
    
    OpenMeteoHourly hourly;
    OpenMeteoDaily daily;
    
    // Helper methods for icon decision logic
    bool shouldShowUmbrella() const;
    int getCurrentPop() const;
    float getCurrentPrecipitation() const;
    float getCurrentPressure() const;
};

/**
 * Render event for capturing display operations
 * Sent to mock server via HTTP POST /test/events
 */
struct RenderEvent {
    std::string event_type;      // "icon_drawn", "umbrella_drawn", "text_drawn"
    std::string icon_name;       // Icon identifier
    int x = 0;                   // X coordinate
    int y = 0;                   // Y coordinate
    int weather_code = 0;        // Associated weather code
    int pop = 0;                 // Precipitation probability
    float precipitation = 0.0f;  // Precipitation amount
    std::string timestamp;       // Event timestamp
    
    // Convert to JSON string for HTTP transmission
    std::string toJson() const;
};

} // namespace blackbox

#endif // BLACKBOX_API_RESPONSE_H
