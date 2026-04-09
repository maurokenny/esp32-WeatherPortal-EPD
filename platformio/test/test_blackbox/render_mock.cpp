/* Render Mock Implementation
 * Copyright (C) 2025
 * 
 * Uses conversions.cpp from firmware for unit conversions.
 */

#include "render_mock.h"
#include "conversions.h"
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <limits>
#include <cmath>
#include <cstdlib>

// Platform-specific networking
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #define CLOSE_SOCKET close
#endif

namespace blackbox {

// ============================================================================
// Unit Conversion Helpers (using firmware's conversions.cpp)
// ============================================================================

// Temperature conversion wrapper
static inline float convert_temperature(float celsius) {
#ifdef UNITS_TEMP_FAHRENHEIT
    return celsius_to_fahrenheit(celsius);
#elif defined(UNITS_TEMP_KELVIN)
    return celsius_to_kelvin(celsius);
#else
    return celsius;
#endif
}

static inline const char* temp_unit_str() {
#ifdef UNITS_TEMP_FAHRENHEIT
    return "F";
#elif defined(UNITS_TEMP_KELVIN)
    return "K";
#else
    return "C";
#endif
}

// Wind speed conversion wrapper (Open-Meteo returns km/h, firmware expects m/s)
// Note: We need to convert km/h -> m/s -> target unit
static inline float convert_wind_speed(float kmh) {
    float ms = kilometersperhour_to_meterspersecond(kmh);
#ifdef UNITS_SPEED_MILESPERHOUR
    return meterspersecond_to_milesperhour(ms);
#else
    return meterspersecond_to_kilometersperhour(ms);
#endif
}

static inline const char* wind_unit_str() {
#ifdef UNITS_SPEED_MILESPERHOUR
    return "mph";
#else
    return "km/h";
#endif
}

// Pressure conversion wrapper
static inline float convert_pressure(float hpa) {
#if defined(UNITS_PRES_MILLIMETERSOFMERCURY)
    return hectopascals_to_millimetersofmercury(hpa);
#elif defined(UNITS_PRES_INCHESOFMERCURY)
    return hectopascals_to_inchesofmercury(hpa);
#elif defined(UNITS_PRES_POUNDSPERSQUAREINCH)
    return hectopascals_to_poundspersquareinch(hpa);
#elif defined(UNITS_PRES_ATMOSPHERES)
    return hectopascals_to_atmospheres(hpa);
#elif defined(UNITS_PRES_PASCALS)
    return hectopascals_to_pascals(hpa);
#else
    return hpa;
#endif
}

static inline const char* pres_unit_str() {
#if defined(UNITS_PRES_MILLIMETERSOFMERCURY)
    return "mmHg";
#elif defined(UNITS_PRES_INCHESOFMERCURY)
    return "inHg";
#elif defined(UNITS_PRES_POUNDSPERSQUAREINCH)
    return "psi";
#elif defined(UNITS_PRES_ATMOSPHERES)
    return "atm";
#elif defined(UNITS_PRES_PASCALS)
    return "Pa";
#else
    return "hPa";
#endif
}

// Distance conversion wrapper (Open-Meteo returns km)
static inline float convert_distance(float km) {
#ifdef UNITS_DIST_MILES
    return km * 0.621371f;  // km to miles (same as meters_to_miles / 1000)
#else
    return km;
#endif
}

static inline const char* dist_unit_str() {
#ifdef UNITS_DIST_MILES
    return "mi";
#else
    return "km";
#endif
}

// Precipitation conversion wrapper
static inline float convert_precipitation(float mm) {
#if defined(UNITS_HOURLY_PRECIP_CENTIMETERS) || defined(UNITS_DAILY_PRECIP_CENTIMETERS)
    return millimeters_to_centimeters(mm);
#elif defined(UNITS_HOURLY_PRECIP_INCHES) || defined(UNITS_DAILY_PRECIP_INCHES)
    return millimeters_to_inches(mm);
#else
    return mm;
#endif
}

static inline const char* precip_unit_str() {
#if defined(UNITS_HOURLY_PRECIP_CENTIMETERS) || defined(UNITS_DAILY_PRECIP_CENTIMETERS)
    return "cm";
#elif defined(UNITS_HOURLY_PRECIP_INCHES) || defined(UNITS_DAILY_PRECIP_INCHES)
    return "in";
#else
    return "mm";
#endif
}

// ============================================================================
// RenderMock Implementation
// ============================================================================

RenderMock::RenderMock(HttpClient* client) : httpClient_(client) {}

void RenderMock::drawIcon(const char* iconName, int x, int y, int weatherCode) {
    RenderEvent event;
    event.event_type = "icon_drawn";
    event.icon_name = iconName;
    event.x = x;
    event.y = y;
    event.weather_code = weatherCode;
    addEvent(event);
}

void RenderMock::drawUmbrella(int pop, float precipitation, int x, int y) {
    RenderEvent event;
    event.event_type = "umbrella_drawn";
    event.icon_name = "umbrella";
    event.x = x;
    event.y = y;
    event.pop = pop;
    event.precipitation = precipitation;
    addEvent(event);
}

void RenderMock::drawText(const char* text, int x, int y) {
    RenderEvent event;
    event.event_type = "text_drawn";
    event.icon_name = text; // Store text content
    event.x = x;
    event.y = y;
    addEvent(event);
}

void RenderMock::drawTemperature(float temp, int x, int y) {
    RenderEvent event;
    event.event_type = "text_drawn";
    float displayTemp = convert_temperature(temp);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << displayTemp << temp_unit_str();
    event.icon_name = oss.str();
    event.x = x;
    event.y = y;
    addEvent(event);
}

void RenderMock::drawPop(int pop, int x, int y) {
    RenderEvent event;
    event.event_type = "pop_drawn";
    std::ostringstream oss;
    oss << pop << "%";
    event.icon_name = oss.str();
    event.x = x;
    event.y = y;
    event.pop = pop;
    addEvent(event);
}

void RenderMock::drawHumidity(int humidity, int x, int y) {
    RenderEvent event;
    event.event_type = "humidity_drawn";
    std::ostringstream oss;
    oss << humidity << "%";
    event.icon_name = oss.str();
    event.x = x;
    event.y = y;
    addEvent(event);
}

void RenderMock::drawWindSpeed(float windSpeed, int x, int y) {
    RenderEvent event;
    event.event_type = "wind_drawn";
    float displaySpeed = convert_wind_speed(windSpeed);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << displaySpeed << " " << wind_unit_str();
    event.icon_name = oss.str();
    event.x = x;
    event.y = y;
    addEvent(event);
}

void RenderMock::drawPressure(float pressure, int x, int y) {
    RenderEvent event;
    event.event_type = "pressure_drawn";
    float displayPressure = convert_pressure(pressure);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(0) << displayPressure << " " << pres_unit_str();
    event.icon_name = oss.str();
    event.x = x;
    event.y = y;
    addEvent(event);
}

void RenderMock::drawVisibility(float distance, int x, int y) {
    RenderEvent event;
    event.event_type = "visibility_drawn";
    float displayDist = convert_distance(distance);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << displayDist << " " << dist_unit_str();
    event.icon_name = oss.str();
    event.x = x;
    event.y = y;
    addEvent(event);
}

void RenderMock::drawPrecipitation(float amount, int x, int y) {
    RenderEvent event;
    event.event_type = "precipitation_drawn";
    float displayAmount = convert_precipitation(amount);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << displayAmount << " " << precip_unit_str();
    event.icon_name = oss.str();
    event.x = x;
    event.y = y;
    addEvent(event);
}

void RenderMock::renderWeatherScene(const OpenMeteoResponse& data) {
    // Determine weather icon based on code
    const char* iconName = weatherCodeToIconName(data.hourly.weather_code.empty() ? 0 : data.hourly.weather_code[0]);
    
    // Draw main weather icon
    drawIcon(iconName, 50, 50, data.hourly.weather_code.empty() ? 0 : data.hourly.weather_code[0]);
    
    // Draw umbrella if needed
    if (shouldShowUmbrella(data)) {
        int pop = data.getCurrentPop();
        float precip = data.getCurrentPrecipitation();
        drawUmbrella(pop, precip, 150, 50);
    }
    
    // Draw temperature (Open-Meteo returns Celsius)
    if (!data.hourly.temperature_2m.empty()) {
        drawTemperature(data.hourly.temperature_2m[0], 50, 150);
    }
    
    // Draw humidity
    if (!data.hourly.relative_humidity_2m.empty()) {
        drawHumidity(data.hourly.relative_humidity_2m[0], 150, 150);
    }
    
    // Draw wind speed (Open-Meteo returns km/h)
    if (!data.hourly.wind_speed_10m.empty()) {
        drawWindSpeed(data.hourly.wind_speed_10m[0], 50, 200);
    }
    
    // Draw pressure
    float pressure = data.getCurrentPressure();
    if (pressure > 0) {
        drawPressure(pressure, 150, 200);
    }
    
    // Draw visibility
    float visibility = data.getCurrentVisibility();
    if (visibility > 0) {
        drawVisibility(visibility, 50, 250);
    }
    
    // Draw precipitation amount
    float precip = data.getCurrentPrecipitation();
    if (precip > 0) {
        drawPrecipitation(precip, 150, 250);
    }
}

void RenderMock::addEvent(const RenderEvent& event) {
    events_.push_back(event);
}

bool RenderMock::submitToServer() {
    if (!httpClient_) return false;
    
    // Build JSON array of events
    std::string json = "[";
    for (size_t i = 0; i < events_.size(); ++i) {
        if (i > 0) json += ",";
        json += events_[i].toJson();
    }
    json += "]";
    
    return httpClient_->postEvents(json);
}

bool RenderMock::wasUmbrellaDrawn() const {
    for (const auto& event : events_) {
        if (event.event_type == "umbrella_drawn") {
            return true;
        }
    }
    return false;
}

bool RenderMock::wasIconDrawn(const char* iconName) const {
    for (const auto& event : events_) {
        if (event.event_type == "icon_drawn" && event.icon_name == iconName) {
            return true;
        }
    }
    return false;
}

bool RenderMock::wasTemperatureDrawn(float expectedTemp, float tolerance) const {
    for (const auto& event : events_) {
        if (event.event_type == "text_drawn") {
            // Parse the temperature value from the text (format: "XX.XU" where U is unit)
            float drawnTemp = 0;
            char unit;
            if (sscanf(event.icon_name.c_str(), "%f%c", &drawnTemp, &unit) == 2) {
                if (std::fabs(drawnTemp - expectedTemp) <= tolerance) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool RenderMock::wasHumidityDrawn(int expectedHumidity, int tolerance) const {
    for (const auto& event : events_) {
        if (event.event_type == "humidity_drawn" || event.event_type == "text_drawn") {
            int drawnHumidity = 0;
            if (sscanf(event.icon_name.c_str(), "%d%%", &drawnHumidity) == 1) {
                if (std::abs(drawnHumidity - expectedHumidity) <= tolerance) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool RenderMock::wasWindSpeedDrawn(float expectedWindSpeed, float tolerance) const {
    for (const auto& event : events_) {
        if (event.event_type == "wind_drawn") {
            float drawnSpeed = 0;
            char unit[10];
            if (sscanf(event.icon_name.c_str(), "%f %9s", &drawnSpeed, unit) >= 1) {
                if (std::fabs(drawnSpeed - expectedWindSpeed) <= tolerance) {
                    return true;
                }
            }
        }
    }
    return false;
}

float RenderMock::getDrawnTemperature() const {
    for (const auto& event : events_) {
        if (event.event_type == "text_drawn") {
            float drawnTemp = 0;
            char unit;
            if (sscanf(event.icon_name.c_str(), "%f%c", &drawnTemp, &unit) == 2) {
                return drawnTemp;
            }
        }
    }
    return std::numeric_limits<float>::quiet_NaN();
}

float RenderMock::getDrawnHumidity() const {
    for (const auto& event : events_) {
        if (event.event_type == "humidity_drawn") {
            int drawnHumidity = 0;
            if (sscanf(event.icon_name.c_str(), "%d%%", &drawnHumidity) == 1) {
                return static_cast<float>(drawnHumidity);
            }
        }
    }
    return std::numeric_limits<float>::quiet_NaN();
}

float RenderMock::getDrawnWindSpeed() const {
    for (const auto& event : events_) {
        if (event.event_type == "wind_drawn") {
            float drawnSpeed = 0;
            if (sscanf(event.icon_name.c_str(), "%f", &drawnSpeed) == 1) {
                return drawnSpeed;
            }
        }
    }
    return std::numeric_limits<float>::quiet_NaN();
}

float RenderMock::getDrawnPressure() const {
    for (const auto& event : events_) {
        if (event.event_type == "pressure_drawn") {
            float drawnPressure = 0;
            if (sscanf(event.icon_name.c_str(), "%f", &drawnPressure) == 1) {
                return drawnPressure;
            }
        }
    }
    return std::numeric_limits<float>::quiet_NaN();
}

float RenderMock::getDrawnVisibility() const {
    for (const auto& event : events_) {
        if (event.event_type == "visibility_drawn") {
            float drawnVis = 0;
            if (sscanf(event.icon_name.c_str(), "%f", &drawnVis) == 1) {
                return drawnVis;
            }
        }
    }
    return std::numeric_limits<float>::quiet_NaN();
}

float RenderMock::getDrawnPrecipitation() const {
    for (const auto& event : events_) {
        if (event.event_type == "precipitation_drawn") {
            float drawnPrecip = 0;
            if (sscanf(event.icon_name.c_str(), "%f", &drawnPrecip) == 1) {
                return drawnPrecip;
            }
        }
    }
    return std::numeric_limits<float>::quiet_NaN();
}

const char* RenderMock::weatherCodeToIconName(int code) const {
    // WMO Weather interpretation codes (WW)
    // https://open-meteo.com/en/docs
    if (code == 0) return "sun";           // Clear sky
    if (code == 1 || code == 2 || code == 3) return "partly_cloudy"; // Mainly clear, partly cloudy, overcast
    if (code == 45 || code == 48) return "fog"; // Fog
    if (code == 51 || code == 53 || code == 55) return "drizzle"; // Drizzle
    if (code == 56 || code == 57) return "freezing_drizzle"; // Freezing drizzle
    if (code == 61 || code == 63 || code == 65) return "rain"; // Rain
    if (code == 66 || code == 67) return "freezing_rain"; // Freezing rain
    if (code == 71 || code == 73 || code == 75) return "snow"; // Snow fall
    if (code == 77) return "snow_grains"; // Snow grains
    if (code == 80 || code == 81 || code == 82) return "rain_showers"; // Rain showers
    if (code == 85 || code == 86) return "snow_showers"; // Snow showers
    if (code == 95) return "thunder"; // Thunderstorm
    if (code == 96 || code == 99) return "thunder_hail"; // Thunderstorm with hail
    return "unknown";
}

bool RenderMock::shouldShowUmbrella(const OpenMeteoResponse& data) const {
    // Umbrella logic: show if POP >= 30% or if there's current precipitation
    int pop = data.getCurrentPop();
    float precip = data.getCurrentPrecipitation();
    return (pop >= 30) || (precip > 0.1f);
}

// ============================================================================
// RenderEvent Implementation
// ============================================================================

std::string RenderEvent::toJson() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"event_type\":\"" << event_type << "\",";
    oss << "\"icon_name\":\"" << icon_name << "\",";
    oss << "\"x\":" << x << ",";
    oss << "\"y\":" << y << ",";
    oss << "\"weather_code\":" << weather_code << ",";
    oss << "\"pop\":" << pop << ",";
    oss << "\"precipitation\":" << precipitation;
    oss << "}";
    return oss.str();
}

// ============================================================================
// SimpleHttpClient Implementation
// ============================================================================

SimpleHttpClient::SimpleHttpClient(const std::string& baseUrl) 
    : baseUrl_(baseUrl), timeoutMs_(5000) {}

SimpleHttpClient::~SimpleHttpClient() = default;

std::string SimpleHttpClient::extractHost(const std::string& url) {
    size_t start = url.find("://");
    if (start == std::string::npos) start = 0;
    else start += 3;
    
    size_t end = url.find(":", start);
    if (end == std::string::npos) end = url.find("/", start);
    if (end == std::string::npos) end = url.length();
    
    return url.substr(start, end - start);
}

int SimpleHttpClient::extractPort(const std::string& url) {
    size_t start = url.find("://");
    if (start == std::string::npos) start = 0;
    else start += 3;
    
    size_t portStart = url.find(":", start);
    if (portStart == std::string::npos) {
        // Default port
        return url.find("https://") == 0 ? 443 : 80;
    }
    
    size_t portEnd = url.find("/", portStart);
    if (portEnd == std::string::npos) portEnd = url.length();
    
    return std::stoi(url.substr(portStart + 1, portEnd - portStart - 1));
}

std::string SimpleHttpClient::extractPath(const std::string& url) {
    size_t start = url.find("://");
    if (start == std::string::npos) start = 0;
    else start += 3;
    
    size_t pathStart = url.find("/", start);
    if (pathStart == std::string::npos) return "/";
    
    return url.substr(pathStart);
}

SimpleHttpClient::Response SimpleHttpClient::request(
    const std::string& method,
    const std::string& path,
    const std::string& body) {
    
    Response resp = {0, "", false};
    
    std::string host = extractHost(baseUrl_);
    int port = extractPort(baseUrl_);
    
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return resp;
    }
#endif
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return resp;
    }
    
    // Set timeout
    struct timeval tv;
    tv.tv_sec = timeoutMs_ / 1000;
    tv.tv_usec = (timeoutMs_ % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
    
    struct hostent* server = gethostbyname(host.c_str());
    if (!server) {
        CLOSE_SOCKET(sock);
        return resp;
    }
    
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);
    serverAddr.sin_port = htons(port);
    
    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        CLOSE_SOCKET(sock);
        return resp;
    }
    
    // Build HTTP request
    std::string request = method + " " + path + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    request += "Connection: close\r\n";
    
    if (method == "POST" || method == "PUT") {
        request += "Content-Type: application/json\r\n";
        request += "Content-Length: " + std::to_string(body.length()) + "\r\n";
    }
    
    request += "\r\n";
    
    if (!body.empty()) {
        request += body;
    }
    
    // Send request
    if (send(sock, request.c_str(), request.length(), 0) < 0) {
        CLOSE_SOCKET(sock);
        return resp;
    }
    
    // Receive response
    char buffer[4096];
    std::string response;
    int bytesRead;
    
    while ((bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
        response += buffer;
    }
    
    CLOSE_SOCKET(sock);
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    // Parse response
    size_t statusStart = response.find("HTTP/1.1 ");
    if (statusStart != std::string::npos) {
        statusStart += 9;
        resp.statusCode = std::stoi(response.substr(statusStart, 3));
    }
    
    size_t bodyStart = response.find("\r\n\r\n");
    if (bodyStart != std::string::npos) {
        resp.body = response.substr(bodyStart + 4);
    }
    
    resp.success = (resp.statusCode >= 200 && resp.statusCode < 300);
    return resp;
}

std::string SimpleHttpClient::get(const std::string& path) {
    Response resp = request("GET", path, "");
    return resp.success ? resp.body : "";
}

bool SimpleHttpClient::post(const std::string& path, const std::string& body) {
    Response resp = request("POST", path, body);
    return resp.success;
}

std::string SimpleHttpClient::postWithResponse(const std::string& path, const std::string& body) {
    Response resp = request("POST", path, body);
    return resp.success ? resp.body : "";
}

bool SimpleHttpClient::postEvents(const std::string& jsonBody) {
    return post("/test/events", jsonBody);
}

} // namespace blackbox
