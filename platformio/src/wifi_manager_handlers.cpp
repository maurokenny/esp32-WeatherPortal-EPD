/* wifi_manager_handlers.cpp - Secure configuration handler for ESP32 Weather Station
 * Copyright (C) 2025
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "wifi_manager.h"
#include <time.h>
#include <cctype>
#include <ESPAsyncWebServer.h>

// External RTC RAM variables (defined in wifi_manager.cpp)
// Buffer sizes: Character Limit + 1 for null-terminator
extern char ramSSID[33];       // Wi-Fi SSID: 32 chars + \0
extern char ramPassword[64];   // WPA2 password: 63 chars + \0
extern char ramCity[64];       // City: 63 chars + \0
extern char ramCountry[64];    // Country: 63 chars + \0
extern char ramLat[21];        // Latitude: 20 chars + \0
extern char ramLon[21];        // Longitude: 20 chars + \0
extern char ramTimezone[64];   // Timezone: 63 chars + \0
extern bool ramAutoGeo;
extern uint8_t ramTimezoneMode;
extern RuntimeState runtime;

// Maximum input lengths (buffer size - 1 for null terminator)
// These values MUST match the buffer sizes declared in wifi_manager.h
// MAX_SSID_LEN (32) is defined by ESP32 SDK (esp_wifi_types.h) - use that directly
#define MAX_PASSWORD_LEN   63   // WPA2 standard
#define MAX_CITY_LEN       63   // General text field
#define MAX_COUNTRY_LEN    63   // General text field
#define MAX_LAT_LEN        20   // High-precision coordinate (e.g., "-122.1234567890")
#define MAX_LON_LEN        20   // High-precision coordinate
#define MAX_TIMEZONE_LEN   63   // POSIX timezone strings can be long

// Safe string copy with guaranteed null termination using strlcpy
// strlcpy is safer than strncpy as it always null-terminates and returns the
// total length of the source string (allowing truncation detection)
// Uses stack buffer approach - no dynamic allocation
void safeCopy(const char* src, char* dest, size_t destLen) {
    if (!src || !dest || destLen == 0) {
        if (dest && destLen > 0) {
            dest[0] = '\0';
        }
        return;
    }
    // strlcpy guarantees null-termination and prevents buffer overflow
    size_t srcLen = strlcpy(dest, src, destLen);
    // Optional: Log truncation for debugging (can be removed in production)
    #if DEBUG_LEVEL >= 1
    if (srcLen >= destLen) {
        Serial.printf("[WARNING] String truncated: source=%u, dest=%u\n", 
                      (unsigned int)srcLen, (unsigned int)destLen);
    }
    #endif
}

// SSID validation: allow letters, numbers, spaces, underscore, dash, dot
// Remove control characters only
void sanitizeSSID(const char* input, char* output, size_t outLen) {
    if (!input || !output || outLen == 0) {
        if (output && outLen > 0) {
            output[0] = '\0';
        }
        return;
    }

    size_t j = 0;
    for (size_t i = 0; input[i] != '\0' && j < outLen - 1; i++) {
        char c = input[i];
        // Allow: printable ASCII except dangerous control chars
        // SSID can have spaces, dots, dashes, underscores, alphanumeric
        if (c >= 32 && c <= 126) {
            output[j++] = c;
        }
        // Skip control characters and non-printable
    }
    output[j] = '\0';
}

/**
 * Sanitizes City/Country names.
 * Allows: Alphanumerics, spaces, dashes, underscores, dots, and APOSTROPHES (').
 * Blocks: Dangerous characters used in Shell Injection, XSS, or Path Traversal.
 */
void sanitizeCityCountry(const char* input, char* output, size_t outLen) {
    // Basic safety check for pointers and buffer size
    if (!input || !output || outLen == 0) {
        if (output && outLen > 0) {
            output[0] = '\0';
        }
        return;
    }

    size_t j = 0;
    for (size_t i = 0; input[i] != '\0' && j < outLen - 1; i++) {
        unsigned char c = (unsigned char)input[i];

        // 1. BLACKLIST: Explicitly skip dangerous control/scripting characters
        if (c == '"' || c == '\\' || c == '/' || c == ';' ||
            c == '`' || c == '|' || c == '&' || c == '<' || c == '>') {
            continue;
        }

        // 2. WHITELIST: Only copy allowed characters into the output buffer
        if ((c >= 'a' && c <= 'z') || 
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || 
            c == ' '  || 
            c == '-'  ||
            c == '_'  || 
            c == '.'  || 
            c == '\'') {
            
            output[j++] = c;
        }
    }

    // Always null-terminate the output string
    output[j] = '\0';
}

// Validate latitude string: must be numeric, range -90 to 90
bool validateLatitude(const char* latStr) {
    if (!latStr || latStr[0] == '\0') {
        return false;
    }

    // Check length to prevent buffer issues in conversion
    if (strlen(latStr) > MAX_LAT_LEN) {
        return false;
    }

    // Validate characters: digits, minus sign, and one decimal point
    bool hasDigit = false;
    bool hasDecimal = false;
    size_t start = 0;

    // Allow leading minus sign
    if (latStr[0] == '-') {
        start = 1;
    }

    for (size_t i = start; latStr[i] != '\0'; i++) {
        char c = latStr[i];
        if (c >= '0' && c <= '9') {
            hasDigit = true;
        } else if (c == '.' && !hasDecimal) {
            hasDecimal = true;
        } else {
            return false; // Invalid character
        }
    }

    if (!hasDigit) {
        return false; // Must have at least one digit
    }

    // Convert and check range
    char* endPtr = nullptr;
    float lat = strtof(latStr, &endPtr);

    // Check if conversion consumed entire string
    if (endPtr && *endPtr != '\0') {
        return false;
    }

    // Range check: -90 to 90
    if (lat < -90.0f || lat > 90.0f) {
        return false;
    }

    return true;
}

// Validate longitude string: must be numeric, range -180 to 180
bool validateLongitude(const char* lonStr) {
    if (!lonStr || lonStr[0] == '\0') {
        return false;
    }

    // Check length
    if (strlen(lonStr) > MAX_LON_LEN) {
        return false;
    }

    // Validate characters: digits, minus sign, and one decimal point
    bool hasDigit = false;
    bool hasDecimal = false;
    size_t start = 0;

    // Allow leading minus sign
    if (lonStr[0] == '-') {
        start = 1;
    }

    for (size_t i = start; lonStr[i] != '\0'; i++) {
        char c = lonStr[i];
        if (c >= '0' && c <= '9') {
            hasDigit = true;
        } else if (c == '.' && !hasDecimal) {
            hasDecimal = true;
        } else {
            return false; // Invalid character
        }
    }

    if (!hasDigit) {
        return false; // Must have at least one digit
    }

    // Convert and check range
    char* endPtr = nullptr;
    float lon = strtof(lonStr, &endPtr);

    // Check if conversion consumed entire string
    if (endPtr && *endPtr != '\0') {
        return false;
    }

    // Range check: -180 to 180
    if (lon < -180.0f || lon > 180.0f) {
        return false;
    }

    return true;
}

// Validate timezone string: allowed characters only, length check
// Allowed: A-Z, a-z, 0-9, _, /, +, -, ,, ., :
bool validateTimezone(const char* tzStr) {
    if (!tzStr || tzStr[0] == '\0') {
        return false;
    }

    size_t len = strlen(tzStr);
    if (len == 0 || len > MAX_TIMEZONE_LEN) {
        return false;
    }

    for (size_t i = 0; tzStr[i] != '\0'; i++) {
        char c = tzStr[i];
        bool valid = false;

        if (c >= 'A' && c <= 'Z') valid = true;
        else if (c >= 'a' && c <= 'z') valid = true;
        else if (c >= '0' && c <= '9') valid = true;
        else if (c == '_' || c == '/' || c == '+' || c == '-') valid = true;
        else if (c == ',' || c == '.' || c == ':') valid = true;
        else if (c == '<' || c == '>') valid = true;

        if (!valid) {
            return false;
        }
    }

    return true;
}

// Apply timezone and verify it works
// Returns true on success, false on failure (fallback to UTC applied)
bool applyTimezone(const char* tzStr) {
    if (!tzStr || tzStr[0] == '\0') {
        return false;
    }

    // Set the timezone environment variable
    setenv("TZ", tzStr, 1);
    tzset();

    // Verify the timezone works by attempting conversion
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);

    if (tm_info == nullptr) {
        // Invalid timezone - revert to UTC
        setenv("TZ", "UTC0", 1);
        tzset();
        return false;
    }

    return true;
}

// Helper to send error response and prevent restart
void sendErrorResponse(AsyncWebServerRequest* request, const char* message) {
    const char* htmlTemplate =
        "<!DOCTYPE html><html><head><title>Error</title></head><body>"
        "<h2>Configuration Error</h2>"
        "<p>%s</p>"
        "<p><small>Note: When 'Detect location automatically' is off, "
        "City, Country, Latitude, Longitude and Timezone are required.</small></p>"
        "<p><a href='/'>Go back</a></p>"
        "</body></html>";

    char response[512];
    snprintf(response, sizeof(response), htmlTemplate, message);
    request->send(400, "text/html", response);
}

// Helper to send success response and trigger restart
void sendSuccessResponse(AsyncWebServerRequest* request) {
    const char* htmlResponse =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<title>Success</title>"
        "<meta http-equiv=\"refresh\" content=\"5;url=/\">"
        "</head>"
        "<body>"
        "<h2>Configuration Saved Successfully</h2>"
        "<p>The device will restart and connect to your Wi-Fi network.</p>"
        "<p>Redirecting in 5 seconds...</p>"
        "</body></html>";

    request->send(200, "text/html", htmlResponse);

    // Mark portal as inactive to trigger restart sequence
    runtime.portalActive = false;
}

// Main configuration save handler
// Validates all inputs, sanitizes strings, applies timezone
void handleConfigSave(AsyncWebServerRequest* request) {
    // Temporary buffers for validation (on stack, no dynamic allocation)
    // Sizes synchronized with RTC RAM variables in wifi_manager.cpp
    char tempSSID[33] = {0};       // Matches ramSSID[33]
    char tempPassword[64] = {0};   // Matches ramPassword[64]
    char tempCity[64] = {0};       // Matches ramCity[64]
    char tempCountry[64] = {0};    // Matches ramCountry[64]
    char tempLat[21] = {0};        // Matches ramLat[21]
    char tempLon[21] = {0};        // Matches ramLon[21]
    char tempTimezone[64] = {0};   // Matches ramTimezone[64]
    bool tempAutoGeo = false;

    // Track validation errors
    bool hasError = false;
    const char* errorMessage = nullptr;

    // --- Extract SSID ---
    if (request->hasParam("ssid", true)) {
        const char* rawSSID = request->getParam("ssid", true)->value().c_str();
        sanitizeSSID(rawSSID, tempSSID, sizeof(tempSSID));
        if (tempSSID[0] == '\0') {
            hasError = true;
            errorMessage = "SSID cannot be empty.";
        }
    } else {
        hasError = true;
        errorMessage = "SSID parameter is missing.";
    }

    // --- Extract Password ---
    if (!hasError && request->hasParam("password", true)) {
        const char* rawPass = request->getParam("password", true)->value().c_str();
        safeCopy(rawPass, tempPassword, sizeof(tempPassword));
    }

    // --- Extract Auto-Geolocation Flag FIRST (determines which fields are required) ---
    tempAutoGeo = request->hasParam("auto_geo", true);

    // --- Extract Location Fields (only required when auto_geo is OFF) ---
    if (!tempAutoGeo) {
        // City is required in manual mode
        if (!hasError && request->hasParam("city", true)) {
            const char* rawCity = request->getParam("city", true)->value().c_str();
            sanitizeCityCountry(rawCity, tempCity, sizeof(tempCity));
            if (tempCity[0] == '\0') {
                hasError = true;
                errorMessage = "City is required when not using auto-detection.";
            }
        } else if (!hasError) {
            hasError = true;
            errorMessage = "City is required when not using auto-detection.";
        }

        // Country is required in manual mode
        if (!hasError && request->hasParam("country", true)) {
            const char* rawCountry = request->getParam("country", true)->value().c_str();
            sanitizeCityCountry(rawCountry, tempCountry, sizeof(tempCountry));
            if (tempCountry[0] == '\0') {
                hasError = true;
                errorMessage = "Country is required when not using auto-detection.";
            }
        } else if (!hasError) {
            hasError = true;
            errorMessage = "Country is required when not using auto-detection.";
        }

        // Latitude is required in manual mode
        if (!hasError && request->hasParam("lat", true)) {
            const char* rawLat = request->getParam("lat", true)->value().c_str();
            if (validateLatitude(rawLat)) {
                safeCopy(rawLat, tempLat, sizeof(tempLat));
            } else {
                hasError = true;
                errorMessage = "Invalid latitude. Must be between -90 and 90.";
            }
        } else if (!hasError) {
            hasError = true;
            errorMessage = "Latitude is required when not using auto-detection.";
        }

        // Longitude is required in manual mode
        if (!hasError && request->hasParam("lon", true)) {
            const char* rawLon = request->getParam("lon", true)->value().c_str();
            if (validateLongitude(rawLon)) {
                safeCopy(rawLon, tempLon, sizeof(tempLon));
            } else {
                hasError = true;
                errorMessage = "Invalid longitude. Must be between -180 and 180.";
            }
        } else if (!hasError) {
            hasError = true;
            errorMessage = "Longitude is required when not using auto-detection.";
        }

        // Timezone is required in manual mode
        if (!hasError && request->hasParam("timezone", true)) {
            const char* rawTz = request->getParam("timezone", true)->value().c_str();
            if (validateTimezone(rawTz) && strlen(rawTz) > 0) {
                safeCopy(rawTz, tempTimezone, sizeof(tempTimezone));
            } else {
                hasError = true;
                errorMessage = "Timezone is required when not using auto-detection.";
            }
        } else if (!hasError) {
            hasError = true;
            errorMessage = "Timezone is required when not using auto-detection.";
        }
    }
    // Note: When auto_geo is ON, location fields are optional (will be detected via IP)

    // --- Determine Timezone Mode ---
    // If auto_geo is checked: use API timezone (AUTO)
    // If auto_geo is unchecked: use manual timezone from dropdown (MANUAL)
    uint8_t tempTimezoneMode = tempAutoGeo ? TIMEZONE_MODE_AUTO : TIMEZONE_MODE_MANUAL;

    // If there were validation errors, send error and don't save anything
    if (hasError) {
        sendErrorResponse(request, errorMessage);
        return;
    }

    // --- Apply Timezone and Verify (only for manual mode) ---
    bool tzApplied = true;
    if (!tempAutoGeo) {
        // In manual mode, validate the user-selected timezone
        tzApplied = applyTimezone(tempTimezone);
        if (!tzApplied) {
            // Timezone was invalid at runtime - use UTC fallback
            safeCopy("UTC0", tempTimezone, sizeof(tempTimezone));
        }
    } else {
        // In auto mode, timezone will be determined by API
        // Use UTC as placeholder until first API call
        safeCopy("UTC0", tempTimezone, sizeof(tempTimezone));
    }

    // --- All validations passed - copy to RTC RAM ---
    safeCopy(tempSSID, ramSSID, sizeof(ramSSID));
    safeCopy(tempPassword, ramPassword, sizeof(ramPassword));
    safeCopy(tempCity, ramCity, sizeof(ramCity));
    safeCopy(tempCountry, ramCountry, sizeof(ramCountry));
    safeCopy(tempLat, ramLat, sizeof(ramLat));
    safeCopy(tempLon, ramLon, sizeof(ramLon));
    safeCopy(tempTimezone, ramTimezone, sizeof(ramTimezone));
    ramAutoGeo = tempAutoGeo;
    ramTimezoneMode = tempTimezoneMode;

    // Log configuration (password masked for security)
    Serial.println("Configuration saved:");
    Serial.printf("  SSID: %s\n", ramSSID);
    Serial.printf("  Password: %s\n", (ramPassword[0] != '\0') ? "[SET]" : "[EMPTY]");
    Serial.printf("  City: %s\n", ramCity);
    Serial.printf("  Country: %s\n", ramCountry);
    Serial.printf("  Lat: %s\n", ramLat);
    Serial.printf("  Lon: %s\n", ramLon);
    Serial.printf("  Timezone: %s%s\n", ramTimezone, tzApplied ? "" : " (fallback from invalid)");
    Serial.printf("  AutoGeo: %s\n", ramAutoGeo ? "true" : "false");
    Serial.printf("  TimezoneMode: %s\n", (ramTimezoneMode == TIMEZONE_MODE_AUTO) ? "AUTO" : "MANUAL");

    // Send success response
    // Only show error if timezone failed in MANUAL mode
    // In AUTO mode, timezone will be determined by API, so always show success
    if (!tzApplied && !tempAutoGeo) {
        // Special case: saved but timezone was invalid in manual mode
        sendErrorResponse(request, "Settings saved but timezone was invalid. Using UTC fallback.");
        // Still trigger restart after a delay
        runtime.portalActive = false;
    } else {
        sendSuccessResponse(request);
    }
}
