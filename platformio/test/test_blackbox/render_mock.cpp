/* Render Mock Implementation
 * Copyright (C) 2025
 */

#include "render_mock.h"
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <limits>
#include <cmath>
#include <cstdlib>

// Unit detection based on config defines (must be before namespace)
#ifdef UNITS_TEMP_FAHRENHEIT
    #define RENDER_UNIT_TEMP_FAHRENHEIT 1
#else
    #define RENDER_UNIT_TEMP_FAHRENHEIT 0
#endif

#ifdef UNITS_SPEED_MILESPERHOUR
    #define RENDER_UNIT_SPEED_MPH 1
#else
    #define RENDER_UNIT_SPEED_MPH 0
#endif

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
// Unit Conversion Helpers
// ============================================================================

#if RENDER_UNIT_TEMP_FAHRENHEIT
static inline float convert_temperature(float celsius) {
    return celsius * 9.0f / 5.0f + 32.0f;
}
static inline const char* temp_unit_str() { return "F"; }
#else
static inline float convert_temperature(float temp) { return temp; }
static inline const char* temp_unit_str() { return "C"; }
#endif

#if RENDER_UNIT_SPEED_MPH
static inline float convert_wind_speed(float kmh) {
    return kmh / 1.60934f;
}
static inline const char* wind_unit_str() { return "mph"; }
#else
static inline float convert_wind_speed(float speed) { return speed; }
static inline const char* wind_unit_str() { return "km/h"; }
#endif

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
    oss << std::fixed << std::setprecision(1) << displaySpeed << wind_unit_str();
    event.icon_name = oss.str();
    event.x = x;
    event.y = y;
    addEvent(event);
}

void RenderMock::renderWeatherScene(const OpenMeteoResponse& data) {
    clearEvents();
    
    // Get weather code and map to icon
    int weatherCode = data.hourly.getDominantWeatherCode();
    const char* iconName = weatherCodeToIconName(weatherCode);
    
    // Draw main weather icon
    drawIcon(iconName, 50, 50, weatherCode);
    
    // Draw temperature (first hour/current)
    if (!data.hourly.temperature_2m.empty()) {
        drawTemperature(data.hourly.temperature_2m[0], 150, 50);
    }
    
    // Draw precipitation probability
    int pop = data.getCurrentPop();
    drawPop(pop, 150, 100);
    
    // Draw humidity (first hour/current)
    if (!data.hourly.relative_humidity_2m.empty()) {
        drawHumidity(data.hourly.relative_humidity_2m[0], 150, 150);
    }
    
    // Draw wind speed (first hour/current)
    if (!data.hourly.wind_speed_10m.empty()) {
        drawWindSpeed(data.hourly.wind_speed_10m[0], 150, 200);
    }
    
    // Draw umbrella if needed (firmware logic)
    if (shouldShowUmbrella(data)) {
        drawUmbrella(pop, data.getCurrentPrecipitation(), 250, 50);
    }
}

bool RenderMock::submitToServer() {
    if (!httpClient_ || events_.empty()) {
        return false;
    }
    
    // Build JSON array
    std::ostringstream json;
    json << "[";
    for (size_t i = 0; i < events_.size(); ++i) {
        if (i > 0) json << ",";
        json << events_[i].toJson();
    }
    json << "]";
    
    return httpClient_->postEvents(json.str());
}

bool RenderMock::wasUmbrellaDrawn() const {
    return std::any_of(events_.begin(), events_.end(),
        [](const RenderEvent& e) {
            return e.event_type == "umbrella_drawn" || 
                   (e.event_type == "icon_drawn" && e.icon_name == "umbrella");
        });
}

bool RenderMock::wasIconDrawn(const char* iconName) const {
    return std::any_of(events_.begin(), events_.end(),
        [iconName](const RenderEvent& e) {
            return e.event_type == "icon_drawn" && e.icon_name == iconName;
        });
}

bool RenderMock::wasTemperatureDrawn(float expectedTemp, float tolerance) const {
    float drawn = getDrawnTemperature();
    if (std::isnan(drawn)) return false;
    return std::abs(drawn - expectedTemp) <= tolerance;
}

bool RenderMock::wasHumidityDrawn(int expectedHumidity, int tolerance) const {
    int drawn = getDrawnHumidity();
    if (drawn < 0) return false;
    return std::abs(drawn - expectedHumidity) <= tolerance;
}

bool RenderMock::wasWindSpeedDrawn(float expectedWindSpeed, float tolerance) const {
    float drawn = getDrawnWindSpeed();
    if (std::isnan(drawn)) return false;
    return std::abs(drawn - expectedWindSpeed) <= tolerance;
}

float RenderMock::getDrawnTemperature() const {
    for (const auto& e : events_) {
        if (e.event_type == "text_drawn") {
            // Check for C or F suffix
            size_t posC = e.icon_name.find("C");
            size_t posF = e.icon_name.find("F");
            size_t pos = (posC != std::string::npos) ? posC : posF;
            if (pos != std::string::npos && pos > 0) {
                try {
                    return std::stof(e.icon_name.substr(0, pos));
                } catch (...) {
                    return std::numeric_limits<float>::quiet_NaN();
                }
            }
        }
    }
    return std::numeric_limits<float>::quiet_NaN();
}

int RenderMock::getDrawnHumidity() const {
    for (const auto& e : events_) {
        if (e.event_type == "humidity_drawn") {
            // Parse humidity from string like "45%"
            size_t pos = e.icon_name.find("%");
            if (pos != std::string::npos) {
                try {
                    return std::stoi(e.icon_name.substr(0, pos));
                } catch (...) {
                    return -1;
                }
            }
        }
    }
    return -1;
}

float RenderMock::getDrawnWindSpeed() const {
    for (const auto& e : events_) {
        if (e.event_type == "wind_drawn") {
            // Check for km/h or mph suffix
            size_t posKmh = e.icon_name.find("km/h");
            size_t posMph = e.icon_name.find("mph");
            size_t pos = (posKmh != std::string::npos) ? posKmh : posMph;
            if (pos != std::string::npos && pos > 0) {
                try {
                    return std::stof(e.icon_name.substr(0, pos));
                } catch (...) {
                    return std::numeric_limits<float>::quiet_NaN();
                }
            }
        }
    }
    return std::numeric_limits<float>::quiet_NaN();
}

void RenderMock::addEvent(const RenderEvent& event) {
    events_.push_back(event);
}

const char* RenderMock::weatherCodeToIconName(int code) const {
    // WMO Weather code to icon mapping (simplified)
    if (code == 0 || code == 1) return "sun";
    if (code == 2) return "partly_cloudy";
    if (code == 3) return "cloud";
    if (code >= 45 && code <= 48) return "fog";
    if (code >= 51 && code <= 55) return "drizzle";
    if (code >= 61 && code <= 67) return "rain";
    if (code >= 71 && code <= 77) return "snow";
    if (code >= 80 && code <= 82) return "showers";
    if (code >= 85 && code <= 86) return "snow_showers";
    if (code >= 95 && code <= 99) return "thunder";
    return "unknown";
}

bool RenderMock::shouldShowUmbrella(const OpenMeteoResponse& data) const {
    // Mirror the firmware's umbrella decision logic
    return data.shouldShowUmbrella();
}

// ============================================================================
// SimpleHttpClient Implementation
// ============================================================================

SimpleHttpClient::SimpleHttpClient(const std::string& baseUrl) 
    : baseUrl_(baseUrl), timeoutMs_(5000) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

SimpleHttpClient::~SimpleHttpClient() {
#ifdef _WIN32
    WSACleanup();
#endif
}

bool SimpleHttpClient::postEvents(const std::string& jsonBody) {
    auto resp = request("POST", "/test/events", jsonBody);
    return resp.success && resp.statusCode == 200;
}

std::string SimpleHttpClient::get(const std::string& path) {
    auto resp = request("GET", path, "");
    return resp.body;
}

bool SimpleHttpClient::post(const std::string& path, const std::string& body) {
    auto resp = request("POST", path, body);
    return resp.success;
}

std::string SimpleHttpClient::postWithResponse(const std::string& path, const std::string& body) {
    auto resp = request("POST", path, body);
    if (resp.success && resp.statusCode == 200) {
        return resp.body;
    }
    return "";
}

std::string SimpleHttpClient::extractHost(const std::string& url) {
    size_t start = url.find("://");
    start = (start == std::string::npos) ? 0 : start + 3;
    size_t end = url.find(':', start);
    if (end == std::string::npos) {
        end = url.find('/', start);
    }
    return url.substr(start, end - start);
}

int SimpleHttpClient::extractPort(const std::string& url) {
    size_t start = url.find("://");
    start = (start == std::string::npos) ? 0 : start + 3;
    size_t portStart = url.find(':', start);
    if (portStart == std::string::npos) {
        return 80;
    }
    size_t portEnd = url.find('/', portStart);
    if (portEnd == std::string::npos) {
        return std::atoi(url.substr(portStart + 1).c_str());
    }
    return std::atoi(url.substr(portStart + 1, portEnd - portStart - 1).c_str());
}

std::string SimpleHttpClient::extractPath(const std::string& url) {
    size_t start = url.find("://");
    start = (start == std::string::npos) ? 0 : start + 3;
    size_t pathStart = url.find('/', start);
    if (pathStart == std::string::npos) {
        return "";
    }
    return url.substr(pathStart);
}

SimpleHttpClient::Response SimpleHttpClient::request(
    const std::string& method, 
    const std::string& path, 
    const std::string& body) {
    
    Response resp = {0, "", false};
    
    std::string host = extractHost(baseUrl_);
    int port = extractPort(baseUrl_);
    std::string basePath = extractPath(baseUrl_);
    
    // Debug output
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return resp;
    }
    
    // Resolve hostname
    struct hostent* server = gethostbyname(host.c_str());
    if (!server) {
        CLOSE_SOCKET(sock);
        return resp;
    }
    
    // Connect
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    std::memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        CLOSE_SOCKET(sock);
        return resp;
    }
    
    // Build HTTP request
    std::string request = method + " " + basePath + path + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    request += "Connection: close\r\n";
    
    if (!body.empty()) {
        request += "Content-Type: application/json\r\n";
    }
    request += "Content-Length: " + std::to_string(body.length()) + "\r\n";
    
    request += "\r\n";
    if (!body.empty()) {
        request += body;
    }
    
    // Send
    if (send(sock, request.c_str(), request.length(), 0) < 0) {
        CLOSE_SOCKET(sock);
        return resp;
    }
    
    // Receive
    char buffer[8192];
    std::string response;
    int bytesRead;
    
    while ((bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
        response += buffer;
    }
    
    CLOSE_SOCKET(sock);
    
    // Parse response
    size_t headerEnd = response.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return resp;
    }
    
    std::string headers = response.substr(0, headerEnd);
    resp.body = response.substr(headerEnd + 4);
    
    // Parse status code
    size_t statusStart = headers.find(" ") + 1;
    size_t statusEnd = headers.find(" ", statusStart);
    std::string statusStr = headers.substr(statusStart, statusEnd - statusStart);
    resp.statusCode = std::atoi(statusStr.c_str());
    resp.success = (resp.statusCode >= 200 && resp.statusCode < 300);
    
    return resp;
}

} // namespace blackbox
