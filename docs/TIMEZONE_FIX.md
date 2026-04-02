# Timezone Handling Fix: Technical Documentation

## Overview

This document explains the architectural fix implemented to resolve timezone and time offset inconsistencies in the ESP32 Weather E-Paper Display project when using the Open-Meteo API.

## Problem Statement

### The Issue

The Open-Meteo API returns timestamps in different formats depending on the timezone configuration:

| Mode | API Parameter | Timestamp Format | Example (São Paulo, GMT-3) |
|------|--------------|------------------|---------------------------|
| **AUTO** | `timezone=auto` | Local time at coordinates | `"2026-03-24T15:30"` (3:30 PM local) |
| **MANUAL** | `timezone=GMT` | UTC time | `"2026-03-24T18:30"` (6:30 PM UTC) |

**Previous Approach Issues:**
1. Manual offset calculations scattered across multiple modules
2. Double-conversion bugs when applying `timezone_offset` to already-local times
3. Inconsistent handling between system time (NTP) and API timestamps
4. No centralized authority for timezone logic
5. DST (Daylight Saving Time) transitions handled incorrectly

### Impact

- Sunrise/sunset times displayed incorrectly
- Hourly forecast labels showing wrong hours
- Sleep duration calculations miscalculated
- Update timestamps in wrong timezone

## Solution Architecture

### Core Principle: Single Source of Truth

Introduced the `TimeCoordinator` class as the **only** module responsible for:
- Timezone configuration
- Timestamp conversion
- Time formatting for display
- Sleep duration calculation

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│   API Response  │────▶│   API Response   │────▶│  TimeCoordinator │
│   (JSON/HTTP)   │     │   Parser         │     │  (Single Source) │
└─────────────────┘     └──────────────────┘     └────────┬────────┘
                                                          │
                              ┌───────────────────────────┼───────────┐
                              ▼                           ▼           ▼
                        ┌──────────┐               ┌──────────┐  ┌──────────┐
                        │ Renderer │               │  Main    │  │  Power   │
                        │   (UI)   │               │  Loop    │  │ Manager  │
                        └──────────┘               └──────────┘  └──────────┘
```

### Key Components

#### 1. TimeCoordinator Class

**Location:** `include/time_coordinator.h`, `src/time_coordinator.cpp`

**Responsibilities:**
- Detect timezone mode (AUTO vs MANUAL)
- Configure system timezone via POSIX `setenv/tzset`
- Normalize API timestamps
- Format display-ready time strings
- Calculate sleep duration

**Critical Design Decision:**
Use the C library's built-in timezone support instead of manual offset arithmetic:

```cpp
// Configure system timezone
void TimeCoordinator::configureTimezoneFromOffset_(int offsetSeconds) {
    // Convert seconds to POSIX TZ format
    // Note: POSIX sign is inverted (UTC-2 means GMT+2)
    int hours = offsetSeconds / 3600;
    char tzBuf[16];
    snprintf(tzBuf, sizeof(tzBuf), "UTC%+d", -hours);
    
    setenv("TZ", tzBuf, 1);  // Set environment variable
    tzset();                  // Apply to runtime
}
```

Once configured, all time conversions use `localtime_r()`:

```cpp
time_t now = time(nullptr);           // Always UTC from system
tm tmLocal = {};
localtime_r(&now, &tmLocal);          // Converts to local using configured TZ
// tmLocal.tm_hour now contains correct local hour
```

#### 2. API Response Parser Updates

**Location:** `src/api_response.cpp`

**Change:** Parser now aware of input timezone:

```cpp
// Detect mode and set parsing strategy
const bool openMeteoTimeStringsAreUtc = (ramTimezoneMode == TIMEZONE_MODE_MANUAL);

// Parse timestamps accordingly
r.current.dt = parseIso8601(timeStr, openMeteoTimeStringsAreUtc, r.timezone_offset);
```

The `parseIso8601` function handles both cases:

```cpp
int64_t parseIso8601(const char* iso8601, bool inputIsUtc, int tzOffsetSeconds) {
    struct tm timeinfo = {};
    // ... parse year, month, day, hour, minute ...
    
    if (inputIsUtc) {
        // Input is UTC → convert to epoch using UTC timezone
        return timegm_compat(&timeinfo);
    }
    
    // Input is local time → convert considering offset
    if (tzOffsetSeconds != 0) {
        int64_t utcEpoch = timegm_compat(&timeinfo);
        return utcEpoch - tzOffsetSeconds;
    }
    
    return mktime(&timeinfo);
}
```

#### 3. Data Flow

**Step 1: Initialization**
```cpp
void setup() {
    // ... other init ...
    timeCoordinator.begin();  // Detects mode from RTC memory
}
```

**Step 2: API Data Processing**
```cpp
TimeDisplayData TimeCoordinator::process(const owm_resp_onecall_t& apiData, 
                                         unsigned long startTimeMillis) {
    // 1. Extract raw timestamps from API
    NormalizedWeather norm;
    normalize_(apiData, norm);
    
    // 2. Configure system timezone
    if (mode_ == TIME_MODE_AUTO) {
        configureTimezoneFromOffset_(norm.apiOffsetSeconds);
        syncRtcIfNeeded_(norm.apiOffsetSeconds);
    } else {
        configureTimezoneFromString_(TIMEZONE);
    }
    
    // 3. Format all display data using localtime_r()
    TimeDisplayData out;
    formatDisplayData_(apiData, norm, out, startTimeMillis);
    
    return out;
}
```

**Step 3: Display Usage**
```cpp
void drawCurrentConditions(const TimeDisplayData& timeData) {
    // Use pre-formatted strings - no conversion needed
    drawString(timeData.sunriseTime, x, y);
    drawString(timeData.sunsetTime, x, y);
}
```

## Mode-Specific Behavior

### AUTO Mode (Automatic Timezone Detection)

**Use Case:** User selects "Detect automatically" in configuration portal.

**Behavior:**
1. API returns `timezone_offset` (seconds from UTC)
2. `TimeCoordinator` converts offset to POSIX TZ string
3. System timezone set dynamically
4. RTC synchronized with NTP if needed

**Code Flow:**
```cpp
// API returns: timezone_offset = -10800 (GMT-3, São Paulo)
configureTimezoneFromOffset_(-10800);
// Sets TZ="UTC+3"
// localtime_r() will subtract 3 hours from UTC
```

### MANUAL Mode (User-Defined Timezone)

**Use Case:** User manually specifies timezone in config.

**Behavior:**
1. API returns times in UTC (`timezone=GMT`)
2. `TimeCoordinator` uses static `TIMEZONE` from `config.h`
3. System timezone set once at boot

**Code Flow:**
```cpp
// config.h: #define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"
configureTimezoneFromString_(TIMEZONE);
// Handles DST transitions automatically
```

## Benefits

### 1. Eliminated Double-Conversion

**Before:**
```cpp
// WRONG: API time is already local, but we're adding offset again
int localTime = apiTime + timezone_offset;  // Bug!
```

**After:**
```cpp
// CORRECT: Store as epoch, convert using system TZ
localtime_r(&apiTimestamp, &tm);  // Correct conversion
```

### 2. Automatic DST Handling

POSIX TZ strings encode DST rules:
```
EST5EDT,M3.2.0,M11.1.0
```
- `EST5`: Eastern Standard Time (UTC-5)
- `EDT`: Eastern Daylight Time
- `M3.2.0`: Start DST on March, 2nd Sunday, 00:00
- `M11.1.0`: End DST on November, 1st Sunday, 00:00

The C library automatically applies these rules when `localtime_r()` is called.

### 3. Simplified Renderer Code

**Before:**
```cpp
// Renderer had to know about timezones
void drawHourlyForecast(owm_resp_onecall_t& data) {
    for (int i = 0; i < OWM_NUM_HOURLY; i++) {
        time_t t = data.hourly[i].dt + data.timezone_offset;  // Fragile!
        // ... format and draw ...
    }
}
```

**After:**
```cpp
// Renderer receives pre-formatted data
void drawHourlyForecast(const TimeDisplayData& timeData) {
    for (int i = 0; i < OWM_NUM_HOURLY; i++) {
        drawString(timeData.hourlyLabels[i], x, y);  // Simple!
    }
}
```

### 4. Testability

The `TimeCoordinator` can be tested in isolation:
- Mock API responses with various timezone scenarios
- Verify correct output strings
- Test DST transitions
- Test edge cases (UTC+14, UTC-12, etc.)

## Files Changed

| File | Changes |
|------|---------|
| `include/time_coordinator.h` | New header defining TimeCoordinator class and data structures |
| `src/time_coordinator.cpp` | New implementation with timezone logic |
| `include/api_response.h` | Updated to support timezone-aware parsing |
| `src/api_response.cpp` | Modified `parseIso8601` to handle UTC vs local input |
| `include/renderer.h` | Updated signatures to use `TimeDisplayData` |
| `src/renderer.cpp` | Simplified to use pre-formatted time strings |
| `src/main.cpp` | Integrated TimeCoordinator into main loop |
| `include/web_ui_data.h` | Regenerated (auto-generated file) |

## Migration Guide

### For Developers

**Old Pattern (DO NOT USE):**
```cpp
// Manual offset arithmetic
int hour = (apiTime + timezone_offset) % 86400 / 3600;
```

**New Pattern:**
```cpp
// Use TimeCoordinator
timeCoordinator.begin();
TimeDisplayData td = timeCoordinator.process(apiData, startMillis);
// Use td.hourlyLabels, td.sunriseTime, etc.
```

### For Users

No action required. The fix is transparent:
- AUTO mode: Continues to work with automatic detection
- MANUAL mode: Continues to use configured `TIMEZONE`

## Testing

### Test Scenarios Covered

1. **AUTO mode with positive offset** (GMT+5:30, India)
2. **AUTO mode with negative offset** (GMT-8, California)
3. **MANUAL mode with DST** (Europe, summer/winter)
4. **Zero offset** (UTC/GMT)
5. **Cross-midnight forecasts**
6. **Sleep duration calculation at bedtime boundary**

### Verification Steps

1. Check footer displays correct local time
2. Verify sunrise/sunset match expected values
3. Confirm hourly labels start at correct hour
4. Validate sleep duration aligns with wake time

## References

- [Open-Meteo Timezone Documentation](https://open-meteo.com/en/docs)
- [POSIX TZ Format](https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html)
- [ESP32 Time Library](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html)

---

**Commit:** `64b35b80b5429c584740fa42ce16525bbb462c1c`
**Author:** Mauro de Freitas
**Date:** April 1, 2026
