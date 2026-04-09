/* API Response Compatibility Layer for Blackbox Testing
 * Copyright (C) 2025
 *
 * Includes firmware's api_response.h with Arduino compatibility mocks.
 * Provides test-specific helpers for owm_resp_onecall_t.
 */

#ifndef API_RESPONSE_COMPAT_H
#define API_RESPONSE_COMPAT_H

// Must include Arduino compat BEFORE firmware headers
#include "arduino_compat.h"

// Now we can include firmware's api_response.h
#include "api_response.h"

// Include conversions for helper functions
#include "conversions.h"

#include <sstream>
#include <iomanip>

namespace blackbox {

// ============================================================================
// Render Event (test-specific, not in firmware)
// ============================================================================

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

// ============================================================================
// Helper Functions for owm_resp_onecall_t
// ============================================================================

/**
 * Get current POP from hourly data (returns 0-100%)
 */
inline int getCurrentPop(const owm_resp_onecall_t& data) {
    if (data.hourly[0].pop >= 0.0f && data.hourly[0].pop <= 1.0f) {
        return static_cast<int>(data.hourly[0].pop * 100.0f);
    }
    return 0;
}

/**
 * Get current precipitation (rain + snow) in mm
 */
inline float getCurrentPrecipitation(const owm_resp_onecall_t& data) {
    return data.hourly[0].rain_1h + data.hourly[0].snow_1h;
}

/**
 * Get current pressure in hPa
 */
inline float getCurrentPressure(const owm_resp_onecall_t& data) {
    return static_cast<float>(data.hourly[0].pressure);
}

/**
 * Get current visibility in km
 */
inline float getCurrentVisibility(const owm_resp_onecall_t& data) {
    return static_cast<float>(data.hourly[0].visibility) / 1000.0f; // m -> km
}

/**
 * Check if umbrella should be shown
 * Logic: POP >= 30% OR current precipitation > 0.1mm
 */
inline bool shouldShowUmbrella(const owm_resp_onecall_t& data) {
    int pop = getCurrentPop(data);
    float precip = getCurrentPrecipitation(data);
    return (pop >= 30) || (precip > 0.1f);
}

/**
 * Get max POP from hourly forecast
 */
inline int getMaxPop(const owm_resp_onecall_t& data) {
    float maxPop = 0.0f;
    for (int i = 0; i < OWM_NUM_HOURLY; i++) {
        if (data.hourly[i].pop > maxPop) {
            maxPop = data.hourly[i].pop;
        }
    }
    return static_cast<int>(maxPop * 100.0f);
}

/**
 * Get max precipitation from hourly forecast
 */
inline float getMaxPrecipitation(const owm_resp_onecall_t& data) {
    float maxPrecip = 0.0f;
    for (int i = 0; i < OWM_NUM_HOURLY; i++) {
        float precip = data.hourly[i].rain_1h + data.hourly[i].snow_1h;
        if (precip > maxPrecip) {
            maxPrecip = precip;
        }
    }
    return maxPrecip;
}

/**
 * Convert WMO weather code to icon name
 */
inline const char* wmoCodeToIconName(int code) {
    // WMO Weather interpretation codes (WW)
    if (code == 0) return "sun";           // Clear sky
    if (code == 1 || code == 2 || code == 3) return "partly_cloudy";
    if (code == 45 || code == 48) return "fog";
    if (code == 51 || code == 53 || code == 55) return "drizzle";
    if (code == 56 || code == 57) return "freezing_drizzle";
    if (code == 61 || code == 63 || code == 65) return "rain";
    if (code == 66 || code == 67) return "freezing_rain";
    if (code == 71 || code == 73 || code == 75) return "snow";
    if (code == 77) return "snow_grains";
    if (code == 80 || code == 81 || code == 82) return "rain_showers";
    if (code == 85 || code == 86) return "snow_showers";
    if (code == 95) return "thunder";
    if (code == 96 || code == 99) return "thunder_hail";
    return "unknown";
}

// wmoToOwmWeather is declared in include/api_response.h (firmware header)

} // namespace blackbox

#endif // API_RESPONSE_COMPAT_H
