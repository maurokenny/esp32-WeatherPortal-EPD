/* API Response Implementation for Blackbox Testing
 * Copyright (C) 2025
 */

#include "api_response.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace blackbox {

// ============================================================================
// OpenMeteoHourly Implementation
// ============================================================================

int OpenMeteoHourly::getMaxPop() const {
    if (precipitation_probability.empty()) return 0;
    return *std::max_element(
        precipitation_probability.begin(), 
        precipitation_probability.end()
    );
}

float OpenMeteoHourly::getMaxPrecipitation() const {
    if (precipitation.empty()) return 0.0f;
    return *std::max_element(precipitation.begin(), precipitation.end());
}

float OpenMeteoHourly::getMaxRain() const {
    if (rain.empty()) return 0.0f;
    return *std::max_element(rain.begin(), rain.end());
}

float OpenMeteoHourly::getMaxShowers() const {
    if (showers.empty()) return 0.0f;
    return *std::max_element(showers.begin(), showers.end());
}

int OpenMeteoHourly::getDominantWeatherCode() const {
    if (weather_code.empty()) return 0;
    // Return first hour's code as current weather
    return weather_code[0];
}

float OpenMeteoHourly::getCurrentPressure() const {
    if (surface_pressure.empty()) return 0.0f;
    // Return first hour's pressure as current
    return surface_pressure[0];
}

float OpenMeteoHourly::getCurrentVisibility() const {
    if (visibility.empty()) return 0.0f;
    // Return first hour's visibility as current
    return visibility[0];
}

// ============================================================================
// OpenMeteoResponse Implementation
// ============================================================================

bool OpenMeteoResponse::shouldShowUmbrella() const {
    // Umbrella decision logic
    // Show umbrella if: POP >= 30% OR any precipitation > 0
    const int POP_THRESHOLD = 30;
    
    int maxPop = hourly.getMaxPop();
    float maxPrecip = hourly.getMaxPrecipitation();
    float maxRain = hourly.getMaxRain();
    float maxShowers = hourly.getMaxShowers();
    
    return (maxPop >= POP_THRESHOLD) || 
           (maxPrecip > 0.0f) || 
           (maxRain > 0.0f) || 
           (maxShowers > 0.0f);
}

int OpenMeteoResponse::getCurrentPop() const {
    return hourly.getMaxPop();
}

float OpenMeteoResponse::getCurrentPrecipitation() const {
    return hourly.getMaxPrecipitation();
}

float OpenMeteoResponse::getCurrentPressure() const {
    return hourly.getCurrentPressure();
}

float OpenMeteoResponse::getCurrentVisibility() const {
    return hourly.getCurrentVisibility();
}

// ============================================================================
// RenderEvent Implementation
// ============================================================================

std::string RenderEvent::toJson() const {
    std::ostringstream json;
    json << "{";
    json << "\"event_type\":\"" << event_type << "\",";
    json << "\"icon_name\":\"" << icon_name << "\",";
    json << "\"x\":" << x << ",";
    json << "\"y\":" << y << ",";
    json << "\"weather_code\":" << weather_code << ",";
    json << "\"pop\":" << pop << ",";
    json << "\"precipitation\":" << std::fixed << std::setprecision(1) << precipitation;
    if (!timestamp.empty()) {
        json << ",\"timestamp\":\"" << timestamp << "\"";
    }
    json << "}";
    return json.str();
}

} // namespace blackbox
