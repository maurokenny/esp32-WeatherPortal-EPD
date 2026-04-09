/* Blackbox Integration Tests - Main Test Suite
 * Copyright (C) 2025
 *
 * Architecture Flow:
 *   Test Blackbox (Unity) 
 *        ↓
 *   Parse JSON (parseOpenMeteo)
 *        ↓
 *   Render (Mock) → HTTP POST → Mock Server
 *        ↓
 *   Validate (Pattern Matching)
 *
 * Tests:
 * - test_sunny: Sunny weather shows sun icon, no umbrella, warm temp, low humidity
 * - test_light_rain: Rain shows rain icon, umbrella, mild temp, high humidity
 * - test_heavy_storm: Storm shows thunder icon, umbrella, strong winds
 * - test_high_precipitation_prob: High POP shows umbrella even without rain
 * - test_no_precip_low_prob: Low POP shows no umbrella, mild conditions
 * - test_snowy: Snow shows snow icon, umbrella, freezing temps
 * - test_temperature_ranges: Validates temperature display accuracy
 * - test_humidity_levels: Validates humidity display across scenarios
 * - test_wind_speeds: Validates wind speed display across scenarios
 */

#include <unity.h>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cmath>
#ifndef _WIN32
    #include <unistd.h>  // For usleep
#endif

// =============================================================================
// Unit-Aware Test Expectations
// =============================================================================
// These macros convert base values (Celsius, km/h, hPa) to the configured units

// Temperature expectations (base: Celsius)
#ifdef UNITS_TEMP_FAHRENHEIT
    #define TEST_TEMP(BASE) ((BASE) * 9.0f / 5.0f + 32.0f)
    #define TEST_TEMP_UNIT "F"
#elif defined(UNITS_TEMP_KELVIN)
    #define TEST_TEMP(BASE) ((BASE) + 273.15f)
    #define TEST_TEMP_UNIT "K"
#else
    #define TEST_TEMP(BASE) (BASE)
    #define TEST_TEMP_UNIT "C"
#endif

// Wind speed expectations (base: km/h)
#ifdef UNITS_SPEED_MILESPERHOUR
    #define TEST_WIND(MS) ((MS) / 1.60934f)
    #define TEST_WIND_UNIT "mph"
#else
    #define TEST_WIND(MS) (MS)
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

// Distance expectations (base: km)
#ifdef UNITS_DIST_MILES
    #define TEST_DIST(KM) ((KM) * 0.621371f)
    #define TEST_DIST_UNIT "mi"
#else
    #define TEST_DIST(KM) (KM)
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
// Scenario Expectations (with unit conversion)
// =============================================================================
// Sunny: 22°C, 45% humidity, 5 km/h wind, 1015 hPa, 10 km visibility
#define TEST_EXP_SUNNY_TEMP     TEST_TEMP(22.0f)
#define TEST_EXP_SUNNY_HUM      45
#define TEST_EXP_SUNNY_WIND     TEST_WIND(5.0f)
#define TEST_EXP_SUNNY_PRES     TEST_PRES(1015.0f)
#define TEST_EXP_SUNNY_DIST     TEST_DIST(10.0f)

// Light rain: 18°C, 75% humidity, 8 km/h wind, 1005 hPa, 5 km visibility
#define TEST_EXP_RAIN_TEMP      TEST_TEMP(18.0f)
#define TEST_EXP_RAIN_HUM       75
#define TEST_EXP_RAIN_WIND      TEST_WIND(8.0f)
#define TEST_EXP_RAIN_PRES      TEST_PRES(1005.0f)
#define TEST_EXP_RAIN_DIST      TEST_DIST(5.0f)

// Heavy storm: 16°C, 90% humidity, 25 km/h wind, 995 hPa, 2 km visibility
#define TEST_EXP_STORM_TEMP     TEST_TEMP(16.0f)
#define TEST_EXP_STORM_HUM      90
#define TEST_EXP_STORM_WIND     TEST_WIND(25.0f)
#define TEST_EXP_STORM_PRES     TEST_PRES(995.0f)
#define TEST_EXP_STORM_DIST     TEST_DIST(2.0f)

// High precip prob: 20°C, 70% humidity, 10 km/h wind, 1010 hPa, 8 km visibility
#define TEST_EXP_HPOP_TEMP      TEST_TEMP(20.0f)
#define TEST_EXP_HPOP_HUM       70
#define TEST_EXP_HPOP_WIND      TEST_WIND(10.0f)
#define TEST_EXP_HPOP_PRES      TEST_PRES(1010.0f)

// Low precip prob: 21°C, 50% humidity, 5 km/h wind, 1018 hPa, 10 km visibility
#define TEST_EXP_LPOP_TEMP      TEST_TEMP(21.0f)
#define TEST_EXP_LPOP_HUM       50
#define TEST_EXP_LPOP_WIND      TEST_WIND(5.0f)
#define TEST_EXP_LPOP_PRES      TEST_PRES(1018.0f)

// Snowy: 0°C, 85% humidity, 10 km/h wind, 1020 hPa, 3 km visibility
#define TEST_EXP_SNOW_TEMP      TEST_TEMP(0.0f)
#define TEST_EXP_SNOW_HUM       85
#define TEST_EXP_SNOW_WIND      TEST_WIND(10.0f)
#define TEST_EXP_SNOW_PRES      TEST_PRES(1020.0f)
#define TEST_EXP_SNOW_DIST      TEST_DIST(3.0f)

#include "api_response.h"
#include "parse_openmeteo.h"
#include "render_mock.h"

using namespace blackbox;

// ============================================================================
// Configuration
// ============================================================================

// Default mock server URL (can be overridden via build_flags if needed)
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
 * 3. Parse JSON (parseOpenMeteo)
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
        // Parse JSON
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
        // Build validation request
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
    const OpenMeteoResponse& getWeatherData() const { return weatherData_; }
    RenderMock& getRenderMock() { return renderMock_; }
    
private:
    SimpleHttpClient httpClient_;
    RenderMock renderMock_{&httpClient_};
    OpenMeteoResponse weatherData_;
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
    // Reset server state before each test
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
    // Step 1: Set scenario
    TEST_ASSERT_TRUE_MESSAGE(
        g_orchestrator->setScenario("sunny"),
        "Failed to set sunny scenario"
    );
    
    // Step 2: Fetch weather data
    std::string json = g_orchestrator->fetchWeatherData();
    TEST_ASSERT_FALSE_MESSAGE(json.empty(), "Failed to fetch weather data");
    
    // Steps 3 & 4: Parse and Render
    g_orchestrator->parseAndRender(json);
    
    // Verify parsed data
    const OpenMeteoResponse& data = g_orchestrator->getWeatherData();
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, data.getCurrentPop(), 
        "Sunny scenario should have 0% POP");
    TEST_ASSERT_FALSE_MESSAGE(data.shouldShowUmbrella(), 
        "Sunny scenario should NOT require umbrella");
    
    // Verify temperature, humidity and wind for sunny scenario
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(1.0f, 22.0f, data.hourly.temperature_2m[0],
        "Sunny scenario should have warm temperature (~22°C)");
    TEST_ASSERT_INT_WITHIN_MESSAGE(5, 45, data.hourly.relative_humidity_2m[0],
        "Sunny scenario should have low humidity (~45%)");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(1.0f, 5.0f, data.hourly.wind_speed_10m[0],
        "Sunny scenario should have light winds (~5 km/h)");
    
    // Step 5: Submit events
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->submitEvents(), 
        "Failed to submit render events");
    
    // Verify render mock captured correct events
    RenderMock& renderer = g_orchestrator->getRenderMock();
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasIconDrawn("sun"), 
        "Sun icon should be drawn for sunny weather");
    TEST_ASSERT_FALSE_MESSAGE(renderer.wasUmbrellaDrawn(), 
        "Umbrella should NOT be drawn for sunny weather");
    
    // Verify weather values were rendered
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasTemperatureDrawn(TEST_EXP_SUNNY_TEMP, 2.0f),
        "Temperature should be rendered for sunny");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasHumidityDrawn(TEST_EXP_SUNNY_HUM, 5),
        "Humidity should be rendered for sunny");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasWindSpeedDrawn(TEST_EXP_SUNNY_WIND, 2.0f),
        "Wind speed should be rendered for sunny");
    
    // Step 6: Validate with server
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->validate(), 
        "Server validation failed for sunny scenario");
}

/**
 * Test: Light rain
 * Expected: Rain icon drawn, umbrella drawn
 */
void test_light_rain(void) {
    // Step 1: Set scenario
    TEST_ASSERT_TRUE_MESSAGE(
        g_orchestrator->setScenario("light_rain"),
        "Failed to set light_rain scenario"
    );
    
    // Step 2: Fetch weather data
    std::string json = g_orchestrator->fetchWeatherData();
    TEST_ASSERT_FALSE_MESSAGE(json.empty(), "Failed to fetch weather data");
    
    // Steps 3 & 4: Parse and Render
    g_orchestrator->parseAndRender(json);
    
    // Verify parsed data
    const OpenMeteoResponse& data = g_orchestrator->getWeatherData();
    TEST_ASSERT_TRUE_MESSAGE(data.getCurrentPop() >= 30, 
        "Light rain should have POP >= 30%");
    TEST_ASSERT_TRUE_MESSAGE(data.getCurrentPrecipitation() > 0, 
        "Light rain should have precipitation > 0");
    TEST_ASSERT_TRUE_MESSAGE(data.shouldShowUmbrella(), 
        "Light rain SHOULD require umbrella");
    
    // Verify temperature, humidity for rainy scenario
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(2.0f, 18.0f, data.hourly.temperature_2m[0],
        "Light rain should have mild temperature (~18°C)");
    TEST_ASSERT_INT_WITHIN_MESSAGE(5, 75, data.hourly.relative_humidity_2m[0],
        "Light rain should have high humidity (~75%)");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(2.0f, 8.0f, data.hourly.wind_speed_10m[0],
        "Light rain should have moderate winds (~8 km/h)");
    
    // Step 5: Submit events
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->submitEvents(), 
        "Failed to submit render events");
    
    // Verify render mock
    RenderMock& renderer = g_orchestrator->getRenderMock();
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasIconDrawn("rain"), 
        "Rain icon should be drawn");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasUmbrellaDrawn(), 
        "Umbrella SHOULD be drawn for light rain");
    
    // Verify weather values were rendered
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasTemperatureDrawn(TEST_EXP_RAIN_TEMP, 2.0f),
        "Temperature should be rendered for light rain");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasHumidityDrawn(TEST_EXP_RAIN_HUM, 5),
        "Humidity should be rendered for light rain");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasWindSpeedDrawn(TEST_EXP_RAIN_WIND, 2.0f),
        "Wind speed should be rendered for light rain");
    
    // Step 6: Validate with server
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->validate(), 
        "Server validation failed for light_rain scenario");
}

/**
 * Test: Heavy storm
 * Expected: Thunder icon drawn, umbrella drawn, high precipitation detected
 */
void test_heavy_storm(void) {
    // Step 1: Set scenario
    TEST_ASSERT_TRUE_MESSAGE(
        g_orchestrator->setScenario("heavy_storm"),
        "Failed to set heavy_storm scenario"
    );
    
    // Step 2: Fetch weather data
    std::string json = g_orchestrator->fetchWeatherData();
    
    // Steps 3 & 4: Parse and Render
    g_orchestrator->parseAndRender(json);
    
    // Verify parsed data
    const OpenMeteoResponse& data = g_orchestrator->getWeatherData();
    TEST_ASSERT_TRUE_MESSAGE(data.getCurrentPop() >= 90, 
        "Heavy storm should have high POP");
    TEST_ASSERT_TRUE_MESSAGE(data.getCurrentPrecipitation() > 10.0f, 
        "Heavy storm should have high precipitation");
    
    // Verify storm conditions: high humidity, strong winds
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(2.0f, 16.0f, data.hourly.temperature_2m[0],
        "Heavy storm should have moderate temperature (~16°C)");
    TEST_ASSERT_INT_WITHIN_MESSAGE(5, 90, data.hourly.relative_humidity_2m[0],
        "Heavy storm should have very high humidity (~90%)");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(5.0f, 25.0f, data.hourly.wind_speed_10m[0],
        "Heavy storm should have strong winds (~25+ km/h)");
    
    // Step 5: Submit events
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->submitEvents(), 
        "Failed to submit render events");
    
    // Verify render mock
    RenderMock& renderer = g_orchestrator->getRenderMock();
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasIconDrawn("thunder"), 
        "Thunder icon should be drawn for storm");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasUmbrellaDrawn(), 
        "Umbrella SHOULD be drawn for heavy storm");
    
    // Verify weather values - especially high wind speed
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasTemperatureDrawn(TEST_EXP_STORM_TEMP, 2.0f),
        "Temperature should be rendered for storm");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasHumidityDrawn(TEST_EXP_STORM_HUM, 5),
        "Humidity should be rendered for storm");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasWindSpeedDrawn(TEST_EXP_STORM_WIND, 10.0f),
        "High wind speed should be rendered for storm");
    
    // Step 6: Validate with server
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->validate(), 
        "Server validation failed for heavy_storm scenario");
}

/**
 * Test: High precipitation probability (no current rain)
 * Expected: Umbrella drawn because POP > 50%
 */
void test_high_precipitation_prob(void) {
    // Step 1: Set scenario
    TEST_ASSERT_TRUE_MESSAGE(
        g_orchestrator->setScenario("high_precipitation_prob"),
        "Failed to set high_precipitation_prob scenario"
    );
    
    // Step 2: Fetch weather data
    std::string json = g_orchestrator->fetchWeatherData();
    
    // Steps 3 & 4: Parse and Render
    g_orchestrator->parseAndRender(json);
    
    // Verify parsed data - this is the key test!
    const OpenMeteoResponse& data = g_orchestrator->getWeatherData();
    TEST_ASSERT_TRUE_MESSAGE(data.getCurrentPop() >= 50, 
        "Scenario should have high POP (>=50%)");
    
    // Important: Even with 0 current precipitation, umbrella should show
    // because POP threshold is crossed
    TEST_ASSERT_TRUE_MESSAGE(data.shouldShowUmbrella(), 
        "Umbrella should be shown when POP >= 30% even without current rain");
    
    // Verify mild conditions despite high POP
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(2.0f, 20.0f, data.hourly.temperature_2m[0],
        "Should have mild temperature (~20°C)");
    TEST_ASSERT_INT_WITHIN_MESSAGE(10, 70, data.hourly.relative_humidity_2m[0],
        "Should have moderate humidity (~70%)");
    
    // Step 5: Submit events
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->submitEvents(), 
        "Failed to submit render events");
    
    // Verify render mock - umbrella MUST be drawn
    RenderMock& renderer = g_orchestrator->getRenderMock();
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasUmbrellaDrawn(), 
        "Umbrella MUST be drawn when POP crosses threshold");
    
    // Verify weather values
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasTemperatureDrawn(TEST_EXP_HPOP_TEMP, 2.0f),
        "Temperature should be rendered");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasHumidityDrawn(TEST_EXP_HPOP_HUM, 10),
        "Humidity should be rendered");
    
    // Step 6: Validate with server
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->validate(), 
        "Server validation failed for high_precipitation_prob scenario");
}

/**
 * Test: No precipitation with low probability
 * Expected: Cloud icon drawn, NO umbrella
 */
void test_no_precip_low_prob(void) {
    // Step 1: Set scenario
    TEST_ASSERT_TRUE_MESSAGE(
        g_orchestrator->setScenario("no_precip_low_prob"),
        "Failed to set no_precip_low_prob scenario"
    );
    
    // Step 2: Fetch weather data
    std::string json = g_orchestrator->fetchWeatherData();
    
    // Steps 3 & 4: Parse and Render
    g_orchestrator->parseAndRender(json);
    
    // Verify parsed data
    const OpenMeteoResponse& data = g_orchestrator->getWeatherData();
    TEST_ASSERT_TRUE_MESSAGE(data.getCurrentPop() < 30, 
        "Scenario should have low POP (<30%)");
    TEST_ASSERT_FALSE_MESSAGE(data.shouldShowUmbrella(), 
        "Low POP and no precip should NOT require umbrella");
    
    // Verify mild cloudy conditions
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(2.0f, 21.0f, data.hourly.temperature_2m[0],
        "Cloudy scenario should have mild temperature (~21°C)");
    TEST_ASSERT_INT_WITHIN_MESSAGE(5, 50, data.hourly.relative_humidity_2m[0],
        "Cloudy scenario should have moderate humidity (~50%)");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(2.0f, 5.0f, data.hourly.wind_speed_10m[0],
        "Cloudy scenario should have light winds (~5 km/h)");
    
    // Step 5: Submit events
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->submitEvents(), 
        "Failed to submit render events");
    
    // Verify render mock
    RenderMock& renderer = g_orchestrator->getRenderMock();
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasIconDrawn("cloud") || renderer.wasIconDrawn("partly_cloudy"), 
        "Cloud icon should be drawn for cloudy weather");
    TEST_ASSERT_FALSE_MESSAGE(renderer.wasUmbrellaDrawn(), 
        "Umbrella should NOT be drawn for low probability");
    
    // Verify weather values
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasTemperatureDrawn(TEST_EXP_LPOP_TEMP, 2.0f),
        "Temperature should be rendered for cloudy");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasHumidityDrawn(TEST_EXP_LPOP_HUM, 5),
        "Humidity should be rendered for cloudy");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasWindSpeedDrawn(TEST_EXP_LPOP_WIND, 2.0f),
        "Wind speed should be rendered for cloudy");
    
    // Step 6: Validate with server
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->validate(), 
        "Server validation failed for no_precip_low_prob scenario");
}

/**
 * Test: Snowy conditions
 * Expected: Snow icon drawn, umbrella drawn
 */
void test_snowy(void) {
    // Step 1: Set scenario
    TEST_ASSERT_TRUE_MESSAGE(
        g_orchestrator->setScenario("snowy"),
        "Failed to set snowy scenario"
    );
    
    // Step 2: Fetch weather data
    std::string json = g_orchestrator->fetchWeatherData();
    
    // Steps 3 & 4: Parse and Render
    g_orchestrator->parseAndRender(json);
    
    // Verify parsed data
    const OpenMeteoResponse& data = g_orchestrator->getWeatherData();
    TEST_ASSERT_TRUE_MESSAGE(data.shouldShowUmbrella(), 
        "Snowy conditions SHOULD require umbrella (high POP + precip)");
    
    // Verify freezing conditions
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(2.0f, 0.0f, data.hourly.temperature_2m[0],
        "Snowy scenario should have freezing temperature (~0°C)");
    TEST_ASSERT_INT_WITHIN_MESSAGE(5, 85, data.hourly.relative_humidity_2m[0],
        "Snowy scenario should have high humidity (~85%)");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(3.0f, 10.0f, data.hourly.wind_speed_10m[0],
        "Snowy scenario should have moderate winds (~10 km/h)");
    
    // Verify snowfall is present
    TEST_ASSERT_TRUE_MESSAGE(data.hourly.getMaxPrecipitation() > 0,
        "Snowy scenario should have precipitation (snowfall)");
    
    // Step 5: Submit events
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->submitEvents(), 
        "Failed to submit render events");
    
    // Verify render mock
    RenderMock& renderer = g_orchestrator->getRenderMock();
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasIconDrawn("snow"), 
        "Snow icon should be drawn");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasUmbrellaDrawn(), 
        "Umbrella should be drawn for snow");
    
    // Verify freezing temperature rendered
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasTemperatureDrawn(TEST_EXP_SNOW_TEMP, 2.0f),
        "Freezing temperature should be rendered for snow");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasHumidityDrawn(TEST_EXP_SNOW_HUM, 5),
        "High humidity should be rendered for snow");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasWindSpeedDrawn(TEST_EXP_SNOW_WIND, 3.0f),
        "Wind speed should be rendered for snow");
    
    // Step 6: Validate with server
    TEST_ASSERT_TRUE_MESSAGE(g_orchestrator->validate(), 
        "Server validation failed for snowy scenario");
}

/**
 * Test: Report generation
 * Validates that the mock server generates proper reports
 */
void test_report_generation(void) {
    // Set a scenario and capture some events
    g_orchestrator->setScenario("light_rain");
    std::string json = g_orchestrator->fetchWeatherData();
    g_orchestrator->parseAndRender(json);
    g_orchestrator->submitEvents();
    
    // Get report
    std::string report = g_orchestrator->getReport();
    TEST_ASSERT_FALSE_MESSAGE(report.empty(), "Report should not be empty");
    TEST_ASSERT_TRUE_MESSAGE(report.find("current_scenario") != std::string::npos, 
        "Report should contain current_scenario");
    TEST_ASSERT_TRUE_MESSAGE(report.find("captured_events") != std::string::npos, 
        "Report should contain captured_events");
}

/**
 * Test: Temperature accuracy across scenarios
 * Validates that temperature values are correctly parsed and rendered
 * Handles both Celsius and Fahrenheit based on UNITS_* defines
 */
void test_temperature_accuracy(void) {
    float sunnyTemp, snowyTemp;
    
    // Test sunny - warm
    g_orchestrator->setScenario("sunny");
    std::string json = g_orchestrator->fetchWeatherData();
    g_orchestrator->parseAndRender(json);
    sunnyTemp = g_orchestrator->getWeatherData().hourly.temperature_2m[0];
    
    // Compare raw value (API returns Celsius) with expected Celsius
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, 22.0f, sunnyTemp,
        "Sunny: Raw temperature should be ~22°C");
    
    // Compare rendered value with expected in display units
    float drawnTemp = g_orchestrator->getRenderMock().getDrawnTemperature();
    TEST_ASSERT_FALSE_MESSAGE(std::isnan(drawnTemp),
        "Sunny: Temperature should be rendered");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(1.0f, TEST_EXP_SUNNY_TEMP, drawnTemp,
        "Sunny: Rendered temperature should match configured units");
    
    // Test snowy - freezing (store value before changing scenario)
    g_orchestrator->setScenario("snowy");
    json = g_orchestrator->fetchWeatherData();
    g_orchestrator->parseAndRender(json);
    snowyTemp = g_orchestrator->getWeatherData().hourly.temperature_2m[0];
    
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(1.0f, 0.0f, snowyTemp,
        "Snowy: Raw temperature should be ~0°C");
    
    // Verify temperature range difference (using stored values)
    float tempDiff = sunnyTemp - snowyTemp;
    TEST_ASSERT_TRUE_MESSAGE(tempDiff > 15.0f,
        "Temperature difference between sunny and snowy should be >15°C");
}

/**
 * Test: Humidity levels across scenarios
 * Validates that humidity values are correctly parsed and rendered
 */
void test_humidity_levels(void) {
    int sunnyHumidity, stormHumidity;
    
    // Sunny - low humidity
    g_orchestrator->setScenario("sunny");
    std::string json = g_orchestrator->fetchWeatherData();
    g_orchestrator->parseAndRender(json);
    sunnyHumidity = g_orchestrator->getWeatherData().hourly.relative_humidity_2m[0];
    
    TEST_ASSERT_TRUE_MESSAGE(sunnyHumidity < 60,
        "Sunny: Humidity should be <60%");
    
    // Storm - high humidity (store value before changing scenario)
    g_orchestrator->setScenario("heavy_storm");
    json = g_orchestrator->fetchWeatherData();
    g_orchestrator->parseAndRender(json);
    stormHumidity = g_orchestrator->getWeatherData().hourly.relative_humidity_2m[0];
    
    TEST_ASSERT_TRUE_MESSAGE(stormHumidity > 80,
        "Storm: Humidity should be >80%");
    
    // Verify humidity difference (using stored values)
    int humidityDiff = stormHumidity - sunnyHumidity;
    TEST_ASSERT_TRUE_MESSAGE(humidityDiff > 30,
        "Humidity difference between storm and sunny should be >30%");
}

/**
 * Test: Wind speed variations across scenarios
 * Validates that wind speed values are correctly parsed and rendered
 * Handles both km/h and mph based on UNITS_* defines
 */
void test_wind_speed_variations(void) {
    float sunnyWind, stormWind;
    
    // Sunny - calm
    g_orchestrator->setScenario("sunny");
    std::string json = g_orchestrator->fetchWeatherData();
    g_orchestrator->parseAndRender(json);
    sunnyWind = g_orchestrator->getWeatherData().hourly.wind_speed_10m[0];
    
    // Raw values (API returns km/h) - test threshold in original units
    TEST_ASSERT_TRUE_MESSAGE(sunnyWind < 10.0f,
        "Sunny: Wind speed should be <10 km/h (calm)");
    
    // Storm - strong winds (store value before changing scenario)
    g_orchestrator->setScenario("heavy_storm");
    json = g_orchestrator->fetchWeatherData();
    g_orchestrator->parseAndRender(json);
    stormWind = g_orchestrator->getWeatherData().hourly.wind_speed_10m[0];
    
    TEST_ASSERT_TRUE_MESSAGE(stormWind > 20.0f,
        "Storm: Wind speed should be >20 km/h (strong)");
    
    // Verify significant wind difference (using stored values)
    float windDiff = stormWind - sunnyWind;
    TEST_ASSERT_TRUE_MESSAGE(windDiff > 10.0f,
        "Wind speed difference between storm and sunny should be >10 km/h");
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
        std::cerr << "Make sure the mock server is running:" << std::endl;
        std::cerr << "  cd test/mock_server && docker run -p 8080:8080 weather-epd-mock" << std::endl;
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
