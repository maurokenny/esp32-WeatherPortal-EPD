# Error Screens Comparison: Original vs Current Implementation

This document compares how error conditions are handled and displayed between the **original Luke Marzen implementation** and the **current refactored version**.

## Overview

The original code from Luke Marzen consistently displayed dedicated error screens for all failure conditions. The current implementation uses a tiered approach with different behaviors depending on the error type and failure history.

---

## 1. Low Battery

### Original Implementation (lmarzen)

```cpp
if (batteryVoltage <= LOW_BATTERY_VOLTAGE) {
  if (lowBat == false) {
    drawError(battery_alert_0deg_196x196, TXT_LOW_BATTERY);
  }
  // Sleep with periodic wake-up to retry
  esp_sleep_enable_timer_wakeup(LOW_BATTERY_SLEEP_INTERVAL * 60ULL * 1000000ULL);
  esp_deep_sleep_start();
}
```

| Aspect | Behavior |
|--------|----------|
| **Screen** | Full error screen with battery icon (`battery_alert_0deg_196x196`) |
| **Recovery** | Enters deep sleep, wakes up after `LOW_BATTERY_SLEEP_INTERVAL` minutes to retry |
| **Critical Level** | If `CRIT_LOW_BATTERY_VOLTAGE`, disables timer wakeup (hibernates indefinitely) |

### Current Implementation

```cpp
if (USING_BATTERY && batteryVoltage <= LOW_BATTERY_VOLTAGE) {
    // Immediate sleep logic (NOT IMPLEMENTED)
}
```

| Aspect | Behavior |
|--------|----------|
| **Screen** | ❌ **NOT IMPLEMENTED** - No error screen displayed |
| **Recovery** | Battery check logic exists but is incomplete |

---

## 2. WiFi Connection Failed

### Original Implementation (lmarzen)

```cpp
if (wifiStatus != WL_CONNECTED) {
  drawError(wifi_x_196x196, TXT_WIFI_CONNECTION_FAILED);
  powerOffDisplay();
  beginDeepSleep(startTime, &timeInfo);  // Always retries
}
```

| Aspect | Behavior |
|--------|----------|
| **Screen** | Full error screen with WiFi X icon (`wifi_x_196x196`) |
| **When Shown** | On **every** connection failure |
| **Recovery** | Always enters deep sleep and retries on next wake |
| **User Action** | None required - automatic retry |

### Current Implementation

```cpp
// wifi_manager.cpp
if (connectionFailCycles >= MAX_FAIL_CYCLES) {
    drawErrorScreen("Connection Failed", 
                    "Failed to connect after X attempts",
                    "Please check WiFi signal and credentials");
    isErrorState = true;
    setFirmwareState(STATE_ERROR);  // PERMANENT ERROR
}
```

| Aspect | Behavior |
|--------|----------|
| **Screen** | `drawErrorScreen()` with warning icon |
| **When Shown** | Only after `MAX_FAIL_CYCLES` consecutive failures (default: 3) |
| **Recovery** | ⚠️ **STATE_ERROR = Sleep indefinitely** - Requires manual reset |
| **User Action** | Must press reset button or power cycle to recover |

**Configuration:**
```cpp
#define MAX_FAIL_CYCLES  3   // 0 = infinite retries (never enters STATE_ERROR)
```

---

## 3. Time Server Error (NTP Synchronization)

### Original Implementation (lmarzen)

```cpp
if (!timeConfigured) {
  drawError(wi_time_4_196x196, TXT_TIME_SYNCHRONIZATION_FAILED);
  powerOffDisplay();
  beginDeepSleep(startTime, &timeInfo);  // Always retries
}
```

| Aspect | Behavior |
|--------|----------|
| **Screen** | Full error screen with time icon (`wi_time_4_196x196`) |
| **Recovery** | Enters deep sleep, retries NTP on next wake |
| **User Action** | None required - automatic retry |

### Current Implementation

```cpp
if (!isTimeConfigured) {
  Serial.println(TXT_TIME_SYNCHRONIZATION_FAILED);
  updateEinkStatus(TXT_TIME_SYNCHRONIZATION_FAILED);  // Loading screen
  setFirmwareState(STATE_SLEEP_PENDING);  // Will retry
  return;
}
```

| Aspect | Behavior |
|--------|----------|
| **Screen** | Loading screen with message (`updateEinkStatus`) |
| **Recovery** | Enters deep sleep, retries NTP on next wake |
| **User Action** | None required - automatic retry |

---

## 4. API Error

### Original Implementation (lmarzen)

```cpp
if (rxStatus != HTTP_CODE_OK) {
  statusStr = "One Call " + OWM_ONECALL_VERSION + " API";
  tmpStr = String(rxStatus, DEC) + ": " + getHttpResponsePhrase(rxStatus);
  drawError(wi_cloud_down_196x196, statusStr, tmpStr);
  powerOffDisplay();
  beginDeepSleep(startTime, &timeInfo);  // Always retries
}
```

| Aspect | Behavior |
|--------|----------|
| **Screen** | Full error screen with cloud icon (`wi_cloud_down_196x196`) |
| **Details** | Shows HTTP status code AND error description |
| **Example** | "One Call 3.0 API", "404: Not Found" |
| **Recovery** | Enters deep sleep, retries API on next wake |
| **User Action** | None required - automatic retry |

### Current Implementation

```cpp
if (rxStatus != HTTP_CODE_OK) {
  globalStatus = "Open-Meteo Forecast API Error";
  setFirmwareState(STATE_SLEEP_PENDING);  // Will retry
  return;
}
```

| Aspect | Behavior |
|--------|----------|
| **Screen** | ❌ **NO ERROR SCREEN** - Silent failure |
| **Details** | Only logs to Serial, sets `globalStatus` for status bar |
| **Recovery** | Enters deep sleep, retries API on next wake |
| **User Action** | None required - automatic retry |

---

## Summary Table

| Error Condition | Original (lmarzen) | Current |
|-----------------|-------------------|---------|
| **Low Battery** | `drawError()` + periodic sleep | ❌ Not implemented |
| **WiFi Failed** | `drawError()` + retry | `drawErrorScreen()` only after MAX_FAIL_CYCLES → permanent sleep |
| **NTP Failed** | `drawError()` + retry | `updateEinkStatus()` (loading) + retry |
| **API Failed** | `drawError()` with HTTP code + retry | ❌ Silent (Serial only) + retry |

## Key Behavioral Differences

### 1. Error Screen Consistency
- **Original**: All errors show dedicated `drawError()` screens with appropriate icons
- **Current**: Mixed approach - some show screens, some don't

### 2. WiFi Recovery Strategy
- **Original**: Always retries indefinitely (user-friendly for transient issues)
- **Current**: Can enter permanent error state requiring manual intervention

### 3. Information Detail
- **Original**: API errors show specific HTTP codes and descriptions
- **Current**: Generic status messages only

### 4. Battery Protection
- **Original**: Complete low battery handling with visual feedback
- **Current**: Logic exists but is not fully implemented

## Configuration Recommendations

### To Match Original Behavior (Infinite Retries)

```cpp
// config.h
#define MAX_FAIL_CYCLES  0   // Never enter STATE_ERROR, always retry
```

### To Add Missing Error Screens (Current Code)

To restore original behavior, implement these functions:

```cpp
// For API errors in main.cpp
if (rxStatus != HTTP_CODE_OK) {
    initDisplay();
    String errMsg = String(rxStatus) + ": " + getHttpResponsePhrase(rxStatus);
    do {
        drawError(wi_cloud_down_196x196, "Open-Meteo API Error", errMsg);
    } while (display.nextPage());
    powerOffDisplay();
    setFirmwareState(STATE_SLEEP_PENDING);
    return;
}
```

## Silent Mode Behavior

The current implementation includes a `SILENT_STATUS` configuration option that controls when status screens are displayed.

### Configuration

```cpp
// config.h
#define SILENT_STATUS true   // true = silent mode ON, false = verbose mode
```

### Logic Pattern

All status screens follow this conditional pattern:

```cpp
if (isFirstBoot || !SILENT_STATUS) {
    // Show loading/status screen
    drawLoading(...);
}
```

This means:
- **`isFirstBoot = true`**: Screens are **ALWAYS shown** (regardless of SILENT_STATUS)
- **`isFirstBoot = false` + `SILENT_STATUS = true`**: Screens are **SKIPPED** (silent)
- **`isFirstBoot = false` + `SILENT_STATUS = false`**: Screens are **SHOWN** (verbose)

### Screen Visibility by Mode

| Screen | Condition | Silent Mode (true) | Verbose Mode (false) |
|--------|-----------|-------------------|---------------------|
| **WiFi Connecting** | `isFirstBoot \|\| !SILENT_STATUS` | Only on first boot | Always shown |
| **WiFi Connected** | `isFirstBoot \|\| !SILENT_STATUS` | Only on first boot | Always shown |
| **Fetching Weather** | `isFirstBoot \|\| !SILENT_STATUS` | Only on first boot | Always shown |

### Error Screens (ALWAYS Shown)

The following screens are **NOT affected** by SILENT_STATUS and are always displayed:

| Screen | When Shown | Silent Mode | Verbose Mode |
|--------|-----------|-------------|--------------|
| **AP Mode Screen** | `startAP()` called | ✅ Always | ✅ Always |
| **Timeout Screen** | AP config timeout | ✅ Always | ✅ Always |
| **Connection Failed** | `MAX_FAIL_CYCLES` reached | ✅ Always | ✅ Always |
| **NTP Failed** | Time sync fails | ✅ Always* | ✅ Always* |

*Note: NTP error uses `updateEinkStatus()` which is a loading screen, not `drawErrorScreen()`

### API Errors (NEVER Shown)

API errors are **completely silent** regardless of mode:

| Mode | API Error Display |
|------|------------------|
| Silent (`SILENT_STATUS = true`) | Serial log only |
| Verbose (`SILENT_STATUS = false`) | Serial log only |

### Summary

```
┌─────────────────────────┬──────────────────┬──────────────────┐
│        Screen           │   Silent Mode    │   Verbose Mode   │
│                         │ (SILENT_STATUS=  │ (SILENT_STATUS=  │
│                         │      true)       │      false)      │
├─────────────────────────┼──────────────────┼──────────────────┤
│ WiFi Connecting         │ 1st boot only    │ Always           │
│ WiFi Connected          │ 1st boot only    │ Always           │
│ Fetching Weather        │ 1st boot only    │ Always           │
│ AP Mode                 │ Always           │ Always           │
│ Timeout Error           │ Always           │ Always           │
│ Connection Failed       │ Always           │ Always           │
│ NTP Failed              │ Always*          │ Always*          │
│ API Failed              │ Never            │ Never            │
└─────────────────────────┴──────────────────┴──────────────────┘
* Uses loading screen, not error screen
```

## Recovery Flow Comparison

### Original (lmarzen)

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Error     │───▶│  drawError  │───▶│ deep_sleep  │───▶ [RETRY]
│  Occurs     │    │   Screen    │    │             │
└─────────────┘    └─────────────┘    └─────────────┘
```

### Current (with MAX_FAIL_CYCLES=3 and Silent Mode)

```
SILENT MODE = TRUE, NOT FIRST BOOT:
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│ WiFi Error  │───▶│   NO SCREEN │───▶│ deep_sleep  │───▶ [RETRY]
│ (1st-2nd)   │    │   (silent)  │    │             │
└─────────────┘    └─────────────┘    └─────────────┘

┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│ WiFi Error  │───▶│ drawError   │───▶│ PERMANENT   │───▶ [RESET]
│  (3rd+)     │    │   Screen    │    │   SLEEP     │     REQUIRED
└─────────────┘    └─────────────┘    └─────────────┘

SILENT MODE = FALSE OR FIRST BOOT:
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│ WiFi Error  │───▶│  drawLoading│───▶│ deep_sleep  │───▶ [RETRY]
│ (1st-2nd)   │    │   "Connecting"│   │             │
└─────────────┘    └─────────────┘    └─────────────┘
```

## Power Source Configuration

The `USING_BATTERY` setting affects low battery detection and protection behavior.

### Configuration

```cpp
// config.h
// POWER SOURCE
//   Set to 1 if powering via battery (enables low voltage protection)
//   Set to 0 if powering via USB/power supply (disables low voltage sleep)
#define USING_BATTERY 0
```

### Battery Monitoring (Current Implementation)

```cpp
// main.cpp (setup function)
uint32_t batteryVoltage = readBatteryVoltage();
if (USING_BATTERY && batteryVoltage <= LOW_BATTERY_VOLTAGE) {
    // Immediate sleep logic (NOT IMPLEMENTED)
}
```

### Behavior Comparison

| Setting | Low Battery Detection | Low Battery Screen | Sleep Behavior |
|---------|----------------------|-------------------|----------------|
| `USING_BATTERY 1` | ✅ Enabled | ❌ Not shown (incomplete) | Undefined |
| `USING_BATTERY 0` | ❌ Disabled | N/A | Normal operation |

### Voltage Thresholds (Defined but Not Fully Used)

```cpp
// config.cpp
const uint32_t WARN_BATTERY_VOLTAGE     = 3535;  // ~20%
const uint32_t LOW_BATTERY_VOLTAGE      = 3462;  // ~10%
const uint32_t VERY_LOW_BATTERY_VOLTAGE = 3442;  // ~8%
const uint32_t CRIT_LOW_BATTERY_VOLTAGE = 3404;  // ~5%

const unsigned long LOW_BATTERY_SLEEP_INTERVAL      = 30;   // minutes
const unsigned long VERY_LOW_BATTERY_SLEEP_INTERVAL = 120;  // minutes
```

These thresholds exist in the current code but the protection logic is **incomplete**.

### Original Implementation (Fully Functional)

```cpp
// Original lmarzen implementation
#if BATTERY_MONITORING
  uint32_t batteryVoltage = readBatteryVoltage();
  
  // LOW BATTERY: Show error screen + periodic sleep
  if (batteryVoltage <= LOW_BATTERY_VOLTAGE) {
    if (lowBat == false) {
      initDisplay();
      do {
        drawError(battery_alert_0deg_196x196, TXT_LOW_BATTERY);
      } while (display.nextPage());
      powerOffDisplay();
    }
    esp_sleep_enable_timer_wakeup(LOW_BATTERY_SLEEP_INTERVAL * 60ULL * 1000000ULL);
    esp_deep_sleep_start();
  }
  
  // VERY LOW BATTERY: Longer sleep interval
  if (batteryVoltage <= VERY_LOW_BATTERY_VOLTAGE) {
    esp_sleep_enable_timer_wakeup(VERY_LOW_BATTERY_SLEEP_INTERVAL * 60ULL * 1000000ULL);
  }
  
  // CRITICAL BATTERY: Hibernate indefinitely (no timer wakeup)
  if (batteryVoltage <= CRIT_LOW_BATTERY_VOLTAGE) {
    // No esp_sleep_enable_timer_wakeup() called
    // Requires manual reset to wake
  }
#endif
```

### Current Implementation (Incomplete)

```cpp
// Current implementation - logic exists but is empty
#if BATTERY_MONITORING
  uint32_t batteryVoltage = readBatteryVoltage();
  // ... voltage is read but not acted upon in most cases
#endif

// In setup():
if (USING_BATTERY && batteryVoltage <= LOW_BATTERY_VOLTAGE) {
    // Immediate sleep logic (NOT IMPLEMENTED)
    // No error screen, no sleep configuration
}
```

### Summary Table: Battery Protection

| Feature | Original (lmarzen) | Current (USING_BATTERY=1) | Current (USING_BATTERY=0) |
|---------|-------------------|---------------------------|---------------------------|
| **Low Battery Screen** | ✅ `drawError()` with battery icon | ❌ Not shown | N/A |
| **Periodic Sleep** | ✅ Configurable intervals | ❌ Not implemented | N/A |
| **Critical Hibernation** | ✅ Sleep forever (no wakeup) | ❌ Not implemented | N/A |
| **Voltage Display** | ✅ Always shown on status bar | ✅ Always shown* | ✅ Always shown* |

*Note: Battery voltage is always displayed on the status bar regardless of USING_BATTERY setting.

## Configuration Examples

### Maximum Silence (Default)

```cpp
#define SILENT_STATUS true
#define MAX_FAIL_CYCLES 3
#define USING_BATTERY 0
```
- Minimal screen updates
- Only critical errors shown
- First boot shows status, then silent
- No battery protection (assumes USB power)

### Maximum Verbosity

```cpp
#define SILENT_STATUS false
#define MAX_FAIL_CYCLES 0
#define USING_BATTERY 0
```
- All status screens shown
- WiFi never enters permanent error state
- Always retries (like original)
- No battery protection

### Battery-Powered Silent Mode

```cpp
#define SILENT_STATUS true
#define MAX_FAIL_CYCLES 5
#define USING_BATTERY 1
```
- ⚠️ **Warning**: Battery protection incomplete
- Silent operation after first boot
- Allows 5 WiFi failures before permanent error
- Low battery detection exists but screen/sleep logic not implemented

### Development/Debug Mode

```cpp
#define SILENT_STATUS false
#define DEBUG_LEVEL 2
#define MAX_FAIL_CYCLES 0
#define USING_BATTERY 0
```
- All screens visible
- Full debug output
- Infinite retries
- No battery protection (USB powered)

### Recommended for Production (Battery)

To match original battery protection behavior:

```cpp
// config.h
#define USING_BATTERY 1

// Then implement the missing logic in main.cpp setup():
if (USING_BATTERY && batteryVoltage <= LOW_BATTERY_VOLTAGE) {
    initDisplay();
    do {
        drawError(battery_alert_0deg_196x196, TXT_LOW_BATTERY);
    } while (display.nextPage());
    powerOffDisplay();
    
    // Configure sleep based on voltage level
    if (batteryVoltage <= CRIT_LOW_BATTERY_VOLTAGE) {
        // Hibernate indefinitely
        Serial.println(TXT_CRIT_LOW_BATTERY_VOLTAGE);
        esp_deep_sleep_start();  // No timer wakeup
    } else if (batteryVoltage <= VERY_LOW_BATTERY_VOLTAGE) {
        esp_sleep_enable_timer_wakeup(VERY_LOW_BATTERY_SLEEP_INTERVAL * 60ULL * 1000000ULL);
        esp_deep_sleep_start();
    } else {
        esp_sleep_enable_timer_wakeup(LOW_BATTERY_SLEEP_INTERVAL * 60ULL * 1000000ULL);
        esp_deep_sleep_start();
    }
}
```

## References

- Original: `https://github.com/lmarzen/esp32-weather-epd`
- Current: Local refactored version with state machine
