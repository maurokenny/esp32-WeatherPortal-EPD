/// @file _locale.h
/// @brief Localization strings and translation declarations
/// @copyright Copyright (C) 2022-2024 Luke Marzen
/// @license GNU General Public License v3.0
///
/// @details
/// Declares all localized strings used in the UI. Actual translations are
/// defined in separate .inc files in the locales/ directory.
/// Include the appropriate locale file in your build based on LOCALE macro.
///
/// String categories:
/// - Date/time formatting
/// - Weather conditions
/// - Units and measurements
/// - Error messages
/// - Alert terminology
/// - HTTP status codes

#ifndef ___LOCALE_H__
#define ___LOCALE_H__

#include <vector>
#include <Arduino.h>
#include <aqi.h>

// ═══════════════════════════════════════════════════════════════════════════
// LC_TIME - Date/Time Formatting
// ═══════════════════════════════════════════════════════════════════════════

extern const char *LC_D_T_FMT;      ///< Date and time format
extern const char *LC_D_FMT;        ///< Date format
extern const char *LC_T_FMT;        ///< Time format
extern const char *LC_T_FMT_AMPM;   ///< Time format with AM/PM
extern const char *LC_AM_STR;       ///< AM string
extern const char *LC_PM_STR;       ///< PM string
extern const char *LC_DAY[7];       ///< Full day names
extern const char *LC_ABDAY[7];     ///< Abbreviated day names
extern const char *LC_MON[12];      ///< Full month names
extern const char *LC_ABMON[12];    ///< Abbreviated month names
extern const char *LC_ERA;          ///< Era description
extern const char *LC_ERA_D_FMT;    ///< Era date format
extern const char *LC_ERA_D_T_FMT;  ///< Era date/time format
extern const char *LC_ERA_T_FMT;    ///< Era time format

// ═══════════════════════════════════════════════════════════════════════════
// API LANGUAGE
// ═══════════════════════════════════════════════════════════════════════════

extern const String OWM_LANG;       ///< OpenWeatherMap API language code

// ═══════════════════════════════════════════════════════════════════════════
// CURRENT CONDITIONS LABELS
// ═══════════════════════════════════════════════════════════════════════════

extern const char *TXT_FEELS_LIKE;          ///< "Feels Like"
extern const char *TXT_SUNRISE;             ///< "Sunrise"
extern const char *TXT_SUNSET;              ///< "Sunset"
extern const char *TXT_MOONRISE;            ///< "Moonrise"
extern const char *TXT_MOONSET;             ///< "Moonset"
extern const char *TXT_WIND;                ///< "Wind"
extern const char *TXT_HUMIDITY;            ///< "Humidity"
extern const char *TXT_UV_INDEX;            ///< "UV Index"
extern const char *TXT_PRESSURE;            ///< "Pressure"
extern const char *TXT_AIR_QUALITY;         ///< "Air Quality"
extern const char *TXT_AIR_POLLUTION;       ///< "Air Pollution"
extern const char *TXT_VISIBILITY;          ///< "Visibility"
extern const char *TXT_INDOOR_TEMPERATURE;  ///< "Indoor Temp"
extern const char *TXT_INDOOR_HUMIDITY;     ///< "Indoor Humidity"
extern const char *TXT_DEWPOINT;            ///< "Dew Point"

// ═══════════════════════════════════════════════════════════════════════════
// MOON PHASE NAMES
// ═══════════════════════════════════════════════════════════════════════════

extern const char *TXT_MOONPHASE;           ///< "Moon Phase"
extern const char *TXT_NEW_MOON;            ///< "New Moon"
extern const char *TXT_WAXING_CRESCENT;     ///< "Waxing Crescent"
extern const char *TXT_FIRST_QUARTER;       ///< "First Quarter"
extern const char *TXT_WAXING_GIBBOUS;      ///< "Waxing Gibbous"
extern const char *TXT_FULL_MOON;           ///< "Full Moon"
extern const char *TXT_WANING_GIBBOUS;      ///< "Waning Gibbous"
extern const char *TXT_THIRD_QUARTER;       ///< "Third Quarter"
extern const char *TXT_WANING_CRESCENT;     ///< "Waning Crescent"

// ═══════════════════════════════════════════════════════════════════════════
// UV INDEX DESCRIPTIONS
// ═══════════════════════════════════════════════════════════════════════════

extern const char *TXT_UV_LOW;              ///< "Low"
extern const char *TXT_UV_MODERATE;         ///< "Moderate"
extern const char *TXT_UV_HIGH;             ///< "High"
extern const char *TXT_UV_VERY_HIGH;        ///< "Very High"
extern const char *TXT_UV_EXTREME;          ///< "Extreme"

// ═══════════════════════════════════════════════════════════════════════════
// WIFI SIGNAL STRENGTH
// ═══════════════════════════════════════════════════════════════════════════

extern const char *TXT_WIFI_EXCELLENT;      ///< "Excellent"
extern const char *TXT_WIFI_GOOD;           ///< "Good"
extern const char *TXT_WIFI_FAIR;           ///< "Fair"
extern const char *TXT_WIFI_WEAK;           ///< "Weak"
extern const char *TXT_WIFI_NO_CONNECTION;  ///< "No Connection"

// ═══════════════════════════════════════════════════════════════════════════
// UNIT SYMBOLS - TEMPERATURE
// ═══════════════════════════════════════════════════════════════════════════

extern const char *TXT_UNITS_TEMP_KELVIN;       ///< "K"
extern const char *TXT_UNITS_TEMP_CELSIUS;      ///< "°C"
extern const char *TXT_UNITS_TEMP_FAHRENHEIT;   ///< "°F"

// ═══════════════════════════════════════════════════════════════════════════
// UNIT SYMBOLS - WIND SPEED
// ═══════════════════════════════════════════════════════════════════════════

extern const char *TXT_UNITS_SPEED_METERSPERSECOND;    ///< "m/s"
extern const char *TXT_UNITS_SPEED_FEETPERSECOND;      ///< "ft/s"
extern const char *TXT_UNITS_SPEED_KILOMETERSPERHOUR;  ///< "km/h"
extern const char *TXT_UNITS_SPEED_MILESPERHOUR;       ///< "mph"
extern const char *TXT_UNITS_SPEED_KNOTS;              ///< "kn"
extern const char *TXT_UNITS_SPEED_BEAUFORT;           ///< "Bft"

// ═══════════════════════════════════════════════════════════════════════════
// UNIT SYMBOLS - PRESSURE
// ═══════════════════════════════════════════════════════════════════════════

extern const char *TXT_UNITS_PRES_HECTOPASCALS;             ///< "hPa"
extern const char *TXT_UNITS_PRES_PASCALS;                  ///< "Pa"
extern const char *TXT_UNITS_PRES_MILLIMETERSOFMERCURY;     ///< "mmHg"
extern const char *TXT_UNITS_PRES_INCHESOFMERCURY;          ///< "inHg"
extern const char *TXT_UNITS_PRES_MILLIBARS;                ///< "mbar"
extern const char *TXT_UNITS_PRES_ATMOSPHERES;              ///< "atm"
extern const char *TXT_UNITS_PRES_GRAMSPERSQUARECENTIMETER; ///< "g/cm²"
extern const char *TXT_UNITS_PRES_POUNDSPERSQUAREINCH;      ///< "psi"

// ═══════════════════════════════════════════════════════════════════════════
// UNIT SYMBOLS - DISTANCE
// ═══════════════════════════════════════════════════════════════════════════

extern const char *TXT_UNITS_DIST_KILOMETERS;  ///< "km"
extern const char *TXT_UNITS_DIST_MILES;       ///< "mi"

// ═══════════════════════════════════════════════════════════════════════════
// UNIT SYMBOLS - PRECIPITATION
// ═══════════════════════════════════════════════════════════════════════════

extern const char *TXT_UNITS_PRECIP_MILLIMETERS;  ///< "mm"
extern const char *TXT_UNITS_PRECIP_CENTIMETERS;  ///< "cm"
extern const char *TXT_UNITS_PRECIP_INCHES;       ///< "in"

// ═══════════════════════════════════════════════════════════════════════════
// MISCELLANEOUS MESSAGES
// ═══════════════════════════════════════════════════════════════════════════

// Title Case
extern const char *TXT_LOW_BATTERY;                     ///< "Low Battery"
extern const char *TXT_NETWORK_NOT_AVAILABLE;           ///< "Network Not Available"
extern const char *TXT_TIME_SYNCHRONIZATION_FAILED;     ///< "Time Sync Failed"
extern const char *TXT_WIFI_CONNECTION_FAILED;          ///< "WiFi Connection Failed"

// First Word Capitalized
extern const char *TXT_ATTEMPTING_HTTP_REQ;             ///< "Attempting HTTP Request"
extern const char *TXT_AWAKE_FOR;                       ///< "Awake For"
extern const char *TXT_BATTERY_VOLTAGE;                 ///< "Battery Voltage"
extern const char *TXT_CONNECTING_TO;                   ///< "Connecting To"
extern const char *TXT_COULD_NOT_CONNECT_TO;            ///< "Could Not Connect To"
extern const char *TXT_ENTERING_DEEP_SLEEP_FOR;         ///< "Entering Deep Sleep For"
extern const char *TXT_READING_FROM;                    ///< "Reading From"
extern const char *TXT_FAILED;                          ///< "Failed"
extern const char *TXT_SUCCESS;                         ///< "Success"
extern const char *TXT_UNKNOWN;                         ///< "Unknown"

// All Lowercase
extern const char *TXT_NOT_FOUND;                       ///< "not found"
extern const char *TXT_READ_FAILED;                     ///< "read failed"

// Complete Phrases
extern const char *TXT_FAILED_TO_GET_TIME;              ///< "Failed To Get Time"
extern const char *TXT_HIBERNATING_INDEFINITELY_NOTICE; ///< "Hibernating Indefinitely"
extern const char *TXT_REFERENCING_OLDER_TIME_NOTICE;   ///< "Referencing Older Time"
extern const char *TXT_WAITING_FOR_SNTP;                ///< "Waiting For SNTP"
extern const char *TXT_LOW_BATTERY_VOLTAGE;             ///< "Low Battery Voltage"
extern const char *TXT_VERY_LOW_BATTERY_VOLTAGE;        ///< "Very Low Battery Voltage"
extern const char *TXT_CRIT_LOW_BATTERY_VOLTAGE;        ///< "Critical Low Battery Voltage"

// ═══════════════════════════════════════════════════════════════════════════
// ALERT TERMINOLOGY
// ═══════════════════════════════════════════════════════════════════════════

extern const std::vector<String> ALERT_URGENCY;  ///< Urgency keywords by priority

extern const std::vector<String> TERM_SMOG;      ///< Smog-related terms
extern const std::vector<String> TERM_SMOKE;     ///< Smoke-related terms
extern const std::vector<String> TERM_FOG;       ///< Fog-related terms
extern const std::vector<String> TERM_METEOR;    ///< Meteor-related terms
extern const std::vector<String> TERM_NUCLEAR;   ///< Nuclear/radiation terms
extern const std::vector<String> TERM_BIOHAZARD; ///< Biohazard terms
extern const std::vector<String> TERM_EARTHQUAKE;///< Earthquake terms
extern const std::vector<String> TERM_FIRE;      ///< Fire-related terms
extern const std::vector<String> TERM_HEAT;      ///< Heat-related terms
extern const std::vector<String> TERM_WINTER;    ///< Winter weather terms
extern const std::vector<String> TERM_TSUNAMI;   ///< Tsunami terms
extern const std::vector<String> TERM_LIGHTNING; ///< Lightning terms
extern const std::vector<String> TERM_SANDSTORM; ///< Sandstorm terms
extern const std::vector<String> TERM_FLOOD;     ///< Flood terms
extern const std::vector<String> TERM_VOLCANO;   ///< Volcano terms
extern const std::vector<String> TERM_AIR_QUALITY;///< Air quality terms
extern const std::vector<String> TERM_TORNADO;   ///< Tornado terms
extern const std::vector<String> TERM_SMALL_CRAFT_ADVISORY; ///< Marine terms
extern const std::vector<String> TERM_GALE_WARNING;///< Marine terms
extern const std::vector<String> TERM_STORM_WARNING;///< Marine terms
extern const std::vector<String> TERM_HURRICANE_WARNING;///< Hurricane terms
extern const std::vector<String> TERM_HURRICANE; ///< Hurricane terms
extern const std::vector<String> TERM_DUST;      ///< Dust-related terms
extern const std::vector<String> TERM_STRONG_WIND;///< Wind-related terms

// ═══════════════════════════════════════════════════════════════════════════
// AIR QUALITY INDEX (by region)
// ═══════════════════════════════════════════════════════════════════════════

extern "C" {
extern const aqi_scale_t AQI_SCALE;           ///< Selected AQI scale
extern const char *AUSTRALIA_AQI_TXT[6];      ///< Australia AQI descriptions
extern const char *CANADA_AQHI_TXT[4];        ///< Canada AQHI descriptions
extern const char *EUROPEAN_UNION_CAQI_TXT[5];///< EU CAQI descriptions
extern const char *HONG_KONG_AQHI_TXT[5];     ///< Hong Kong AQHI descriptions
extern const char *INDIA_AQI_TXT[6];          ///< India AQI descriptions
extern const char *CHINA_AQI_TXT[6];          ///< China AQI descriptions
extern const char *SINGAPORE_PSI_TXT[5];      ///< Singapore PSI descriptions
extern const char *SOUTH_KOREA_CAI_TXT[4];    ///< South Korea CAI descriptions
extern const char *UNITED_KINGDOM_DAQI_TXT[4];///< UK DAQI descriptions
extern const char *UNITED_STATES_AQI_TXT[6];  ///< US AQI descriptions
}

// ═══════════════════════════════════════════════════════════════════════════
// COMPASS POINT NOTATION
// ═══════════════════════════════════════════════════════════════════════════

extern const char *COMPASS_POINT_NOTATION[32];  ///< 32-point compass directions

// ═══════════════════════════════════════════════════════════════════════════
// HTTP CLIENT ERRORS
// ═══════════════════════════════════════════════════════════════════════════

extern const char *TXT_HTTPC_ERROR_CONNECTION_REFUSED;
extern const char *TXT_HTTPC_ERROR_SEND_HEADER_FAILED;
extern const char *TXT_HTTPC_ERROR_SEND_PAYLOAD_FAILED;
extern const char *TXT_HTTPC_ERROR_NOT_CONNECTED;
extern const char *TXT_HTTPC_ERROR_CONNECTION_LOST;
extern const char *TXT_HTTPC_ERROR_NO_STREAM;
extern const char *TXT_HTTPC_ERROR_NO_HTTP_SERVER;
extern const char *TXT_HTTPC_ERROR_TOO_LESS_RAM;
extern const char *TXT_HTTPC_ERROR_ENCODING;
extern const char *TXT_HTTPC_ERROR_STREAM_WRITE;
extern const char *TXT_HTTPC_ERROR_READ_TIMEOUT;

// ═══════════════════════════════════════════════════════════════════════════
// HTTP RESPONSE STATUS CODES (1xx-5xx)
// ═══════════════════════════════════════════════════════════════════════════

// 1xx Informational
extern const char *TXT_HTTP_RESPONSE_100;  ///< Continue
extern const char *TXT_HTTP_RESPONSE_101;  ///< Switching Protocols
extern const char *TXT_HTTP_RESPONSE_102;  ///< Processing
extern const char *TXT_HTTP_RESPONSE_103;  ///< Early Hints

// 2xx Success
extern const char *TXT_HTTP_RESPONSE_200;  ///< OK
extern const char *TXT_HTTP_RESPONSE_201;  ///< Created
extern const char *TXT_HTTP_RESPONSE_202;  ///< Accepted
extern const char *TXT_HTTP_RESPONSE_203;  ///< Non-Authoritative Information
extern const char *TXT_HTTP_RESPONSE_204;  ///< No Content
extern const char *TXT_HTTP_RESPONSE_205;  ///< Reset Content
extern const char *TXT_HTTP_RESPONSE_206;  ///< Partial Content
extern const char *TXT_HTTP_RESPONSE_207;  ///< Multi-Status
extern const char *TXT_HTTP_RESPONSE_208;  ///< Already Reported
extern const char *TXT_HTTP_RESPONSE_226;  ///< IM Used

// 3xx Redirection
extern const char *TXT_HTTP_RESPONSE_300;  ///< Multiple Choices
extern const char *TXT_HTTP_RESPONSE_301;  ///< Moved Permanently
extern const char *TXT_HTTP_RESPONSE_302;  ///< Found
extern const char *TXT_HTTP_RESPONSE_303;  ///< See Other
extern const char *TXT_HTTP_RESPONSE_304;  ///< Not Modified
extern const char *TXT_HTTP_RESPONSE_305;  ///< Use Proxy
extern const char *TXT_HTTP_RESPONSE_307;  ///< Temporary Redirect
extern const char *TXT_HTTP_RESPONSE_308;  ///< Permanent Redirect

// 4xx Client Errors
extern const char *TXT_HTTP_RESPONSE_400;  ///< Bad Request
extern const char *TXT_HTTP_RESPONSE_401;  ///< Unauthorized
extern const char *TXT_HTTP_RESPONSE_402;  ///< Payment Required
extern const char *TXT_HTTP_RESPONSE_403;  ///< Forbidden
extern const char *TXT_HTTP_RESPONSE_404;  ///< Not Found
extern const char *TXT_HTTP_RESPONSE_405;  ///< Method Not Allowed
extern const char *TXT_HTTP_RESPONSE_406;  ///< Not Acceptable
extern const char *TXT_HTTP_RESPONSE_407;  ///< Proxy Authentication Required
extern const char *TXT_HTTP_RESPONSE_408;  ///< Request Timeout
extern const char *TXT_HTTP_RESPONSE_409;  ///< Conflict
extern const char *TXT_HTTP_RESPONSE_410;  ///< Gone
extern const char *TXT_HTTP_RESPONSE_411;  ///< Length Required
extern const char *TXT_HTTP_RESPONSE_412;  ///< Precondition Failed
extern const char *TXT_HTTP_RESPONSE_413;  ///< Payload Too Large
extern const char *TXT_HTTP_RESPONSE_414;  ///< URI Too Long
extern const char *TXT_HTTP_RESPONSE_415;  ///< Unsupported Media Type
extern const char *TXT_HTTP_RESPONSE_416;  ///< Range Not Satisfiable
extern const char *TXT_HTTP_RESPONSE_417;  ///< Expectation Failed
extern const char *TXT_HTTP_RESPONSE_418;  ///< I'm a Teapot
extern const char *TXT_HTTP_RESPONSE_421;  ///< Misdirected Request
extern const char *TXT_HTTP_RESPONSE_422;  ///< Unprocessable Entity
extern const char *TXT_HTTP_RESPONSE_423;  ///< Locked
extern const char *TXT_HTTP_RESPONSE_424;  ///< Failed Dependency
extern const char *TXT_HTTP_RESPONSE_425;  ///< Too Early
extern const char *TXT_HTTP_RESPONSE_426;  ///< Upgrade Required
extern const char *TXT_HTTP_RESPONSE_428;  ///< Precondition Required
extern const char *TXT_HTTP_RESPONSE_429;  ///< Too Many Requests
extern const char *TXT_HTTP_RESPONSE_431;  ///< Request Header Fields Too Large
extern const char *TXT_HTTP_RESPONSE_451;  ///< Unavailable For Legal Reasons

// 5xx Server Errors
extern const char *TXT_HTTP_RESPONSE_500;  ///< Internal Server Error
extern const char *TXT_HTTP_RESPONSE_501;  ///< Not Implemented
extern const char *TXT_HTTP_RESPONSE_502;  ///< Bad Gateway
extern const char *TXT_HTTP_RESPONSE_503;  ///< Service Unavailable
extern const char *TXT_HTTP_RESPONSE_504;  ///< Gateway Timeout
extern const char *TXT_HTTP_RESPONSE_505;  ///< HTTP Version Not Supported
extern const char *TXT_HTTP_RESPONSE_506;  ///< Variant Also Negotiates
extern const char *TXT_HTTP_RESPONSE_507;  ///< Insufficient Storage
extern const char *TXT_HTTP_RESPONSE_508;  ///< Loop Detected
extern const char *TXT_HTTP_RESPONSE_510;  ///< Not Extended
extern const char *TXT_HTTP_RESPONSE_511;  ///< Network Authentication Required

// ═══════════════════════════════════════════════════════════════════════════
// ARDUINOJSON ERROR CODES
// ═══════════════════════════════════════════════════════════════════════════

extern const char *TXT_DESERIALIZATION_ERROR_OK;              ///< No error
extern const char *TXT_DESERIALIZATION_ERROR_EMPTY_INPUT;     ///< Empty input
extern const char *TXT_DESERIALIZATION_ERROR_INCOMPLETE_INPUT;///< Incomplete JSON
extern const char *TXT_DESERIALIZATION_ERROR_INVALID_INPUT;   ///< Invalid JSON
extern const char *TXT_DESERIALIZATION_ERROR_NO_MEMORY;       ///< Out of memory
extern const char *TXT_DESERIALIZATION_ERROR_TOO_DEEP;        ///< Nesting too deep

// ═══════════════════════════════════════════════════════════════════════════
// WIFI STATUS CODES
// ═══════════════════════════════════════════════════════════════════════════

extern const char *TXT_WL_NO_SHIELD;        ///< WiFi shield not present
extern const char *TXT_WL_IDLE_STATUS;      ///< WiFi idle
extern const char *TXT_WL_NO_SSID_AVAIL;    ///< SSID not available
extern const char *TXT_WL_SCAN_COMPLETED;   ///< Scan completed
extern const char *TXT_WL_CONNECTED;        ///< Connected
extern const char *TXT_WL_CONNECT_FAILED;   ///< Connection failed
extern const char *TXT_WL_CONNECTION_LOST;  ///< Connection lost
extern const char *TXT_WL_DISCONNECTED;     ///< Disconnected

#endif // ___LOCALE_H__
