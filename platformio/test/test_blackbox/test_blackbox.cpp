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
#ifndef _WIN32
    #include <unistd.h>  // For usleep
#endif

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
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasTemperatureDrawn(22.0f, 2.0f),
        "Temperature should be rendered (~22°C for sunny)");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasHumidityDrawn(45, 5),
        "Humidity should be rendered (~45% for sunny)");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasWindSpeedDrawn(5.0f, 2.0f),
        "Wind speed should be rendered (~5 km/h for sunny)");
    
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
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasTemperatureDrawn(18.0f, 2.0f),
        "Temperature should be rendered (~18°C for light rain)");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasHumidityDrawn(75, 5),
        "Humidity should be rendered (~75% for light rain)");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasWindSpeedDrawn(8.0f, 2.0f),
        "Wind speed should be rendered (~8 km/h for light rain)");
    
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
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasTemperatureDrawn(16.0f, 2.0f),
        "Temperature should be rendered (~16°C for storm)");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasHumidityDrawn(90, 5),
        "Humidity should be rendered (~90% for storm)");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasWindSpeedDrawn(25.0f, 10.0f),
        "High wind speed should be rendered (~25+ km/h for storm)");
    
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
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasTemperatureDrawn(20.0f, 2.0f),
        "Temperature should be rendered (~20°C)");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasHumidityDrawn(70, 10),
        "Humidity should be rendered (~70%)");
    
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
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasTemperatureDrawn(21.0f, 2.0f),
        "Temperature should be rendered (~21°C for cloudy)");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasHumidityDrawn(50, 5),
        "Humidity should be rendered (~50% for cloudy)");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasWindSpeedDrawn(5.0f, 2.0f),
        "Wind speed should be rendered (~5 km/h for cloudy)");
    
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
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasTemperatureDrawn(0.0f, 2.0f),
        "Freezing temperature should be rendered (~0°C for snow)");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasHumidityDrawn(85, 5),
        "High humidity should be rendered (~85% for snow)");
    TEST_ASSERT_TRUE_MESSAGE(renderer.wasWindSpeedDrawn(10.0f, 3.0f),
        "Wind speed should be rendered (~10 km/h for snow)");
    
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
 */
void test_temperature_accuracy(void) {
    float sunnyTemp, snowyTemp;
    
    // Test sunny - warm
    g_orchestrator->setScenario("sunny");
    std::string json = g_orchestrator->fetchWeatherData();
    g_orchestrator->parseAndRender(json);
    sunnyTemp = g_orchestrator->getWeatherData().hourly.temperature_2m[0];
    
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, 22.0f, sunnyTemp,
        "Sunny: Temperature should be ~22°C");
    TEST_ASSERT_TRUE_MESSAGE(!std::isnan(g_orchestrator->getRenderMock().getDrawnTemperature()),
        "Sunny: Temperature should be rendered");
    
    // Test snowy - freezing (store value before changing scenario)
    g_orchestrator->setScenario("snowy");
    json = g_orchestrator->fetchWeatherData();
    g_orchestrator->parseAndRender(json);
    snowyTemp = g_orchestrator->getWeatherData().hourly.temperature_2m[0];
    
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(1.0f, 0.0f, snowyTemp,
        "Snowy: Temperature should be ~0°C");
    
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
 */
void test_wind_speed_variations(void) {
    float sunnyWind, stormWind;
    
    // Sunny - calm
    g_orchestrator->setScenario("sunny");
    std::string json = g_orchestrator->fetchWeatherData();
    g_orchestrator->parseAndRender(json);
    sunnyWind = g_orchestrator->getWeatherData().hourly.wind_speed_10m[0];
    
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
