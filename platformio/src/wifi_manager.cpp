/// @file wifi_manager.cpp
/// @brief WiFi connection management and configuration portal implementation
/// @copyright Copyright (C) 2026 Mauro Freitas
/// @license GNU General Public License v3.0
///
/// @details
/// Implements non-blocking WiFi state machine with:
/// - Automatic connection with RTC RAM persisted credentials
/// - Captive portal AP mode for initial configuration
/// - IP-based geolocation for auto-location detection
/// - Failure tracking with configurable retry limits
/// - DNS server for captive portal detection
///
/// State transitions logged to serial at 115200 baud.

#include "wifi_manager.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "web_ui_data.h"
#include "display_utils.h"
#include "failure_handler.h"
#include "state_decision.h"
#include "config.h"
#include "_locale.h"
#include "icons/icons_196x196.h"

/// @brief Current firmware state machine state
FirmwareState currentState = STATE_BOOT;

/// @brief WiFi manager configuration
DeviceConfig wifiConfig = {
    .wifiConnectTimeout = 20,  ///< WiFi connection attempt timeout (seconds)
    .configTimeout = 300       ///< AP mode timeout (seconds)
};

/// @brief Runtime state tracking
RuntimeState runtime = {
    .apMode = false,
    .wifiConnected = false,
    .portalActive = false,
    .wifiStartTime = 0,
    .portalStartTime = 0
};

#if USE_MOCKUP_DATA
// Mock mode: deterministic WiFi simulation timer
static uint32_t mockWifiStartTime = 0;
#endif

/// @defgroup rtc_ram_vars RTC RAM Persistent Variables
/// @brief Variables preserved across deep sleep (lost on power loss)
/// @details Buffer sizes follow: Character Limit + 1 for null-terminator
RTC_DATA_ATTR char ramSSID[33] = "";       // Wi-Fi SSID: 32 chars + \0 (optimized from 64)
RTC_DATA_ATTR char ramPassword[64] = "";   // WPA2 password: 63 chars + \0
RTC_DATA_ATTR char ramCity[64] = "";       // City name: 63 chars + \0
RTC_DATA_ATTR char ramCountry[64] = "";    // Country: 63 chars + \0
RTC_DATA_ATTR char ramLat[21] = "";        // Latitude: 20 chars + \0 (optimized from 32)
RTC_DATA_ATTR char ramLon[21] = "";        // Longitude: 20 chars + \0 (optimized from 32)
RTC_DATA_ATTR char ramTimezone[64] = "UTC0"; // Timezone: 63 chars + \0
RTC_DATA_ATTR bool ramAutoGeo = false;
RTC_DATA_ATTR uint8_t ramTimezoneMode = TIMEZONE_MODE_AUTO;  // Default: use API timezone
RTC_DATA_ATTR bool rtcInitialized = false;
RTC_DATA_ATTR bool isFirstBoot = true;
// RTC RAM failure counters (persist during deep sleep)
RTC_DATA_ATTR uint8_t connectionFailCycles = 0;  // WiFi connection failure cycles
RTC_DATA_ATTR uint8_t ntpFailCycles = 0;         // NTP sync failure cycles  
RTC_DATA_ATTR uint8_t apiFailCycles = 0;         // API request failure cycles
RTC_DATA_ATTR bool isErrorState = false;         // Permanent error state flag

AsyncWebServer server(80);
DNSServer dnsServer;

/// @brief Handle captive portal detection requests
/// @param request HTTP request
/// @return true if request was handled (redirected to portal)
/// @details Implements captive portal detection for iOS, Android, Windows, macOS
bool handleCaptivePortal(AsyncWebServerRequest *request) {
    String host = request->host();
    if (host != "192.168.4.1" && host != "weather.local") {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html_gz, index_html_gz_len);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
        return true;
    }
    return false;
}

/// @brief Start Access Point configuration mode
/// @details Creates "weather_eink-AP" network and starts DNS server
/// for captive portal detection. Serves configuration page on port 80.
void startAP() {
    Serial.println("Starting Access Point Mode...");
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP("weather_eink-AP", NULL, 6);
    
    dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    
    // Main setup page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html_gz, index_html_gz_len);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "public, max-age=31536000");
        request->send(response);
    });

    // Captive portal detection endpoints
    auto captiveHandler = [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html_gz, index_html_gz_len);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    };

    server.on("/generate_204", HTTP_GET, captiveHandler);
    server.on("/gen_204", HTTP_GET, captiveHandler);
    server.on("/hotspot-detect.html", HTTP_GET, captiveHandler);
    server.on("/connecttest.txt", HTTP_GET, captiveHandler);
    server.on("/redirect", HTTP_GET, captiveHandler);
    server.on("/fwlink", HTTP_GET, captiveHandler);
    server.on("/check_network_status.txt", HTTP_GET, captiveHandler);

    // Save configuration with full validation
    server.on("/save", HTTP_POST, handleConfigSave);

    server.onNotFound([](AsyncWebServerRequest *request) {
        if (!handleCaptivePortal(request)) {
            request->send(404, "text/plain", "Not found");
        }
    });

    server.begin();
    MDNS.begin("weather");
    
    runtime.apMode = true;
    runtime.portalActive = true;
    runtime.portalStartTime = millis();
    
    drawAPModeScreen("weather_eink-AP", wifiConfig.configTimeout / 60);
    setFirmwareState(STATE_AP_CONFIG_MODE);
}

/// @brief Initialize WiFi manager
/// @details Sets hostname for network identification
void wifiManagerSetup() {
    WiFi.setHostname("weather-eink");
}

/// @brief Execute WiFi state machine iteration
/// @details Non-blocking state machine. Call repeatedly from loop().
void wifiManagerLoop() {
    // 1. GATHER INPUT (Impure environment sensing)
    DecisionInput input = {};
    input.hasCredentials = (strlen(ramSSID) > 0);
    input.isFirstBoot = isFirstBoot;
    
    // Config limits from config.h or NVS
    input.maxWifiFail = MAX_WIFI_FAIL_CYCLES;
    input.maxNtpFail = MAX_NTP_FAIL_CYCLES;
    input.maxApiFail = MAX_API_FAIL_CYCLES;
    
    // Status counters from RTC memory
    input.wifiFailCycles = connectionFailCycles;
    input.ntpFailCycles = 0; // Handled in main.cpp for now
    input.apiFailCycles = 0; // Handled in main.cpp for now

#if USE_MOCKUP_DATA
    // MOCK MODE: Simulate WiFi feedback
    input.wifiConnected = (millis() - mockWifiStartTime > 2000);
    input.wifiTimeout = false; 
    input.portalTimeout = false;
    input.configSaved = (!runtime.portalActive && input.hasCredentials);
#else
    // PRODUCTION MODE: Real hardware feedback
    input.wifiConnected = (WiFi.status() == WL_CONNECTED);
    input.wifiTimeout = (millis() - runtime.wifiStartTime > wifiConfig.wifiConnectTimeout * 1000);
    input.portalTimeout = (millis() - runtime.portalStartTime > 300000); // 5 min timeout
    input.configSaved = (!runtime.portalActive && input.hasCredentials);
#endif

    // 2. DECIDE (Pure logic)
    DecisionOutput output = decideTransition(currentState, input);

    // 3. EXECUTE SIDE EFFECTS (Impure hardware/UI calls)
    if (output.nextState != currentState) {
        
        // Actions on ENTERING a new state
        switch (output.nextState) {
            case STATE_WIFI_CONNECTING:
#if USE_MOCKUP_DATA
                mockWifiStartTime = millis();
#else
                if (currentState == STATE_BOOT) {
                    WiFi.begin(ramSSID, ramPassword);
                    runtime.wifiStartTime = millis();
                }
#endif
                if (isFirstBoot || !SILENT_STATUS) {
                    const char* ssidToShow = (strlen(ramSSID) > 0) ? ramSSID : "MockNetwork";
                    drawLoading(wifi_196x196, "Connecting to Wi-Fi...", ssidToShow);
                }
                break;

            case STATE_NORMAL_MODE:
                runtime.wifiConnected = true;
                // Clear first boot flag immediately on successful connection
                isFirstBoot = false;
                // Show status on first boot or if not silent
                if (isFirstBoot || !SILENT_STATUS) {
                    updateEinkStatus("Wi-Fi Connected!");
                }

                break;

            case STATE_AP_CONFIG_MODE:
                startAP();
                break;

            case STATE_ERROR:
                if (output.setErrorFlag) {
                    // Logic from original handleFailure / startAP timeout
                    if (currentState == STATE_WIFI_CONNECTING) {
                        String detail = "Attempt " + String(connectionFailCycles + 1) + "/" + String(MAX_WIFI_FAIL_CYCLES);
                        handleFailure(FAILURE_WIFI, TXT_WIFI_CONNECTION_FAILED, detail);
                    } else {
                        handleFailure(FAILURE_BATTERY, "System Error", "Timeout");
                    }
                }
                break;

            case STATE_SLEEP_PENDING:
                // If it was a WiFi timeout, we might want to log it
                if (currentState == STATE_WIFI_CONNECTING && !isFirstBoot) {
                    Serial.printf("Connection fail cycle #%d/%d. Retrying later...\n", 
                                  connectionFailCycles + 1, MAX_WIFI_FAIL_CYCLES);
                }
                break;
                
            default: break;
        }

        // Apply state variables
        if (output.resetWifiFail) { connectionFailCycles = 0; }
        if (output.incWifiFail) { connectionFailCycles++; }
        
        setFirmwareState(output.nextState);
    }

    // 4. RUN PERIODIC TASKS (State-specific polling)
    if (currentState == STATE_AP_CONFIG_MODE) {
        dnsServer.processNextRequest();
    }
}

/// @brief Transition to new firmware state
/// @param newState Target state
/// @details Logs transition to serial for debugging
void setFirmwareState(FirmwareState newState) {
    if (currentState != newState) {
        Serial.printf("State Transition: %d -> %d\n", currentState, newState);
        currentState = newState;
    }
}

/// @brief Perform IP-based geolocation
/// @return true if location successfully determined
/// @details Uses ip-api.com to auto-detect city/coordinates.
/// Results stored in ramCity, ramLat, ramLon, ramCountry.
/// @warning Requires active WiFi connection
bool locateByIpAddress() {
    if (WiFi.status() != WL_CONNECTED) return false;
    
    Serial.println("Performing automatic IP geolocation...");
    
    HTTPClient http;
    http.begin(GEOLOCATION_ENDPOINT);
    int httpCode = http.GET();
    
    bool success = false;
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error && doc["status"] == "success") {
            strncpy(ramCity, doc["city"] | "", sizeof(ramCity) - 1);
            ramCity[sizeof(ramCity) - 1] = '\0';
            strncpy(ramLat, doc["lat"].as<String>().c_str(), sizeof(ramLat) - 1);
            ramLat[sizeof(ramLat) - 1] = '\0';
            strncpy(ramLon, doc["lon"].as<String>().c_str(), sizeof(ramLon) - 1);
            ramLon[sizeof(ramLon) - 1] = '\0';
            strncpy(ramCountry, doc["country"] | "", sizeof(ramCountry) - 1);
            ramCountry[sizeof(ramCountry) - 1] = '\0';
            Serial.printf("Successfully geolocated to: %s, %s (%s, %s)\n", ramCity, ramCountry, ramLat, ramLon);
            success = true;
        } else {
            Serial.println("Geolocation API returned error status.");
        }
    } else {
        Serial.printf("Geolocation HTTP Request failed: %d\n", httpCode);
    }
    
    http.end();
    return success;
}

