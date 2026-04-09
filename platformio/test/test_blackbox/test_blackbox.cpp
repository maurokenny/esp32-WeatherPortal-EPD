/* Blackbox Integration Tests - Main Test Suite
 * Copyright (C) 2025
 *
 * Architecture Flow:
 *   Test Blackbox (Unity) 
 *        ↓
 *   Parse JSON (parseOpenMeteo) → owm_resp_onecall_t
 *        ↓
 *   Render (Mock) → HTTP POST → Mock Server
 *        ↓
 *   Validate (Pattern Matching)
 *
 * Uses firmware's owm_resp_onecall_t structure for data compatibility.
 */

#include <unity.h>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cmath>
#ifndef _WIN32
    #include <unistd.h>
#endif

// =============================================================================
// Unit-Aware Test Expectations
// =============================================================================
// These macros convert base values (Kelvin, m/s, hPa) to the configured units
// Note: Now using OWM format internally (Kelvin, m/s, hPa, meters)

// Temperature expectations (base: Kelvin)
#ifdef UNITS_TEMP_FAHRENHEIT
    #define TEST_TEMP_K(K) ((K) * 9.0f / 5.0f - 459.67f)  // K → F
    #define TEST_TEMP_UNIT "F"
#elif defined(UNITS_TEMP_KELVIN)
    #define TEST_TEMP_K(K) (K)
    #define TEST_TEMP_UNIT "K"
#else
    #define TEST_TEMP_K(K) ((K) - 273.15f)  // K → C
    #define TEST_TEMP_UNIT "C"
#endif

// Wind speed expectations (base: m/s)
#ifdef UNITS_SPEED_MILESPERHOUR
    #define TEST_WIND_MS(MS) ((MS) * 2.237f)  // m/s → mph
    #define TEST_WIND_UNIT "mph"
#else
    #define TEST_WIND_MS(MS) ((MS) * 3.6f)  // m/s → km/h
    #define TEST_WIND_UNIT "km/h"
#endif

// Pressure expectations (base: hPa)
#ifdef UNITS_PRES_MILLIMETERSOFMERCURY
    #define TEST_PRES(HP) ((HP) * 0.750062f)
    #define TEST_PRES_UNIT "mmHg"
#elif defined(UNITS_PRES_INCHESOFMERCURY)
    #define TEST_PRES(HP) ((HP) * 0.02953f)
    #define TEST_PRES_UNIT "inHg"
#elif defined(UNITS_PRES_POUNDSPERSQUAREINCH)
    #define TEST_PRES(HP) ((HP) * 0.014504f)
    #define TEST_PRES_UNIT "psi"
#else
    #define TEST_PRES(HP) (HP)
    #define TEST_PRES_UNIT "hPa"
#endif

// Distance expectations (base: meters)
#ifdef UNITS_DIST_MILES
    #define TEST_DIST_M(M) ((M) * 0.000621371f)  // m → miles
    #define TEST_DIST_UNIT "mi"
#else
    #define TEST_DIST_M(M) ((M) * 0.001f)  // m → km
    #define TEST_DIST_UNIT "km"
#endif

// Precipitation expectations (base: mm)
#ifdef UNITS_HOURLY_PRECIP_CENTIMETERS
    #define TEST_PRECIP(MM) ((MM) * 0.1f)
    #define TEST_PRECIP_UNIT "cm"
#elif defined(UNITS_HOURLY_PRECIP_INCHES)
    #define TEST_PRECIP(MM) ((MM) * 0.0393701f)
    #define TEST_PRECIP_UNIT "in"
#else
    #define TEST_PRECIP(MM) (MM)
    #define TEST_PRECIP_UNIT "mm"
#endif

// =============================================================================
// Scenario Expectations (OWM format: Kelvin, m/s, hPa, meters)
// =============================================================================
// Note: Values in Kelvin (273.15 + Celsius)

// Sunny: 295.15K (22°C), 45% humidity, 1.39 m/s (5 km/h), 1015 hPa, 10000m visibility
#define TEST_EXP_SUNNY_TEMP_K   295.15f
#define TEST_EXP_SUNNY_HUM      45
#define TEST_EXP_SUNNY_WIND_MS  1.39f  // 5 km/h = 1.39 m/s
#define TEST_EXP_SUNNY_PRES     1015.0f
#define TEST_EXP_SUNNY_DIST_M   10000  // 10 km = 10000 m

// Light rain: 291.15K (18°C), 75% humidity, 2.22 m/s (8 km/h), 1005 hPa, 5000m
#define TEST_EXP_RAIN_TEMP_K    291.15f
#define TEST_EXP_RAIN_HUM       75
#define TEST_EXP_RAIN_WIND_MS   2.22f  // 8 km/h = 2.22 m/s
#define TEST_EXP_RAIN_PRES      1005.0f
#define TEST_EXP_RAIN_DIST_M    5000

// Heavy storm: 289.15K (16°C), 90% humidity, 6.94 m/s (25 km/h), 995 hPa, 2000m
#define TEST_EXP_STORM_TEMP_K   289.15f
#define TEST_EXP_STORM_HUM      90
#define TEST_EXP_STORM_WIND_MS  6.94f  // 25 km/h = 6.94 m/s
#define TEST_EXP_STORM_PRES     995.0f
#define TEST_EXP_STORM_DIST_M   2000

// High precip prob: 293.15K (20°C), 70% humidity, 2.78 m/s (10 km/h), 1010 hPa, 8000m
#define TEST_EXP_HPOP_TEMP_K    293.15f
#define TEST_EXP_HPOP_HUM       70
#define TEST_EXP_HPOP_WIND_MS   2.78f  // 10 km/h = 2.78 m/s
#define TEST_EXP_HPOP_PRES      1010.0f
#define TEST_EXP_HPOP_DIST_M    8000

// Low precip prob: 294.15K (21°C), 50% humidity, 1.39 m/s (5 km/h), 1018 hPa, 10000m
#define TEST_EXP_LPOP_TEMP_K    294.15f
#define TEST_EXP_LPOP_HUM       50
#define TEST_EXP_LPOP_WIND_MS   1.39f
#define TEST_EXP_LPOP_PRES      1018.0f
#define TEST_EXP_LPOP_DIST_M    10000

// Snowy: 273.15K (0°C), 85% humidity, 2.78 m/s (10 km/h), 1020 hPa, 3000m
#define TEST_EXP_SNOW_TEMP_K    273.15f
#define TEST_EXP_SNOW_HUM       85
#define TEST_EXP_SNOW_WIND_MS   2.78f
#define TEST_EXP_SNOW_PRES      1020.0f
#define TEST_EXP_SNOW_DIST_M    3000

// Display values (converted for tests)
#define TEST_EXP_SUNNY_TEMP     TEST_TEMP_K(TEST_EXP_SUNNY_TEMP_K)
#define TEST_EXP_SUNNY_WIND     TEST_WIND_MS(TEST_EXP_SUNNY_WIND_MS)
#define TEST_EXP_SUNNY_DIST     TEST_DIST_M(TEST_EXP_SUNNY_DIST_M)

#define TEST_EXP_RAIN_TEMP      TEST_TEMP_K(TEST_EXP_RAIN_TEMP_K)
#define TEST_EXP_RAIN_WIND      TEST_WIND_MS(TEST_EXP_RAIN_WIND_MS)
#define TEST_EXP_RAIN_DIST      TEST_DIST_M(TEST_EXP_RAIN_DIST_M)

#define TEST_EXP_STORM_TEMP     TEST_TEMP_K(TEST_EXP_STORM_TEMP_K)
#define TEST_EXP_STORM_WIND     TEST_WIND_MS(TEST_EXP_STORM_WIND_MS)
#define TEST_EXP_STORM_DIST     TEST_DIST_M(TEST_EXP_STORM_DIST_M)

#define TEST_EXP_HPOP_TEMP      TEST_TEMP_K(TEST_EXP_HPOP_TEMP_K)
#define TEST_EXP_HPOP_WIND      TEST_WIND_MS(TEST_EXP_HPOP_WIND_MS)
#define TEST_EXP_HPOP_DIST      TEST_DIST_M(TEST_EXP_HPOP_DIST_M)

#define TEST_EXP_LPOP_TEMP      TEST_TEMP_K(TEST_EXP_LPOP_TEMP_K)
#define TEST_EXP_LPOP_WIND      TEST_WIND_MS(TEST_EXP_LPOP_WIND_MS)
#define TEST_EXP_LPOP_DIST      TEST_DIST_M(TEST_EXP_LPOP_DIST_M)

#define TEST_EXP_SNOW_TEMP      TEST_TEMP_K(TEST_EXP_SNOW_TEMP_K)
#define TEST_EXP_SNOW_WIND      TEST_WIND_MS(TEST_EXP_SNOW_WIND_MS)
#define TEST_EXP_SNOW_DIST      TEST_DIST_M(TEST_EXP_SNOW_DIST_M)

#include "api_response_compat.h"
#include "parse_openmeteo.h"
#include "render_mock.h"

using namespace blackbox;

// ============================================================================
// Configuration
// ============================================================================

#ifndef MOCK_SERVER_URL
    #define MOCK_SERVER_URL "http://localhost:8080"
#endif

// ============================================================================
// Test Orchestrator
// ============================================================================

/**
 * TestOrchestrator coordinates the blackbox test flow:
 * 1. Set scenario on mock server
 * 2. Fetch weather data (GET /v1/forecast)
 * 3. Parse JSON → owm_resp_onecall_t
 * 4. Render scene (RenderMock captures events)
 * 5. Submit events (HTTP POST /test/events)
 * 6. Validate (POST /test/validate or GET /test/report)
 */
class TestOrchestrator {
public:
    TestOrchestrator() : httpClient_(MOCK_SERVER_URL) {
        renderMock_.setHttpClient(&httpClient_);
    }
    
    // Step 1: Set scenario on mock server
    bool setScenario(const char* scenarioName) {
        std::string path = "/test/scenario/";
        path += scenarioName;
        
        std::string response = httpClient_.postWithResponse(path, "");
        if (response.empty() || response.find("\"status\":\"ok\"") == std::string::npos) {
            std::cerr << "Failed to set scenario: " << scenarioName << std::endl;
            std::cerr << "Response: " << response << std::endl;
            return false;
        }
        
        currentScenario_ = scenarioName;
        return true;
    }
    
    // Step 2: Fetch weather data
    std::string fetchWeatherData() {
        return httpClient_.get("/v1/forecast");
    }
    
    // Steps 3 & 4: Parse and Render
    void parseAndRender(const std::string& json) {
        // Parse JSON → owm_resp_onecall_t
        weatherData_ = parseOpenMeteo(json);
        
        // Render (captures events)
        renderMock_.clearEvents();
        renderMock_.renderWeatherScene(weatherData_);
    }
    
    // Step 5: Submit events to mock server
    bool submitEvents() {
        return renderMock_.submitToServer();
    }
    
    // Step 6: Validate with mock server
    bool validate() {
        std::string requestBody = "{\"scenario\":\"";
        requestBody += currentScenario_;
        requestBody += "\"}";
        
        return httpClient_.post("/test/validate", requestBody);
    }
    
    // Get validation report
    std::string getReport() {
        return httpClient_.get("/test/report");
    }
    
    // Check server health
    bool checkHealth() {
        std::string response = httpClient_.get("/health");
        return !response.empty() && response.find("\"status\":\"healthy\"") != std::string::npos;
    }
    
    // Accessors
    const owm_resp_onecall_t& getWeatherData() const { return weatherData_; }
    RenderMock& getRenderMock() { return renderMock_; }
    
private:
    SimpleHttpClient httpClient_;
    RenderMock renderMock_{&httpClient_};
    owm_resp_onecall_t weatherData_;
    std::string currentScenario_;
};

// Global orchestrator
static TestOrchestrator* g_orchestrator = nullptr;

// ============================================================================
// Unity Setup/Teardown
// ============================================================================

void setUp(void) {
    if (!g_orchestrator) {
        g_orchestrator = new TestOrchestrator();
    }
    g_orchestrator->setScenario("sunny");
}

void tearDown(void) {
    // Cleanup between tests
}

// ============================================================================
// Test Cases
// ============================================================================

/**
 * Test: Sunny weather
 * Expected: Sun icon drawn, NO umbrella
 */
void test_sunny(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        g_orchestrator->setScenario("sunny"),
        "Failed to set sunny scenario"
    );
    
    std::string json = g_orchestrator->fetchWeatherData();
    TEST_ASSERT_FALSE_MESSAGE(json.empty(), "Failed to fetch weather data");
    
    g_orchestrator->parseAndRender(json);
    
    // Verify parsed data (using owm_resp_onecall_t format)
    const owm_resp_onecall_t& data = g_orchestrator->getWeatherData();
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, getCurrentPop(data), 
        "Sunny scenario should have 0% POP");
    TEST_ASSERT_FALSE_MESSAGE(shouldShowUmbrella(data), 
        "Sunny scenario should NOT require umbrella");
    
    // Verify temperature (Kelvin), humidity, wind (m/s)
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(1.0f, TEST_EXP_SUNNY_TEMP_K, data.hourly[0].temp,
        "Sunny scenario should have warm temperature (~295K / 22°C)");
    TEST_ASSERT_INT_WITHIN_MESSAGE(5, TEST_EXP_SUNNY_HUM, data.hourly[0].humidity,
        "Sunny scenario should have low humidity (~45%)");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, TEST_EXP_SUNNY_WIND_MS, data.hourly[0].wind_speed,
        "Sunny scenario should have light winds (~1.4 m/s / 5 km/h)");
    
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->submitEvents(), 
        "Failed to submit render events");
    
    RenderMock& renderer = g_orchestrator->getRenderMock();
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasIconDrawn("sun"), 
        "Sun icon should be drawn for sunny weather");
    TEST_ASSERT_FALSE_MESSAGE(renderer.wasUmbrellaDrawn(), 
        "Umbrella should NOT be drawn for sunny weather");
    
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasTemperatureDrawn(TEST_EXP_SUNNY_TEMP, 2.0f),
        "Temperature should be rendered for sunny");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasHumidityDrawn(TEST_EXP_SUNNY_HUM, 5),
        "Humidity should be rendered for sunny");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasWindSpeedDrawn(TEST_EXP_SUNNY_WIND, 2.0f),
        "Wind speed should be rendered for sunny");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasPressureDrawn(TEST_PRES(TEST_EXP_SUNNY_PRES), 5.0f),
        "Pressure should be rendered for sunny");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasVisibilityDrawn(TEST_DIST_M(TEST_EXP_SUNNY_DIST_M), 0.5f),
        "Visibility should be rendered for sunny");
    
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->validate(), 
        "Server validation failed for sunny scenario");
}

/**
 * Test: Light rain
 */
void test_light_rain(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        g_orchestrator->setScenario("light_rain"),
        "Failed to set light_rain scenario"
    );
    
    std::string json = g_orchestrator->fetchWeatherData();
    TEST_ASSERT_FALSE_MESSAGE(json.empty(), "Failed to fetch weather data");
    
    g_orchestrator->parseAndRender(json);
    
    const owm_resp_onecall_t& data = g_orchestrator->getWeatherData();
    TEST_ASSERT_TRUE_MESSAGE(getCurrentPop(data) >= 30, 
        "Light rain should have POP >= 30%");
    TEST_ASSERT_TRUE_MESSAGE(getCurrentPrecipitation(data) > 0, 
        "Light rain should have precipitation > 0");
    TEST_ASSERT_TRUE_MESSAGE(shouldShowUmbrella(data), 
        "Light rain SHOULD require umbrella");
    
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(2.0f, TEST_EXP_RAIN_TEMP_K, data.hourly[0].temp,
        "Light rain should have mild temperature (~291K / 18°C)");
    TEST_ASSERT_INT_WITHIN_MESSAGE(5, TEST_EXP_RAIN_HUM, data.hourly[0].humidity,
        "Light rain should have high humidity (~75%)");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, TEST_EXP_RAIN_WIND_MS, data.hourly[0].wind_speed,
        "Light rain should have moderate winds (~2.2 m/s / 8 km/h)");
    
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->submitEvents(), 
        "Failed to submit render events");
    
    RenderMock& renderer = g_orchestrator->getRenderMock();
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasIconDrawn("rain"), 
        "Rain icon should be drawn");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasUmbrellaDrawn(), 
        "Umbrella SHOULD be drawn for light rain");
    
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasTemperatureDrawn(TEST_EXP_RAIN_TEMP, 2.0f),
        "Temperature should be rendered for light rain");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasHumidityDrawn(TEST_EXP_RAIN_HUM, 5),
        "Humidity should be rendered for light rain");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasWindSpeedDrawn(TEST_EXP_RAIN_WIND, 2.0f),
        "Wind speed should be rendered for light rain");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasPressureDrawn(TEST_PRES(TEST_EXP_RAIN_PRES), 5.0f),
        "Pressure should be rendered for light rain");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasVisibilityDrawn(TEST_DIST_M(TEST_EXP_RAIN_DIST_M), 0.5f),
        "Visibility should be rendered for light rain");
    
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->validate(), 
        "Server validation failed for light_rain scenario");
}

/**
 * Test: Heavy storm
 */
void test_heavy_storm(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        g_orchestrator->setScenario("heavy_storm"),
        "Failed to set heavy_storm scenario"
    );
    
    std::string json = g_orchestrator->fetchWeatherData();
    
    g_orchestrator->parseAndRender(json);
    
    const owm_resp_onecall_t& data = g_orchestrator->getWeatherData();
    TEST_ASSERT_TRUE_MESSAGE(getCurrentPop(data) >= 90, 
        "Heavy storm should have high POP");
    TEST_ASSERT_TRUE_MESSAGE(getCurrentPrecipitation(data) > 10.0f, 
        "Heavy storm should have high precipitation");
    
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(2.0f, TEST_EXP_STORM_TEMP_K, data.hourly[0].temp,
        "Heavy storm should have moderate temperature (~289K / 16°C)");
    TEST_ASSERT_INT_WITHIN_MESSAGE(5, TEST_EXP_STORM_HUM, data.hourly[0].humidity,
        "Heavy storm should have very high humidity (~90%)");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(1.0f, TEST_EXP_STORM_WIND_MS, data.hourly[0].wind_speed,
        "Heavy storm should have strong winds (~6.9 m/s / 25 km/h)");
    
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->submitEvents(), 
        "Failed to submit render events");
    
    RenderMock& renderer = g_orchestrator->getRenderMock();
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasIconDrawn("thunder"), 
        "Thunder icon should be drawn for storm");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasUmbrellaDrawn(), 
        "Umbrella SHOULD be drawn for heavy storm");
    
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasTemperatureDrawn(TEST_EXP_STORM_TEMP, 2.0f),
        "Temperature should be rendered for storm");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasHumidityDrawn(TEST_EXP_STORM_HUM, 5),
        "Humidity should be rendered for storm");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasWindSpeedDrawn(TEST_EXP_STORM_WIND, 10.0f),
        "High wind speed should be rendered for storm");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasPressureDrawn(TEST_PRES(TEST_EXP_STORM_PRES), 5.0f),
        "Pressure should be rendered for storm");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasVisibilityDrawn(TEST_DIST_M(TEST_EXP_STORM_DIST_M), 0.5f),
        "Visibility should be rendered for storm");
    
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->validate(), 
        "Server validation failed for heavy_storm scenario");
}

/**
 * Test: High precipitation probability (no current rain)
 */
void test_high_precipitation_prob(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        g_orchestrator->setScenario("high_precipitation_prob"),
        "Failed to set high_precipitation_prob scenario"
    );
    
    std::string json = g_orchestrator->fetchWeatherData();
    
    g_orchestrator->parseAndRender(json);
    
    const owm_resp_onecall_t& data = g_orchestrator->getWeatherData();
    TEST_ASSERT_TRUE_MESSAGE(getCurrentPop(data) >= 50, 
        "Scenario should have high POP (>=50%)");
    
    TEST_ASSERT_TRUE_MESSAGE(shouldShowUmbrella(data), 
        "Umbrella should be shown when POP >= 30% even without current rain");
    
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(2.0f, TEST_EXP_HPOP_TEMP_K, data.hourly[0].temp,
        "Should have mild temperature (~293K / 20°C)");
    TEST_ASSERT_INT_WITHIN_MESSAGE(10, TEST_EXP_HPOP_HUM, data.hourly[0].humidity,
        "Should have moderate humidity (~70%)");
    
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->submitEvents(), 
        "Failed to submit render events");
    
    RenderMock& renderer = g_orchestrator->getRenderMock();
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasUmbrellaDrawn(), 
        "Umbrella MUST be drawn when POP crosses threshold");
    
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasTemperatureDrawn(TEST_EXP_HPOP_TEMP, 2.0f),
        "Temperature should be rendered");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasHumidityDrawn(TEST_EXP_HPOP_HUM, 10),
        "Humidity should be rendered");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasPressureDrawn(TEST_PRES(TEST_EXP_HPOP_PRES), 5.0f),
        "Pressure should be rendered for high precip prob");
    
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->validate(), 
        "Server validation failed for high_precipitation_prob scenario");
}

/**
 * Test: No precipitation with low probability
 */
void test_no_precip_low_prob(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        g_orchestrator->setScenario("no_precip_low_prob"),
        "Failed to set no_precip_low_prob scenario"
    );
    
    std::string json = g_orchestrator->fetchWeatherData();
    
    g_orchestrator->parseAndRender(json);
    
    const owm_resp_onecall_t& data = g_orchestrator->getWeatherData();
    TEST_ASSERT_TRUE_MESSAGE(getCurrentPop(data) < 30, 
        "Scenario should have low POP (<30%)");
    TEST_ASSERT_FALSE_MESSAGE(shouldShowUmbrella(data), 
        "Low POP and no precip should NOT require umbrella");
    
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(2.0f, TEST_EXP_LPOP_TEMP_K, data.hourly[0].temp,
        "Cloudy scenario should have mild temperature (~294K / 21°C)");
    TEST_ASSERT_INT_WITHIN_MESSAGE(5, TEST_EXP_LPOP_HUM, data.hourly[0].humidity,
        "Cloudy scenario should have moderate humidity (~50%)");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, TEST_EXP_LPOP_WIND_MS, data.hourly[0].wind_speed,
        "Cloudy scenario should have light winds (~1.4 m/s / 5 km/h)");
    
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->submitEvents(), 
        "Failed to submit render events");
    
    RenderMock& renderer = g_orchestrator->getRenderMock();
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasIconDrawn("cloud") || renderer.wasIconDrawn("partly_cloudy"), 
        "Cloud icon should be drawn for cloudy weather");
    TEST_ASSERT_FALSE_MESSAGE(renderer.wasUmbrellaDrawn(), 
        "Umbrella should NOT be drawn for low probability");
    
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasTemperatureDrawn(TEST_EXP_LPOP_TEMP, 2.0f),
        "Temperature should be rendered for cloudy");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasHumidityDrawn(TEST_EXP_LPOP_HUM, 5),
        "Humidity should be rendered for cloudy");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasWindSpeedDrawn(TEST_EXP_LPOP_WIND, 2.0f),
        "Wind speed should be rendered for cloudy");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasPressureDrawn(TEST_PRES(TEST_EXP_LPOP_PRES), 5.0f),
        "Pressure should be rendered for cloudy");
    
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->validate(), 
        "Server validation failed for no_precip_low_prob scenario");
}

/**
 * Test: Snowy conditions
 */
void test_snowy(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        g_orchestrator->setScenario("snowy"),
        "Failed to set snowy scenario"
    );
    
    std::string json = g_orchestrator->fetchWeatherData();
    
    g_orchestrator->parseAndRender(json);
    
    const owm_resp_onecall_t& data = g_orchestrator->getWeatherData();
    TEST_ASSERT_TRUE_MESSAGE(shouldShowUmbrella(data), 
        "Snowy conditions SHOULD require umbrella (high POP + precip)");
    
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(2.0f, TEST_EXP_SNOW_TEMP_K, data.hourly[0].temp,
        "Snowy scenario should have freezing temperature (~273K / 0°C)");
    TEST_ASSERT_INT_WITHIN_MESSAGE(5, TEST_EXP_SNOW_HUM, data.hourly[0].humidity,
        "Snowy scenario should have high humidity (~85%)");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, TEST_EXP_SNOW_WIND_MS, data.hourly[0].wind_speed,
        "Snowy scenario should have moderate winds (~2.8 m/s / 10 km/h)");
    
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->submitEvents(), 
        "Failed to submit render events");
    
    RenderMock& renderer = g_orchestrator->getRenderMock();
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasIconDrawn("snow"), 
        "Snow icon should be drawn");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasUmbrellaDrawn(), 
        "Umbrella should be drawn for snow");
    
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasTemperatureDrawn(TEST_EXP_SNOW_TEMP, 2.0f),
        "Freezing temperature should be rendered for snow");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasHumidityDrawn(TEST_EXP_SNOW_HUM, 5),
        "High humidity should be rendered for snow");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasWindSpeedDrawn(TEST_EXP_SNOW_WIND, 3.0f),
        "Wind speed should be rendered for snow");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasVisibilityDrawn(TEST_DIST_M(TEST_EXP_SNOW_DIST_M), 0.5f),
        "Visibility should be rendered for snow");
    
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->validate(), 
        "Server validation failed for snowy scenario");
}

/**
 * Test: Report generation
 */
void test_report_generation(void) {
    g_orchestrator->setScenario("light_rain");
    std::string json = g_orchestrator->fetchWeatherData();
    g_orchestrator->parseAndRender(json);
    g_orchestrator->submitEvents();
    
    std::string report = g_orchestrator->getReport();
    TEST_ASSERT_FALSE_MESSAGE(report.empty(), "Report should not be empty");
    TEST_ASSERT_TRUE_MESSAGE(report.find("current_scenario") != std::string::npos, 
        "Report should contain current_scenario");
    TEST_ASSERT_TRUE_MESSAGE(report.find("captured_events") != std::string::npos, 
        "Report should contain captured_events");
}

/**
 * Test: Temperature accuracy across scenarios
 */
void test_temperature_accuracy(void) {
    float sunnyTempK, snowyTempK;
    
    // Test sunny - warm
    g_orchestrator->setScenario("sunny");
    std::string json = g_orchestrator->fetchWeatherData();
    g_orchestrator->parseAndRender(json);
    sunnyTempK = g_orchestrator->getWeatherData().hourly[0].temp;
    
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(1.0f, TEST_EXP_SUNNY_TEMP_K, sunnyTempK,
        "Sunny: Raw temperature should be ~295K (22°C)");
    
    // Rendered temperature check
    float drawnTemp = g_orchestrator->getRenderMock().getDrawnTemperature();
    TEST_ASSERT_FALSE_MESSAGE(std::isnan(drawnTemp),
        "Sunny: Temperature should be rendered");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(2.0f, TEST_EXP_SUNNY_TEMP, drawnTemp,
        "Sunny: Rendered temperature should match configured units");
    
    // Test snowy - freezing
    g_orchestrator->setScenario("snowy");
    json = g_orchestrator->fetchWeatherData();
    g_orchestrator->parseAndRender(json);
    snowyTempK = g_orchestrator->getWeatherData().hourly[0].temp;
    
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(1.0f, TEST_EXP_SNOW_TEMP_K, snowyTempK,
        "Snowy: Raw temperature should be ~273K (0°C)");
    
    // Verify temperature range difference
    float tempDiffK = sunnyTempK - snowyTempK;
    TEST_ASSERT_TRUE_MESSAGE(tempDiffK > 20.0f,
        "Temperature difference between sunny and snowy should be >20K");
}

/**
 * Test: Humidity levels across scenarios
 */
void test_humidity_levels(void) {
    int sunnyHumidity, stormHumidity;
    
    // Sunny - low humidity
    g_orchestrator->setScenario("sunny");
    std::string json = g_orchestrator->fetchWeatherData();
    g_orchestrator->parseAndRender(json);
    sunnyHumidity = g_orchestrator->getWeatherData().hourly[0].humidity;
    
    TEST_ASSERT_TRUE_MESSAGE(sunnyHumidity < 60,
        "Sunny: Humidity should be <60%");
    
    // Storm - high humidity
    g_orchestrator->setScenario("heavy_storm");
    json = g_orchestrator->fetchWeatherData();
    g_orchestrator->parseAndRender(json);
    stormHumidity = g_orchestrator->getWeatherData().hourly[0].humidity;
    
    TEST_ASSERT_TRUE_MESSAGE(stormHumidity > 80,
        "Storm: Humidity should be >80%");
    
    // Verify humidity difference
    int humidityDiff = stormHumidity - sunnyHumidity;
    TEST_ASSERT_TRUE_MESSAGE(humidityDiff > 30,
        "Humidity difference between storm and sunny should be >30%");
}

/**
 * Test: Wind speed variations across scenarios
 */
void test_wind_speed_variations(void) {
    float sunnyWindMs, stormWindMs;
    
    // Sunny - calm
    g_orchestrator->setScenario("sunny");
    std::string json = g_orchestrator->fetchWeatherData();
    g_orchestrator->parseAndRender(json);
    sunnyWindMs = g_orchestrator->getWeatherData().hourly[0].wind_speed;
    
    TEST_ASSERT_TRUE_MESSAGE(sunnyWindMs < 3.0f,
        "Sunny: Wind speed should be <3 m/s (calm)");
    
    // Storm - strong winds
    g_orchestrator->setScenario("heavy_storm");
    json = g_orchestrator->fetchWeatherData();
    g_orchestrator->parseAndRender(json);
    stormWindMs = g_orchestrator->getWeatherData().hourly[0].wind_speed;
    
    TEST_ASSERT_TRUE_MESSAGE(stormWindMs > 5.0f,
        "Storm: Wind speed should be >5 m/s (strong)");
    
    // Verify significant wind difference
    float windDiff = stormWindMs - sunnyWindMs;
    TEST_ASSERT_TRUE_MESSAGE(windDiff > 3.0f,
        "Wind speed difference between storm and sunny should be >3 m/s");
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    std::cout << "========================================" << std::endl;
    std::cout << "ESP32 Weather EPD Blackbox Tests" << std::endl;
    std::cout << "Mock Server: " << MOCK_SERVER_URL << std::endl;
    std::cout << "Using owm_resp_onecall_t (firmware structure)" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Initialize orchestrator
    g_orchestrator = new TestOrchestrator();
    
    // Wait for mock server
    int retries = 30;
    while (retries-- > 0) {
        if (g_orchestrator->checkHealth()) {
            std::cout << "✓ Mock server is ready!" << std::endl;
            break;
        }
        std::cout << "Waiting for mock server... (" << retries << " retries left)" << std::endl;
#ifdef _WIN32
        Sleep(1000);
#else
        usleep(1000000);
#endif
    }
    
    if (retries <= 0) {
        std::cerr << "✗ Mock server not responding at " << MOCK_SERVER_URL << std::endl;
        delete g_orchestrator;
        return 1;
    }
    
    std::cout << std::endl;
    
    // Run Unity tests
    UNITY_BEGIN();
    
    std::cout << "\n--- Running Sunny Test ---" << std::endl;
    RUN_TEST(test_sunny);
    
    std::cout << "\n--- Running Light Rain Test ---" << std::endl;
    RUN_TEST(test_light_rain);
    
    std::cout << "\n--- Running Heavy Storm Test ---" << std::endl;
    RUN_TEST(test_heavy_storm);
    
    std::cout << "\n--- Running High Precipitation Probability Test ---" << std::endl;
    RUN_TEST(test_high_precipitation_prob);
    
    std::cout << "\n--- Running No Precipitation Low Probability Test ---" << std::endl;
    RUN_TEST(test_no_precip_low_prob);
    
    std::cout << "\n--- Running Snowy Test ---" << std::endl;
    RUN_TEST(test_snowy);
    
    std::cout << "\n--- Running Report Generation Test ---" << std::endl;
    RUN_TEST(test_report_generation);
    
    std::cout << "\n--- Running Temperature Accuracy Test ---" << std::endl;
    RUN_TEST(test_temperature_accuracy);
    
    std::cout << "\n--- Running Humidity Levels Test ---" << std::endl;
    RUN_TEST(test_humidity_levels);
    
    std::cout << "\n--- Running Wind Speed Variations Test ---" << std::endl;
    RUN_TEST(test_wind_speed_variations);
    
    int result = UNITY_END();
    
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    if (result == 0) {
        std::cout << "✓ All tests passed!" << std::endl;
    } else {
        std::cout << "✗ " << result << " test(s) failed" << std::endl;
    }
    std::cout << "========================================" << std::endl;
    
    delete g_orchestrator;
    g_orchestrator = nullptr;
    
    return result;
}
