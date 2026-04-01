# ESP32 Weather Station State Machine

This document describes the firmware state machine implementation for the ESP32 Weather E-Paper Display project. The state machine manages WiFi connectivity, configuration mode, API data fetching, and error recovery with persistent state across deep sleep cycles.

## Table of Contents

- [Overview](#overview)
- [States](#states)
- [State Transitions](#state-transitions)
- [Persistent Variables](#persistent-variables)
- [Configuration](#configuration)
- [Failure Cycle Logic](#failure-cycle-logic)
- [Implementation Details](#implementation-details)

---

## Overview

The firmware implements a deterministic state machine designed for battery-powered operation with deep sleep. All states transition based on timeouts or events, not retry counters. The state persists across deep sleep cycles using RTC (Real-Time Clock) memory.

```
┌─────────┐     ┌──────────────────┐     ┌─────────────────┐
│  BOOT   │────→│ WIFI_CONNECTING  │────→│  NORMAL_MODE    │
└─────────┘     └──────────────────┘     └─────────────────┘
                       │    │                      │
                       │    └──────────────────────┤
                       ↓                           ↓
                AP_CONFIG_MODE ←─────────── SLEEP_PENDING
                       │                           │
                       └───────────────────────────┘
                       ↓
              ERROR_CONNECTION (permanent)
```

---

## States

### 1. STATE_BOOT

**Purpose**: Initial state after device power-on or wake from deep sleep.

**Entry Actions**:
- Check if WiFi credentials exist in RTC memory (`ramSSID`)

**Transitions**:
| Condition | Next State |
|-----------|------------|
| `ramSSID` is not empty | `STATE_WIFI_CONNECTING` |
| `ramSSID` is empty AND `isFirstBoot == true` | `STATE_AP_CONFIG_MODE` |

**Notes**:
- This state executes only once per boot cycle
- No timeout applies to this state

---

### 2. STATE_WIFI_CONNECTING

**Purpose**: Attempt to establish WiFi connection with a defined timeout.

**Entry Actions**:
- Call `WiFi.begin(ramSSID, ramPassword)`
- Record start time: `wifiStartTime = millis()`
- Optionally display "Connecting..." screen

**During State**:
- Continuously poll `WiFi.status()`

**Transitions**:

| Condition | Next State | Actions |
|-----------|------------|---------|
| `WiFi.status() == WL_CONNECTED` | `STATE_NORMAL_MODE` | Reset `connectionFailCycles = 0` |
| Timeout reached AND `isFirstBoot == true` | `STATE_AP_CONFIG_MODE` | Start AP for configuration |
| Timeout reached AND `isFirstBoot == false` AND (`MAX_FAIL_CYCLES == 0` OR `connectionFailCycles < MAX_FAIL_CYCLES`) | `STATE_SLEEP_PENDING` | Increment `connectionFailCycles` |
| Timeout reached AND `isFirstBoot == false` AND `MAX_FAIL_CYCLES > 0` AND `connectionFailCycles >= MAX_FAIL_CYCLES` | `STATE_ERROR_CONNECTION` | Display error, sleep forever |

**Timeout**: Defined by `wifiConfig.wifiConnectTimeout` (seconds)

---

### 3. STATE_AP_CONFIG_MODE

**Purpose**: Provide a captive portal for WiFi configuration. Only available on first boot or when device has never connected before.

**Entry Actions**:
- Start WiFi Access Point: `WiFi.softAP("weather_eink-AP")`
- Start DNS server for captive portal
- Start AsyncWebServer
- Record start time: `portalStartTime = millis()`
- Display AP mode screen with SSID and timeout

**During State**:
- Process DNS requests
- Handle HTTP requests
- Monitor for configuration save (`/save` endpoint)

**Transitions**:

| Condition | Next State | Actions |
|-----------|------------|---------|
| User saves valid configuration | `STATE_WIFI_CONNECTING` | Stop AP/DNS, apply new credentials |
| Timeout reached (`AP_TIMEOUT_MS`) | `STATE_ERROR_CONNECTION` | Stop AP, display error, sleep forever |

**Timeout**: Defined by `wifiConfig.configTimeout` (seconds), default 300s (5 minutes)

**Important**: This state is only entered when `isFirstBoot == true`. If the device has previously connected successfully but lost credentials, it will enter `STATE_ERROR_CONNECTION` instead.

---

### 4. STATE_NORMAL_MODE

**Purpose**: Fetch weather data from API and update the display.

**Entry Actions**:
- NTP time synchronization
- (Optional) IP-based geolocation if `ramAutoGeo == true`

**During State**:
- Fetch weather data from Open-Meteo API
- Fetch air quality data (if enabled)
- Read BME sensor data
- Render display with current conditions and forecast

**Transitions**:

| Condition | Next State | Actions |
|-----------|------------|---------|
| Data fetch successful | `STATE_SLEEP_PENDING` | Set `isFirstBoot = false`, reset `connectionFailCycles = 0` |
| API fetch fails | `STATE_SLEEP_PENDING` | Keep previous valid data (do NOT overwrite) |
| Time sync fails | `STATE_SLEEP_PENDING` | Display error status |

**Notes**:
- Valid data is NEVER overwritten by API failures
- Previous weather data persists across failed updates
- `isFirstBoot` is set to `false` only after first successful render

---

### 5. STATE_SLEEP_PENDING

**Purpose**: Prepare for and enter deep sleep to conserve battery.

**Entry Actions**:
- Calculate next wake time based on `SLEEP_DURATION`
- Apply bedtime logic (optional battery saving)

**Transitions**:

| Condition | Next State |
|-----------|------------|
| After `beginDeepSleep()` completes | Wake → `STATE_BOOT` |

**Wake Behavior**:
- Device wakes from deep sleep
- Program restarts from `setup()`
- State re-enters `STATE_BOOT`
- All RTC memory variables persist

---

### 6. STATE_ERROR_CONNECTION

**Purpose**: Permanent error state when maximum retry attempts are exhausted or AP configuration times out.

**Entry Actions**:
- Display error message on e-paper screen
- Log error to Serial

**Behavior**:
- Enter **indefinite deep sleep** (no timer wakeup)
- Device will NOT wake up automatically
- User must manually reset or power cycle to recover

**Recovery**:
- Press RESET button
- Power cycle the device
- This clears RTC memory (including `connectionFailCycles`)

**When Entered**:
1. WiFi fails `MAX_FAIL_CYCLES` consecutive times (if `MAX_FAIL_CYCLES > 0`)
2. AP configuration mode times out without receiving valid credentials

---

## State Transitions

### Complete Transition Table

| Current State | Event | Condition | Next State |
|---------------|-------|-----------|------------|
| BOOT | Credentials exist | `strlen(ramSSID) > 0` | WIFI_CONNECTING |
| BOOT | No credentials, first boot | `ramSSID` empty AND `isFirstBoot` | AP_CONFIG_MODE |
| WIFI_CONNECTING | WiFi connected | `WL_CONNECTED` | NORMAL_MODE |
| WIFI_CONNECTING | Timeout, first boot | `isFirstBoot == true` | AP_CONFIG_MODE |
| WIFI_CONNECTING | Timeout, retry available | `isFirstBoot == false` AND (`MAX_FAIL_CYCLES == 0` OR `failCycles < MAX`) | SLEEP_PENDING |
| WIFI_CONNECTING | Timeout, max failures | `isFirstBoot == false` AND `MAX_FAIL_CYCLES > 0` AND `failCycles >= MAX` | ERROR_CONNECTION |
| AP_CONFIG_MODE | Config saved | Valid POST to `/save` | WIFI_CONNECTING |
| AP_CONFIG_MODE | Timeout | `portalTimeout` exceeded | ERROR_CONNECTION |
| NORMAL_MODE | Success | Render complete | SLEEP_PENDING |
| NORMAL_MODE | API failure | HTTP error | SLEEP_PENDING |
| SLEEP_PENDING | Wake up | Deep sleep ends | BOOT |
| ERROR_CONNECTION | (none) | Permanent sleep | (none) |

---

## Persistent Variables

Variables stored in RTC memory survive deep sleep but are cleared on power loss:

| Variable | Type | Purpose |
|----------|------|---------|
| `isFirstBoot` | `bool` | `true` until first successful data fetch and render |
| `connectionFailCycles` | `uint8_t` | Consecutive WiFi connection failures after first boot |
| `ramSSID[33]` | `char[]` | WiFi SSID (32 chars + null) |
| `ramPassword[64]` | `char[]` | WiFi password (63 chars + null) |
| `ramCity[64]` | `char[]` | Location city name |
| `ramCountry[64]` | `char[]` | Location country name |
| `ramLat[21]` | `char[]` | Latitude (high precision) |
| `ramLon[21]` | `char[]` | Longitude (high precision) |
| `ramTimezone[64]` | `char[]` | POSIX timezone string |
| `ramAutoGeo` | `bool` | Enable automatic IP geolocation |
| `ramTimezoneMode` | `uint8_t` | `TIMEZONE_MODE_AUTO` or `TIMEZONE_MODE_MANUAL` |
| `rtcInitialized` | `bool` | RTC memory has been initialized |

---

## Configuration

### MAX_FAIL_CYCLES

Located in `include/config.h`:

```cpp
// Maximum consecutive WiFi connection failures before entering ERROR_CONNECTION state.
// Set to 0 for infinite retries (device will never enter ERROR_CONNECTION due to WiFi failures).
#define MAX_FAIL_CYCLES  3   // 0 = infinite retries, >0 = max failures before permanent sleep
```

| Value | Behavior |
|-------|----------|
| `0` | Infinite retries. Device never enters `ERROR_CONNECTION` from WiFi failures. Always sleeps and retries. |
| `1` | Single failure allowed. Second consecutive failure enters `ERROR_CONNECTION`. |
| `3` | (Default) Three failures allowed. Fourth failure enters `ERROR_CONNECTION`. |
| `N` | N failures allowed. (N+1)th failure enters `ERROR_CONNECTION`. |

### Timeouts

Located in `src/config.cpp`:

```cpp
struct DeviceConfig wifiConfig = {
    .wifiConnectTimeout = 20,   // WiFi connection timeout (seconds)
    .configTimeout = 300        // AP mode timeout (seconds) = 5 minutes
};
```

---

## Failure Cycle Logic

### What Counts as a Failure Cycle?

A "failure cycle" is defined as a complete boot where:
1. Device attempts to connect to WiFi (`STATE_WIFI_CONNECTING`)
2. WiFi connection times out without success
3. Device enters deep sleep (`STATE_SLEEP_PENDING`)

### When is the Counter Incremented?

The `connectionFailCycles` counter is **only incremented when `isFirstBoot == false`**:

```cpp
if (!isFirstBoot) {
    connectionFailCycles++;
}
```

This means:
- **First boot failures**: Counter stays at 0, device enters AP mode for configuration
- **Subsequent failures**: Counter increments, device either retries or enters error state

### When is the Counter Reset?

The counter is reset to 0 when:
1. WiFi connects successfully (`STATE_WIFI_CONNECTING` → `STATE_NORMAL_MODE`)
2. Weather data is fetched and rendered successfully (end of `STATE_NORMAL_MODE`)

### Example Scenario (MAX_FAIL_CYCLES = 3)

| Boot # | isFirstBoot | WiFi Result | Fail Cycles | Action | Next State |
|--------|-------------|-------------|-------------|--------|------------|
| 1 | true | Timeout | 0 | First boot → AP mode | AP_CONFIG_MODE |
| 2 | true* | Config saved → Connect | 0 | Connected! | NORMAL_MODE |
| 3 | false | Timeout | 1 | Retry (1/3) | SLEEP_PENDING |
| 4 | false | Timeout | 2 | Retry (2/3) | SLEEP_PENDING |
| 5 | false | Timeout | 3 | Retry (3/3) | SLEEP_PENDING |
| 6 | false | Timeout | 4 | Max reached! | ERROR_CONNECTION |

\* After successful render, `isFirstBoot` becomes `false`

---

## Implementation Details

### File Structure

| File | Purpose |
|------|---------|
| `include/wifi_manager.h` | State enum, RTC variable declarations, function prototypes |
| `src/wifi_manager.cpp` | State machine implementation (`wifiManagerLoop()`) |
| `src/main.cpp` | `STATE_NORMAL_MODE` implementation, error display, deep sleep entry |
| `include/config.h` | `MAX_FAIL_CYCLES` compile-time constant |

### Key Functions

```cpp
// State machine entry and loop
void wifiManagerSetup();
void wifiManagerLoop();

// State transition helper
void setFirmwareState(FirmwareState newState);

// AP mode handler
void startAP();

// Configuration save handler
void handleConfigSave(AsyncWebServerRequest* request);

// Deep sleep entry (main.cpp)
void beginDeepSleep(unsigned long startTime, tm *timeInfo, int tzOffsetSeconds);
```

### Thread Safety

The state machine is single-threaded and runs in the main `loop()`. No concurrent access issues exist as there are no ISRs modifying state.

### Memory Safety

- All buffers use `sizeof(buffer) - 1` for string operations
- `strlcpy` is used for safe string copying with guaranteed null-termination
- No dynamic memory allocation in state handlers

---

## Troubleshooting

### Device keeps entering AP mode
- Check that `ramSSID` is being saved correctly
- Verify `/save` endpoint is receiving correct POST data
- Check Serial output for validation errors

### Device sleeps forever (ERROR_CONNECTION)
- Check `connectionFailCycles` in Serial output
- Verify `MAX_FAIL_CYCLES` setting
- Power cycle to clear RTC memory and reset counters

### Data not updating but device stays awake
- Check `STATE_NORMAL_MODE` implementation
- Verify API responses with `DEBUG_LEVEL >= 2`
- Previous valid data is preserved - this is expected behavior

---

## License

This state machine implementation is part of the ESP32 Weather E-Paper Display project and follows the same GPL v3 license.
