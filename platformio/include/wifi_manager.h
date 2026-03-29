#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

// State Machine States
enum FirmwareState {
    STATE_BOOT,
    STATE_WIFI_CONNECTING,
    STATE_AP_CONFIG_MODE,
    STATE_NORMAL_MODE,
    STATE_SLEEP_PENDING
};

extern FirmwareState currentState;

// Configuration Structure
struct DeviceConfig {
    uint32_t wifiConnectTimeout;   // seconds
    uint32_t configTimeout;        // seconds
};

extern DeviceConfig wifiConfig;

// Runtime State
struct RuntimeState {
    bool apMode;
    bool wifiConnected;
    bool portalActive;
    uint32_t wifiStartTime;
    uint32_t portalStartTime;
};

extern RuntimeState runtime;

// RAM Variables for connection and location
extern char ramSSID[64];
extern char ramPassword[64];
extern char ramCity[64];
extern char ramCountry[64];
extern char ramLat[32];
extern char ramLon[32];
extern bool ramAutoGeo;
extern bool rtcInitialized;
extern bool isFirstBoot;

// Functions
void wifiManagerSetup();
void wifiManagerLoop();

// Helper to transition state cleanly
void setFirmwareState(FirmwareState newState);
bool locateByIpAddress();

#endif
