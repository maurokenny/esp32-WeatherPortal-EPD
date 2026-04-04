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
#include "config.h"
#include "_locale.h"
#include "icons/icons_196x196.h"

// Global instances
FirmwareState currentState = STATE_BOOT;
DeviceConfig wifiConfig = {
    .wifiConnectTimeout = 20,
    .configTimeout = 300
    // .configTimeout = 60
};
RuntimeState runtime = {
    .apMode = false,
    .wifiConnected = false,
    .portalActive = false,
    .wifiStartTime = 0,
    .portalStartTime = 0
};

// RTC RAM Variables for connection and location (persist during deep sleep)
// Buffer sizes follow: Character Limit + 1 for null-terminator
// Total RTC RAM saved: 43 bytes (from 386 to 343 bytes for these buffers)
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

// WLED style captive portal detection helper
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

void wifiManagerSetup() {
    WiFi.setHostname("weather-eink");
}

void wifiManagerLoop() {
    switch (currentState) {
        case STATE_BOOT:
            if (strlen(ramSSID) > 0) {
                WiFi.begin(ramSSID, ramPassword);
                runtime.wifiStartTime = millis();
                setFirmwareState(STATE_WIFI_CONNECTING);
                if (isFirstBoot || !SILENT_STATUS) {
                    drawLoading(wifi_196x196, "Connecting to Wi-Fi...", ramSSID);
                }
            } else {
                startAP();
            }
            break;

        case STATE_WIFI_CONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                // SUCCESS: WiFi connected
                runtime.wifiConnected = true;
                // Reset failure counter on successful connection
                connectionFailCycles = 0;
                // Show status message (before changing isFirstBoot to preserve behavior)
                if (isFirstBoot || !SILENT_STATUS) {
                    updateEinkStatus("Wi-Fi Connected!");
                }
                // Mark first boot as complete as soon as WiFi connects
                // This ensures that even if NTP/API fails later, we won't
                // enter AP mode on subsequent connection failures
                if (isFirstBoot) {
                    isFirstBoot = false;
                }
                setFirmwareState(STATE_NORMAL_MODE);
            } else if (millis() - runtime.wifiStartTime > wifiConfig.wifiConnectTimeout * 1000) {
                // TIMEOUT: WiFi connection failed
                Serial.println("Wi-Fi connection timed out.");
                WiFi.disconnect();
                
                // Only count failure cycles after first successful boot
                if (!isFirstBoot) {
                    connectionFailCycles++;
                    Serial.printf("Connection fail cycle #%d\n", connectionFailCycles);
                }

                // Determine next state based on connection history
                if (isFirstBoot) {
                    // Never connected before: allow configuration via AP mode
                    Serial.println("First boot - starting AP Configuration Mode.");
                    startAP();
                } else {
                    // Has connected before: count failures and use handler
                    // Only count failure cycles after first successful boot
                    connectionFailCycles++;
                    Serial.printf("Connection fail cycle #%d/%d\n", connectionFailCycles, MAX_WIFI_FAIL_CYCLES);
                    
                    String detail = "Attempt " + String(connectionFailCycles) + "/" + String(MAX_WIFI_FAIL_CYCLES);
                    handleFailure(FAILURE_WIFI, TXT_WIFI_CONNECTION_FAILED, detail);
                }
            }
            break;

        case STATE_AP_CONFIG_MODE:
            dnsServer.processNextRequest();
            
            // Check for switch trigger if portalActive was set to false by /save
            if (!runtime.portalActive && strlen(ramSSID) > 0) {
                static uint32_t switchDelay = 0;
                if (switchDelay == 0) switchDelay = millis();
                if (millis() - switchDelay > 3000) {
                    Serial.println("Applying settings and connecting...");
                    server.end();
                    dnsServer.stop();
                    WiFi.softAPdisconnect(true); // Clean up AP
                    setFirmwareState(STATE_BOOT);
                    switchDelay = 0; // Reset
                }
            }

             // Security timeout (only active if portal is waiting for input)
             if (runtime.portalActive && (millis() - runtime.portalStartTime > wifiConfig.configTimeout * 1000)) {
                 Serial.println("AP Configuration timeout. No config received. Entering ERROR_CONNECTION.");
                 server.end();
                 dnsServer.stop();
                 WiFi.softAPdisconnect(true);
                 drawErrorScreen("Setup Timed Out", 
                                "No configuration received within timeout",
                                "Please check WiFi credentials and try again");
                 isErrorState = true;
                 setFirmwareState(STATE_ERROR);
             }
            break;

        case STATE_NORMAL_MODE:
            // Handled in main loop for weather updates
            break;

        case STATE_SLEEP_PENDING:
            // Transition handled in main.cpp for deep sleep
            break;

        case STATE_ERROR:
            // Error state: handled in main.cpp for indefinite deep sleep
            // This state prevents any automatic retries
            break;
    }
}

void setFirmwareState(FirmwareState newState) {
    if (currentState != newState) {
        Serial.printf("State Transition: %d -> %d\n", currentState, newState);
        currentState = newState;
    }
}

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

