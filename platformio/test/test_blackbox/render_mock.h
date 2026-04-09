/* Render Mock for Blackbox Testing
 * Copyright (C) 2025
 *
 * Mock implementation of the display renderer that captures drawing
 * operations and sends them to the mock server via HTTP POST.
 * 
 * Component: Render (Mock)
 * Architecture: Firmware → Render Mock → HTTP POST → Mock Server
 */

#ifndef RENDER_MOCK_H
#define RENDER_MOCK_H

#include "api_response.h"
#include <vector>
#include <string>
#include <functional>

namespace blackbox {

/**
 * HTTP Client interface for sending events to mock server
 */
class HttpClient {
public:
    virtual ~HttpClient() = default;
    virtual bool postEvents(const std::string& jsonBody) = 0;
    virtual std::string get(const std::string& path) = 0;
    virtual bool post(const std::string& path, const std::string& body) = 0;
    virtual std::string postWithResponse(const std::string& path, const std::string& body) = 0;
};

/**
 * Render Mock - Captures display operations and sends to mock server
 * 
 * This class simulates the firmware's renderer without actual display hardware.
 * Instead of drawing to e-paper, it records events and submits them to the
 * mock server for validation.
 */
class RenderMock {
public:
    RenderMock(HttpClient* client = nullptr);
    ~RenderMock() = default;
    
    // Prevent copying
    RenderMock(const RenderMock&) = delete;
    RenderMock& operator=(const RenderMock&) = delete;
    
    // -------------------------------------------------------------------------
    // Drawing Operations (captured as events)
    // -------------------------------------------------------------------------
    
    /**
     * Draw a weather icon at specified position
     */
    void drawIcon(const char* iconName, int x, int y, int weatherCode = 0);
    
    /**
     * Draw the umbrella icon (special widget)
     */
    void drawUmbrella(int pop, float precipitation, int x = 0, int y = 0);
    
    /**
     * Draw text at specified position
     */
    void drawText(const char* text, int x, int y);
    
    /**
     * Draw temperature display
     */
    void drawTemperature(float temp, int x, int y);
    
    /**
     * Draw precipitation probability
     */
    void drawPop(int pop, int x, int y);
    
    /**
     * Draw humidity percentage
     */
    void drawHumidity(int humidity, int x, int y);
    
    /**
     * Draw wind speed
     */
    void drawWindSpeed(float windSpeed, int x, int y);
    
    /**
     * Draw atmospheric pressure
     */
    void drawPressure(float pressure, int x, int y);
    
    /**
     * Render complete weather scene based on Open-Meteo data
     * This is the high-level method that orchestrates all drawing
     */
    void renderWeatherScene(const OpenMeteoResponse& data);
    
    // -------------------------------------------------------------------------
    // Event Management
    // -------------------------------------------------------------------------
    
    /**
     * Submit captured events to mock server
     * HTTP POST /test/events
     */
    bool submitToServer();
    
    /**
     * Get all captured events
     */
    const std::vector<RenderEvent>& getCapturedEvents() const { return events_; }
    
    /**
     * Clear captured events
     */
    void clearEvents() { events_.clear(); }
    
    /**
     * Get number of captured events
     */
    size_t getEventCount() const { return events_.size(); }
    
    /**
     * Check if umbrella was drawn
     */
    bool wasUmbrellaDrawn() const;
    
    /**
     * Check if specific icon was drawn
     */
    bool wasIconDrawn(const char* iconName) const;
    
    /**
     * Check if temperature was drawn with specific value (within tolerance)
     */
    bool wasTemperatureDrawn(float expectedTemp, float tolerance = 0.5f) const;
    
    /**
     * Check if humidity was drawn with specific value (within tolerance)
     */
    bool wasHumidityDrawn(int expectedHumidity, int tolerance = 2) const;
    
    /**
     * Check if wind speed was drawn with specific value (within tolerance)
     */
    bool wasWindSpeedDrawn(float expectedWindSpeed, float tolerance = 1.0f) const;
    
    /**
     * Get the drawn temperature value (0 if not found)
     */
    float getDrawnTemperature() const;
    
    /**
     * Get the drawn humidity value (0 if not found)
     */
    int getDrawnHumidity() const;
    
    /**
     * Get the drawn wind speed value (0 if not found)
     */
    float getDrawnWindSpeed() const;
    
    /**
     * Get the drawn pressure value (0 if not found)
     */
    float getDrawnPressure() const;
    
    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------
    
    void setHttpClient(HttpClient* client) { httpClient_ = client; }
    
private:
    std::vector<RenderEvent> events_;
    HttpClient* httpClient_;
    
    // Helper to add event
    void addEvent(const RenderEvent& event);
    
    // Map weather code to icon name
    const char* weatherCodeToIconName(int code) const;
    
    // Determine if umbrella should be shown (mirrors firmware logic)
    bool shouldShowUmbrella(const OpenMeteoResponse& data) const;
};

/**
 * Simple HTTP client implementation for testing
 * Uses native sockets to communicate with mock server
 */
class SimpleHttpClient : public HttpClient {
public:
    explicit SimpleHttpClient(const std::string& baseUrl);
    ~SimpleHttpClient() override;
    
    bool postEvents(const std::string& jsonBody) override;
    std::string get(const std::string& path) override;
    bool post(const std::string& path, const std::string& body) override;
    std::string postWithResponse(const std::string& path, const std::string& body) override;
    
    void setTimeout(int milliseconds) { timeoutMs_ = milliseconds; }
    
private:
    std::string baseUrl_;
    int timeoutMs_;
    
    struct Response {
        int statusCode;
        std::string body;
        bool success;
    };
    
    Response request(const std::string& method, const std::string& path, 
                     const std::string& body = "");
    
    // URL parsing helpers
    std::string extractHost(const std::string& url);
    int extractPort(const std::string& url);
    std::string extractPath(const std::string& url);
};

} // namespace blackbox

#endif // RENDER_MOCK_H
