/// @file wifi_manager.cpp
/// @brief WiFi connection management and configuration portal implementation
/// @copyright Copyright (C) 2026 Mauro Freitas
/// @license GNU General Public License v3.0
///
/// @details
/// Implements non-blocking WiFi state machine with:
/// - NVS persisted credentials and location settings
/// - Captive portal AP mode for explicit provisioning only
/// - Configuration button support (GPIO0) to force AP mode after boot
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
FirmwareState currentState = STATE_CHECK_CONFIG;

/// @brief WiFi manager configuration
DeviceConfig wifiConfig = {
    .wifiConnectTimeout = 60,  ///< WiFi connection attempt timeout (seconds)
    .configTimeout = 300     ///< AP mode timeout (seconds) - 5 minutes
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
/// @details Failure counters only. Configuration is persisted in NVS.
// RTC RAM failure counters (persist during deep sleep)
RTC_DATA_ATTR uint8_t connectionFailCycles = 0;  // WiFi connection failure cycles
RTC_DATA_ATTR uint8_t ntpFailCycles = 0;         // NTP sync failure cycles  
RTC_DATA_ATTR uint8_t apiFailCycles = 0;         // API request failure cycles
RTC_DATA_ATTR bool isErrorState = false;         // Permanent error state flag

/// @defgroup rtc_legacy_vars Legacy RTC RAM Configuration Variables
/// @brief Kept only for one-time migration to NVS.
/// @{
RTC_DATA_ATTR char ramSSID[33] = "";
RTC_DATA_ATTR char ramPassword[64] = "";
RTC_DATA_ATTR char ramCity[64] = "";
RTC_DATA_ATTR char ramCountry[64] = "";
RTC_DATA_ATTR char ramLat[21] = "";
RTC_DATA_ATTR char ramLon[21] = "";
RTC_DATA_ATTR char ramTimezone[64] = "UTC0";
RTC_DATA_ATTR bool ramAutoGeo = false;
RTC_DATA_ATTR uint8_t ramTimezoneMode = TIMEZONE_MODE_AUTO;
RTC_DATA_ATTR bool rtcInitialized = false;
/// @}

/// @brief Global configuration store instance
ConfigStore configStore;

/// @brief Configuration button state sampled once during setup
static bool g_apButtonPressed = false;

// ═══════════════════════════════════════════════════════════════════════════
// CONFIGSTORE IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════════════════

ConfigStore::ConfigStore()
    : autoGeo_(false), timezoneMode_(TIMEZONE_MODE_AUTO), provisioned_(false) {
    ssid_[0] = '\0';
    password_[0] = '\0';
    lat_[0] = '\0';
    lon_[0] = '\0';
    city_[0] = '\0';
    country_[0] = '\0';
    timezone_[0] = '\0';
}

bool ConfigStore::begin(bool readOnly) {
    return prefs_.begin(STORE_NAMESPACE, readOnly);
}

void ConfigStore::end() {
    prefs_.end();
}

bool ConfigStore::loadFromNVS() {
    if (!begin(true)) {
        return false;
    }

    String s = prefs_.getString(KEY_SSID, "");
    safeCopy(s.c_str(), ssid_, sizeof(ssid_));

    s = prefs_.getString(KEY_PASSWORD, "");
    safeCopy(s.c_str(), password_, sizeof(password_));

    s = prefs_.getString(KEY_LAT, "");
    safeCopy(s.c_str(), lat_, sizeof(lat_));

    s = prefs_.getString(KEY_LON, "");
    safeCopy(s.c_str(), lon_, sizeof(lon_));

    s = prefs_.getString(KEY_CITY, "");
    safeCopy(s.c_str(), city_, sizeof(city_));

    s = prefs_.getString(KEY_COUNTRY, "");
    safeCopy(s.c_str(), country_, sizeof(country_));

    s = prefs_.getString(KEY_TZ, "");
    safeCopy(s.c_str(), timezone_, sizeof(timezone_));

    autoGeo_ = prefs_.getBool(KEY_AUTO_GEO, false);
    timezoneMode_ = prefs_.getUChar(KEY_TZ_MODE, TIMEZONE_MODE_AUTO);
    provisioned_ = prefs_.getBool(KEY_PROVISIONED, false);

    end();
    return true;
}

bool ConfigStore::saveToNVS() {
    if (!begin(false)) {
        return false;
    }

    bool ok = true;
    ok &= prefs_.putString(KEY_SSID, ssid_) > 0;
    ok &= prefs_.putString(KEY_PASSWORD, password_) > 0;
    ok &= prefs_.putString(KEY_LAT, lat_) > 0;
    ok &= prefs_.putString(KEY_LON, lon_) > 0;
    ok &= prefs_.putString(KEY_CITY, city_) > 0;
    ok &= prefs_.putString(KEY_COUNTRY, country_) > 0;
    ok &= prefs_.putString(KEY_TZ, timezone_) > 0;
    ok &= prefs_.putBool(KEY_AUTO_GEO, autoGeo_) > 0;
    ok &= prefs_.putUChar(KEY_TZ_MODE, timezoneMode_) > 0;
    ok &= prefs_.putBool(KEY_PROVISIONED, provisioned_) > 0;

    end();
    if (!ok) {
        Serial.println("[ERROR] ConfigStore::saveToNVS() failed to write one or more keys.");
    }
    return ok;
}

void ConfigStore::clear() {
    if (begin(false)) {
        prefs_.clear();
        end();
    }
    ssid_[0] = '\0';
    password_[0] = '\0';
    lat_[0] = '\0';
    lon_[0] = '\0';
    city_[0] = '\0';
    country_[0] = '\0';
    timezone_[0] = '\0';
    autoGeo_ = false;
    timezoneMode_ = TIMEZONE_MODE_AUTO;
    provisioned_ = false;
}

bool ConfigStore::hasValidWifiConfig() const {
    return (ssid_[0] != '\0');
}

bool ConfigStore::hasCompleteLocation() const {
    return (lat_[0] != '\0' && lon_[0] != '\0' &&
            city_[0] != '\0' && country_[0] != '\0' &&
            timezone_[0] != '\0');
}

bool ConfigStore::migrateFromRtcIfNeeded() {
    if (strlen(ramSSID) == 0) {
        return false;
    }

    Serial.println("[MIGRATION] RTC config found, copying to NVS...");

    safeCopy(ramSSID, ssid_, sizeof(ssid_));
    safeCopy(ramPassword, password_, sizeof(password_));
    safeCopy(ramLat, lat_, sizeof(lat_));
    safeCopy(ramLon, lon_, sizeof(lon_));
    safeCopy(ramCity, city_, sizeof(city_));
    safeCopy(ramCountry, country_, sizeof(country_));
    safeCopy(ramTimezone, timezone_, sizeof(timezone_));
    autoGeo_ = ramAutoGeo;
    timezoneMode_ = ramTimezoneMode;

    bool ok = saveToNVS();
    if (ok) {
        Serial.println("[MIGRATION] RTC config copied to NVS successfully.");
    } else {
        Serial.println("[MIGRATION] Failed to save RTC config to NVS.");
    }
    return ok;
}

void ConfigStore::setWifiConfig(const char* ssid, const char* password) {
    safeCopy(ssid, ssid_, sizeof(ssid_));
    safeCopy(password, password_, sizeof(password_));
}

void ConfigStore::setLocation(const char* lat, const char* lon,
                              const char* city, const char* country,
                              const char* tz) {
    safeCopy(lat, lat_, sizeof(lat_));
    safeCopy(lon, lon_, sizeof(lon_));
    safeCopy(city, city_, sizeof(city_));
    safeCopy(country, country_, sizeof(country_));
    safeCopy(tz, timezone_, sizeof(timezone_));
}

void ConfigStore::setTimezoneMode(uint8_t mode) {
    timezoneMode_ = mode;
}

void ConfigStore::setAutoGeo(bool enabled) {
    autoGeo_ = enabled;
}

// ═══════════════════════════════════════════════════════════════════════════
// CONFIGURATION BUTTON
// ═══════════════════════════════════════════════════════════════════════════

bool readConfigButton() {
    pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);
    delay(500);  // Wait for boot to stabilize before sampling GPIO0

    Serial.printf("[BUTTON] Hold GPIO0 for %ums within the next %ums to enter AP setup mode.\n",
                  AP_MODE_HOLD_MS, BUTTON_READ_WINDOW_MS);

    unsigned long windowStart = millis();
    unsigned long pressStart = 0;

    while (millis() - windowStart < BUTTON_READ_WINDOW_MS) {
        if (digitalRead(CONFIG_BUTTON_PIN) == LOW) {
            if (pressStart == 0) {
                pressStart = millis();
            }
            if (millis() - pressStart >= AP_MODE_HOLD_MS) {
                Serial.println("[BUTTON] AP mode requested.");
                return true;
            }
        } else {
            pressStart = 0;  // released early, reset
        }
        delay(50);
    }

    Serial.println("[BOOT] No button press detected. Booting normally.");
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// FACTORY BOOTSTRAP FROM .ENV
// ═══════════════════════════════════════════════════════════════════════════

bool bootstrapFromEnv() {
#if !ALLOW_ENV_BOOTSTRAP_TO_NVS
    Serial.println("[BOOTSTRAP] .env bootstrap disabled by config.");
    return false;
#endif

    if (WIFI_SSID == nullptr || strlen(WIFI_SSID) == 0) {
        Serial.println("[BOOTSTRAP] .env has no valid WiFi config.");
        return false;
    }

    Serial.println("[BOOTSTRAP] Loading factory defaults from .env into NVS...");

    configStore.setWifiConfig(WIFI_SSID, WIFI_PASSWORD);
    configStore.setLocation(LAT.c_str(), LON.c_str(),
                            CITY_STRING.c_str(), COUNTRY_STRING.c_str(),
                            TIMEZONE);
    configStore.setAutoGeo(false);
    configStore.setTimezoneMode(TIMEZONE_MODE_AUTO);

    if (!configStore.saveToNVS()) {
        Serial.println("[BOOTSTRAP] Failed to write .env defaults to NVS.");
        return false;
    }

    Serial.println("[BOOTSTRAP] Factory defaults saved to NVS.");
    return true;
}

AsyncWebServer server(80);
DNSServer dnsServer;

/// @brief Handle captive portal detection requests
/// @param request HTTP request
/// @return true if request was handled (redirected to portal)
/// @details Implements captive portal detection for iOS, Android, Windows, macOS
bool handleCaptivePortal(AsyncWebServerRequest *request) {
    String host = request->host();
    if (host != AP_IP_ADDRESS && host != AP_URL_LOCAL) {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html_gz, index_html_gz_len);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
        return true;
    }
    return false;
}

/// @brief Stop Access Point configuration mode
/// @details Stops DNS server, web server, and disconnects softAP.
///          Resets WiFi mode to STA for clean connection attempts.
void stopAP() {
    if (runtime.apMode || runtime.portalActive) {
        Serial.println("[AP] Stopping access point and DNS server...");
        dnsServer.stop();
        server.end();
        MDNS.end();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        runtime.apMode = false;
        runtime.portalActive = false;
        Serial.println("[AP] Stopped.");
    }
}

/// @brief Start Access Point configuration mode
/// @details Creates AP network and starts DNS server
/// for captive portal detection. Serves configuration page on port 80.
void startAP() {
    Serial.println("Starting Access Point Mode...");
    WiFi.mode(WIFI_AP);
    IPAddress apIp, apGateway, apMask;
    apIp.fromString(AP_IP_ADDRESS);
    apGateway.fromString(AP_IP_ADDRESS);
    apMask.fromString("255.255.255.0");
    WiFi.softAPConfig(apIp, apGateway, apMask);
    WiFi.softAP(AP_SSID, NULL, 6);
    
    dnsServer.start(53, "*", apIp);
    
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
    
    drawAPModeScreen(AP_SSID, wifiConfig.configTimeout / 60);
    setFirmwareState(STATE_AP_CONFIG_MODE);
}

/// @brief Initialize WiFi manager
/// @details Sets hostname, loads configuration from NVS, migrates legacy RTC
///          config if present, and reads the configuration button.
void wifiManagerSetup() {
    WiFi.setHostname("weather-eink");

    if (!configStore.loadFromNVS()) {
        Serial.println("[WiFi] NVS load failed, will attempt bootstrap or AP mode.");
    }

    if (!configStore.hasValidWifiConfig()) {
        Serial.println("[WiFi] No valid NVS config found. Attempting RTC migration...");
        configStore.migrateFromRtcIfNeeded();
    }

    g_apButtonPressed = readConfigButton();
}

/// @brief Execute WiFi state machine iteration
/// @details Non-blocking state machine. Call repeatedly from loop().
void wifiManagerLoop() {
    // 1. GATHER INPUT (Impure environment sensing)
    DecisionInput input = {};
    input.nvsValid = configStore.hasValidWifiConfig();
    input.apButtonPressed = g_apButtonPressed;

    // Config limits from config.h
    input.maxWifiFail = MAX_WIFI_FAIL_CYCLES;
    input.maxNtpFail = MAX_NTP_FAIL_CYCLES;
    input.maxApiFail = MAX_API_FAIL_CYCLES;

    // Status counters from RTC memory
    input.wifiFailCycles = connectionFailCycles;
    input.ntpFailCycles = ntpFailCycles;
    input.apiFailCycles = apiFailCycles;

#if USE_MOCKUP_DATA
    // MOCK MODE: Simulate WiFi feedback
    input.wifiConnected = (millis() - mockWifiStartTime > 2000);
    input.wifiTimeout = false;
    input.portalTimeout = false;
    input.configSaved = (!runtime.portalActive && input.nvsValid);
#else
    // PRODUCTION MODE: Real hardware feedback
    input.wifiConnected = (WiFi.status() == WL_CONNECTED);
    input.wifiTimeout = (millis() - runtime.wifiStartTime > wifiConfig.wifiConnectTimeout * 1000);
    input.portalTimeout = (millis() - runtime.portalStartTime > wifiConfig.configTimeout * 1000);
    input.configSaved = (!runtime.portalActive && input.nvsValid);
#endif

    // 2. DECIDE (Pure logic)
    DecisionOutput output = decideTransition(currentState, input);

    // 3. EXECUTE SIDE EFFECTS (Impure hardware/UI calls)
    if (output.nextState != currentState) {

        Serial.printf("[STATE] %d->%d | nvsValid=%d apButton=%d configSaved=%d portalTimeout=%d wifiConnected=%d wifiTimeout=%d | portalActive=%d apMode=%d | portalElapsed=%lu\n",
                      currentState, output.nextState,
                      input.nvsValid, input.apButtonPressed, input.configSaved, input.portalTimeout,
                      input.wifiConnected, input.wifiTimeout,
                      runtime.portalActive, runtime.apMode,
                      (millis() - runtime.portalStartTime) / 1000);

        // Apply side effects from decision output
        if (output.resetWifiFail) { connectionFailCycles = 0; }
        if (output.incWifiFail) { connectionFailCycles++; }

        // Stop AP mode when transitioning out of it
        if (currentState == STATE_AP_CONFIG_MODE) {
            stopAP();
        }

        // Actions on ENTERING a new state
        switch (output.nextState) {
            case STATE_WIFI_CONNECTING:
#if USE_MOCKUP_DATA
                mockWifiStartTime = millis();
#else
                if (currentState == STATE_CHECK_CONFIG || currentState == STATE_BOOTSTRAP) {
                    WiFi.mode(WIFI_STA);
                    Serial.printf("[WIFI] Connecting to SSID: \"%s\", timeout: %us\n",
                                  configStore.ssid(), (unsigned int)wifiConfig.wifiConnectTimeout);
                    WiFi.begin(configStore.ssid(), configStore.password());
                    runtime.wifiStartTime = millis();
                }
#endif
                if (!configStore.provisioned() || !SILENT_STATUS) {
                    const char* ssidToShow = input.nvsValid ? configStore.ssid() : "MockNetwork";
                    drawLoading(wifi_196x196, "Connecting to Wi-Fi...", ssidToShow);
                }

                break;

            case STATE_NORMAL_MODE:
                runtime.wifiConnected = true;
                if (!configStore.provisioned() || !SILENT_STATUS) {
                    updateEinkStatus("Wi-Fi Connected!");
                }
                break;

            case STATE_AP_CONFIG_MODE:
                g_apButtonPressed = false;
                startAP();
                break;

            case STATE_ERROR:
                if (output.setErrorFlag) {
                    if (currentState == STATE_WIFI_CONNECTING) {
                        String detail = "Attempt " + String(connectionFailCycles + 1) + "/" + String(MAX_WIFI_FAIL_CYCLES);
                        handleFailure(FAILURE_WIFI, TXT_WIFI_CONNECTION_FAILED, detail, false);
                    }
                }
                break;

            case STATE_SLEEP_PENDING:
                if (currentState == STATE_WIFI_CONNECTING) {
                    Serial.printf("Connection fail cycle #%d/%d. Retrying later...\n",
                                  connectionFailCycles + 1, MAX_WIFI_FAIL_CYCLES);
                }
                break;

            case STATE_BOOTSTRAP:
                if (!configStore.hasValidWifiConfig()) {
                    bootstrapFromEnv();
                    configStore.loadFromNVS();  // reload after bootstrap attempt
                }
                break;

            case STATE_CHECK_CONFIG:
                // No side effects needed; pure routing state.
                break;
        }

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
/// Results stored in ConfigStore and persisted to NVS.
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
            char city[64] = "";
            char lat[21] = "";
            char lon[21] = "";
            char country[64] = "";

            safeCopy(doc["city"] | "", city, sizeof(city));
            safeCopy(doc["lat"].as<String>().c_str(), lat, sizeof(lat));
            safeCopy(doc["lon"].as<String>().c_str(), lon, sizeof(lon));
            safeCopy(doc["country"] | "", country, sizeof(country));

            configStore.setLocation(lat, lon, city, country, configStore.timezone());
            configStore.setAutoGeo(false);
            configStore.saveToNVS();

            Serial.printf("Successfully geolocated to: %s, %s (%s, %s)\n",
                          city, country, lat, lon);
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

