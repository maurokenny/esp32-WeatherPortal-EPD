/* Render Mock Implementation
 * Copyright (C) 2025
 * 
 * Uses conversions.cpp from firmware for unit conversions.
 * Works with owm_resp_onecall_t structure.
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

// Temperature: Kelvin → display units
static inline float convertTemperatureFromKelvin(float kelvin) {
#ifdef UNITS_TEMP_FAHRENHEIT
    return kelvin_to_fahrenheit(kelvin);
#elif defined(UNITS_TEMP_KELVIN)
    return kelvin;
#else
    return kelvin_to_celsius(kelvin);
#endif
}

static inline const char* tempUnitStr() {
#ifdef UNITS_TEMP_FAHRENHEIT
    return "F";
#elif defined(UNITS_TEMP_KELVIN)
    return "K";
#else
    return "C";
#endif
}

// Wind speed: m/s → display units
static inline float convertWindSpeedFromMs(float ms) {
#ifdef UNITS_SPEED_MILESPERHOUR
    return meterspersecond_to_milesperhour(ms);
#else
    return meterspersecond_to_kilometersperhour(ms);
#endif
}

static inline const char* windUnitStr() {
#ifdef UNITS_SPEED_MILESPERHOUR
    return "mph";
#else
    return "km/h";
#endif
}

// Pressure: hPa → display units
static inline float convertPressure(float hpa) {
#if defined(UNITS_PRES_MILLIMETERSOFMERCURY)
    return hectopascals_to_millimetersofmercury(hpa);
#elif defined(UNITS_PRES_INCHESOFMERCURY)
    return hectopascals_to_inchesofmercury(hpa);
#elif defined(UNITS_PRES_POUNDSPERSQUAREINCH)
    return hectopascals_to_poundspersquareinch(hpa);
#else
    return hpa;
#endif
}

static inline const char* presUnitStr() {
#if defined(UNITS_PRES_MILLIMETERSOFMERCURY)
    return "mmHg";
#elif defined(UNITS_PRES_INCHESOFMERCURY)
    return "inHg";
#elif defined(UNITS_PRES_POUNDSPERSQUAREINCH)
    return "psi";
#else
    return "hPa";
#endif
}

// Distance: meters → display units (km or miles)
static inline float convertDistanceFromMeters(int meters) {
    float km = static_cast<float>(meters) / 1000.0f;
#ifdef UNITS_DIST_MILES
    return km * 0.621371f;  // km to miles
#else
    return km;
#endif
}

static inline const char* distUnitStr() {
#ifdef UNITS_DIST_MILES
    return "mi";
#else
    return "km";
#endif
}

// Precipitation: mm → display units
static inline float convertPrecipitation(float mm) {
#if defined(UNITS_HOURLY_PRECIP_CENTIMETERS) || defined(UNITS_DAILY_PRECIP_CENTIMETERS)
    return millimeters_to_centimeters(mm);
#elif defined(UNITS_HOURLY_PRECIP_INCHES) || defined(UNITS_DAILY_PRECIP_INCHES)
    return millimeters_to_inches(mm);
#else
    return mm;
#endif
}

static inline const char* precipUnitStr() {
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
    event.pop = pop;  // 0-100%
    event.precipitation = precipitation;
    addEvent(event);
}

void RenderMock::drawText(const char* text, int x, int y) {
    RenderEvent event;
    event.event_type = "text_drawn";
    event.icon_name = text;
    event.x = x;
    event.y = y;
    addEvent(event);
}

void RenderMock::drawTemperature(float tempKelvin, int x, int y) {
    RenderEvent event;
    event.event_type = "text_drawn";
    float displayTemp = convertTemperatureFromKelvin(tempKelvin);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << displayTemp << tempUnitStr();
    event.icon_name = oss.str();
    event.x = x;
    event.y = y;
    addEvent(event);
}

void RenderMock::drawPop(float pop, int x, int y) {
    RenderEvent event;
    event.event_type = "pop_drawn";
    int popPercent = static_cast<int>(pop * 100.0f);
    std::ostringstream oss;
    oss << popPercent << "%";
    event.icon_name = oss.str();
    event.x = x;
    event.y = y;
    event.pop = popPercent;
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

void RenderMock::drawWindSpeed(float windSpeedMs, int x, int y) {
    RenderEvent event;
    event.event_type = "wind_drawn";
    float displaySpeed = convertWindSpeedFromMs(windSpeedMs);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << displaySpeed << " " << windUnitStr();
    event.icon_name = oss.str();
    event.x = x;
    event.y = y;
    addEvent(event);
}

void RenderMock::drawPressure(float pressureHpa, int x, int y) {
    RenderEvent event;
    event.event_type = "pressure_drawn";
    float displayPressure = convertPressure(pressureHpa);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(0) << displayPressure << " " << presUnitStr();
    event.icon_name = oss.str();
    event.x = x;
    event.y = y;
    addEvent(event);
}

void RenderMock::drawVisibility(int visibilityMeters, int x, int y) {
    RenderEvent event;
    event.event_type = "visibility_drawn";
    float displayDist = convertDistanceFromMeters(visibilityMeters);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << displayDist << " " << distUnitStr();
    event.icon_name = oss.str();
    event.x = x;
    event.y = y;
    addEvent(event);
}

void RenderMock::drawPrecipitation(float amountMm, int x, int y) {
    RenderEvent event;
    event.event_type = "precipitation_drawn";
    float displayAmount = convertPrecipitation(amountMm);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << displayAmount << " " << precipUnitStr();
    event.icon_name = oss.str();
    event.x = x;
    event.y = y;
    addEvent(event);
}

void RenderMock::renderWeatherScene(const owm_resp_onecall_t& data) {
    // Get weather info from current conditions
    int weatherId = data.current.weather.id;
    const char* iconName = weatherCodeToIconName(weatherId);
    
    // Draw main weather icon
    drawIcon(iconName, 50, 50, weatherId);
    
    // Draw umbrella if needed (POP >= 30% or precipitation > 0.1mm)
    int popPercent = getCurrentPop(data);
    float precip = getCurrentPrecipitation(data);
    if (shouldShowUmbrella(data)) {
        drawUmbrella(popPercent, precip, 150, 50);
    }
    
    // Draw temperature (from current, in Kelvin)
    drawTemperature(data.current.temp, 50, 150);
    
    // Draw humidity (from current)
    drawHumidity(data.current.humidity, 150, 150);
    
    // Draw wind speed (from current, in m/s)
    drawWindSpeed(data.current.wind_speed, 50, 200);
    
    // Draw pressure (from current, in hPa)
    if (data.current.pressure > 0) {
        drawPressure(static_cast<float>(data.current.pressure), 150, 200);
    }
    
    // Draw visibility (from current, in meters)
    if (data.current.visibility > 0) {
        drawVisibility(data.current.visibility, 50, 250);
    }
    
    // Draw precipitation amount (rain + snow)
    if (precip > 0.0f) {
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
    // OWM weather codes
    if (code == 800) return "sun";           // Clear sky
    if (code == 801) return "partly_cloudy"; // Few clouds
    if (code == 802) return "partly_cloudy"; // Scattered clouds
    if (code == 803 || code == 804) return "cloud"; // Broken/overcast clouds
    if (code >= 200 && code < 300) return "thunder"; // Thunderstorm
    if (code >= 300 && code < 400) return "drizzle"; // Drizzle
    if (code >= 500 && code < 600) return "rain";    // Rain
    if (code >= 600 && code < 700) return "snow";    // Snow
    if (code >= 700 && code < 800) return "fog";     // Atmosphere
    return "unknown";
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
