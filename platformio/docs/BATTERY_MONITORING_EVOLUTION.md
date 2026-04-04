# Battery Configuration Evolution: Single vs Dual Flag

This document explains the evolution from the original single-flag battery configuration (`BATTERY_MONITORING`) to the current dual-flag system (`BATTERY_MONITORING` + `USING_BATTERY`).

---

## Overview

| Aspect | Original (lmarzen) | Current |
|--------|-------------------|---------|
| **Configuration Flags** | 1 (`BATTERY_MONITORING`) | 2 (`BATTERY_MONITORING` + `USING_BATTERY`) |
| **Lines of Code** | ~50 lines | ~10 lines (incomplete) |
| **Battery Protection** | Complete | Incomplete |
| **USB Operation** | Supported | Supported |

---

## 1. Original Implementation (Single Flag)

### Configuration

```cpp
// config.h
#define BATTERY_MONITORING 1  // 1 = Enable battery monitoring and protection
                              // 0 = Disable (assume USB power)
```

### Code Implementation

```cpp
// main.cpp - Original lmarzen implementation
void setup()
{
  // ... initialization code ...
  
  prefs.begin(NVS_NAMESPACE, false);

#if BATTERY_MONITORING
  // ============================================
  // BATTERY MONITORING ENABLED
  // ============================================
  uint32_t batteryVoltage = readBatteryVoltage();
  Serial.print(TXT_BATTERY_VOLTAGE);
  Serial.println(": " + String(batteryVoltage) + "mv");

  // Track low battery state in NVS (Non-Volatile Storage)
  bool lowBat = prefs.getBool("lowBat", false);

  // LOW BATTERY DETECTED
  if (batteryVoltage <= LOW_BATTERY_VOLTAGE)
  {
    if (lowBat == false)
    { 
      // First time detecting low battery - SHOW ERROR SCREEN
      prefs.putBool("lowBat", true);
      prefs.end();
      initDisplay();
      do
      {
        drawError(battery_alert_0deg_196x196, TXT_LOW_BATTERY);
      } while (display.nextPage());
      powerOffDisplay();
    }

    // Determine sleep duration based on voltage level
    if (batteryVoltage <= CRIT_LOW_BATTERY_VOLTAGE)
    {
      // CRITICAL: Hibernate indefinitely (manual reset required)
      Serial.println(TXT_CRIT_LOW_BATTERY_VOLTAGE);
      Serial.println(TXT_HIBERNATING_INDEFINITELY_NOTICE);
      // No esp_sleep_enable_timer_wakeup() called!
    }
    else if (batteryVoltage <= VERY_LOW_BATTERY_VOLTAGE)
    {
      // VERY LOW: Long sleep interval (2 hours)
      esp_sleep_enable_timer_wakeup(VERY_LOW_BATTERY_SLEEP_INTERVAL
                                    * 60ULL * 1000000ULL);
      Serial.println(TXT_VERY_LOW_BATTERY_VOLTAGE);
      Serial.print(TXT_ENTERING_DEEP_SLEEP_FOR);
      Serial.println(" " + String(VERY_LOW_BATTERY_SLEEP_INTERVAL) + "min");
    }
    else
    {
      // LOW: Standard sleep interval (30 minutes)
      esp_sleep_enable_timer_wakeup(LOW_BATTERY_SLEEP_INTERVAL
                                    * 60ULL * 1000000ULL);
      Serial.println(TXT_LOW_BATTERY_VOLTAGE);
      Serial.print(TXT_ENTERING_DEEP_SLEEP_FOR);
      Serial.println(" " + String(LOW_BATTERY_SLEEP_INTERVAL) + "min");
    }
    
    // Enter deep sleep (may or may not wake up based on voltage)
    esp_deep_sleep_start();
  }
  
  // Battery recovered - reset NVS flag
  if (lowBat == true)
  {
    prefs.putBool("lowBat", false);
  }
  
#else
  // ============================================
  // BATTERY MONITORING DISABLED (USB POWER)
  // ============================================
  uint32_t batteryVoltage = UINT32_MAX;  // Fake "full battery" value
  
#endif

  prefs.end();
  // ... continue with WiFi, API, rendering ...
}
```

### Behavior Matrix (Original)

| BATTERY_MONITORING | Hardware | What Happens |
|-------------------|----------|--------------|
| **1** | With Battery | Full protection: monitors voltage, shows low battery screen, hibernates if critical |
| **1** | USB Only | Will try to read ADC (may show wrong values), might trigger false protection |
| **0** | With Battery | No protection! Will run until battery dies (potential damage) |
| **0** | USB Only | Works perfectly. `UINT32_MAX` prevents any protection triggers |

### Key Features (Original)

1. **NVS Persistence**: Remembers low battery state across **power cycles** (uses Preferences/flash storage)
2. **Graduated Response**: Different sleep intervals for different voltage levels
3. **Visual Feedback**: Error screen shown on first low battery detection
4. **Critical Protection**: Can hibernate indefinitely to prevent battery damage
5. **Auto-Recovery**: Resets low battery flag when voltage recovers

> **Note**: NVS (Non-Volatile Storage) persists across power cycles and deep sleep. This is different from RTC memory which only persists during deep sleep but is lost on power loss.

---

## 2. Current Implementation (Dual Flag)

### Configuration

```cpp
// config.h
#define BATTERY_MONITORING 1  // Enable battery icon/voltage display
                              // Does NOT control protection anymore

#define USING_BATTERY 0       // 1 = Enable low voltage protection
                              // 0 = USB power (no protection needed)
```

### Code Implementation

```cpp
// main.cpp - Current implementation
void setup()
{
  // ... initialization code ...

  // BATTERY_MONITORING is used in renderer.cpp to show battery icon
  // It does NOT affect main.cpp behavior anymore

  // USING_BATTERY check (incomplete implementation)
  uint32_t batteryVoltage = readBatteryVoltage();
  if (USING_BATTERY && batteryVoltage <= LOW_BATTERY_VOLTAGE) {
      // Immediate sleep logic (NOT IMPLEMENTED)
      // Empty block - no protection!
  }
  
  // ... continue with WiFi, API, rendering ...
}
```

```cpp
// renderer.cpp - Status bar rendering
void drawStatusBar(String &statusStr, String &refreshTimeStr, 
                   int rssi, uint32_t batVoltage)
{
  // ... other elements ...

#if BATTERY_MONITORING
  // Only controls whether battery icon is shown
  uint32_t batPercent = calcBatPercent(batVoltage,
                                       MIN_BATTERY_VOLTAGE,
                                       MAX_BATTERY_VOLTAGE);
  // Draw battery icon with percentage
#endif
}
```

### Behavior Matrix (Current)

| BATTERY_MONITORING | USING_BATTERY | Hardware | Battery Icon | Protection | Result |
|-------------------|---------------|----------|--------------|------------|--------|
| 1 | 1 | With Battery | ✅ Shown | ❌ Incomplete | ⚠️ Shows % but no real protection |
| 1 | 1 | USB Only | ✅ Shown (fake %) | ❌ N/A | Works but shows fake battery |
| 1 | 0 | With Battery | ✅ Shown | ❌ Disabled | ⚠️ Shows % but no protection |
| 1 | 0 | USB Only | ✅ Shown (fake %) | ❌ N/A | Works, shows fake battery |
| 0 | 1 | With Battery | ❌ Hidden | ❌ Incomplete | No info, no protection |
| 0 | 0 | Any | ❌ Hidden | ❌ N/A | No battery info |

---

## 3. Key Differences

### 3.1 Separation of Concerns

| Feature | Original | Current |
|---------|----------|---------|
| **Display Icon** | Controlled by BATTERY_MONITORING | Controlled by BATTERY_MONITORING |
| **Protection Logic** | Controlled by BATTERY_MONITORING | Controlled by USING_BATTERY |
| **NVS Persistence** | Yes (tracks low battery state in flash) | No (no persistence) |
| **Graduated Levels** | 3 levels (Low/Very Low/Critical) | None |
| **RTC Persistence** | No (uses NVS instead) | No (no battery state tracked) |

### 3.2 Code Complexity

**Original:**
- 1 flag, 1 location, 50+ lines
- All logic in `main.cpp setup()`
- Self-contained and complete

**Current:**
- 2 flags, 2 locations
- Display logic in `renderer.cpp`
- Protection logic in `main.cpp` (incomplete)
- Fragmented and non-functional

### 3.3 Protection Behavior

**Original (BATTERY_MONITORING=1):**
```
Battery Voltage Check
        │
        ▼
┌─────────────────┐
│ > LOW_BATTERY   │───▶ Continue normal operation
│    _VOLTAGE     │
└─────────────────┘
        │
        ▼
┌─────────────────┐     ┌─────────────────┐
│ ≤ CRIT_LOW      │────▶│ Hibernate       │
│ _BATTERY        │     │ indefinitely    │
└─────────────────┘     └─────────────────┘
        │
        ▼
┌─────────────────┐     ┌─────────────────┐
│ ≤ VERY_LOW      │────▶│ Sleep 2 hours   │
│ _BATTERY        │     └─────────────────┘
└─────────────────┘
        │
        ▼
┌─────────────────┐     ┌─────────────────┐
│ ≤ LOW_BATTERY   │────▶│ Sleep 30 min    │
│ _VOLTAGE        │     └─────────────────┘
└─────────────────┘
```

**Current (USING_BATTERY=1):**
```
Battery Voltage Check
        │
        ▼
┌─────────────────┐
│ USING_BATTERY=1 │───▶ Check voltage
│                 │
└─────────────────┘
        │
        ▼
┌─────────────────┐
│ ≤ LOW_BATTERY   │───▶ // Immediate sleep logic
│ _VOLTAGE        │     // (NOT IMPLEMENTED)
└─────────────────┘
        │
        ▼
    Continue...
    (No action taken!)
```

---

## 4. Migration Scenarios

### Scenario 1: USB-Powered Display

**Original:**
```cpp
#define BATTERY_MONITORING 0
```
✅ Works perfectly, no battery checks

**Current:**
```cpp
#define BATTERY_MONITORING 1  // Or 0, doesn't matter for protection
#define USING_BATTERY 0
```
✅ Works, but with dummy battery value if BATTERY_MONITORING=1

### Scenario 2: Battery-Powered Display

**Original:**
```cpp
#define BATTERY_MONITORING 1
```
✅ Full protection, graduated responses, visual feedback

**Current:**
```cpp
#define BATTERY_MONITORING 1
#define USING_BATTERY 1
```
❌ **BROKEN**: Shows battery % but protection code is incomplete!

### Scenario 3: Battery Display with Safety

**Original:**
```cpp
#define BATTERY_MONITORING 1
// Protection automatically enabled
```

**Current (requires manual fix):**
```cpp
#define BATTERY_MONITORING 1
#define USING_BATTERY 1

// Then implement the missing protection code in main.cpp!
```

---

## 5. Recommendations

### To Fix Current Implementation

Replace the incomplete code in `main.cpp`:

```cpp
// BEFORE (Current - Broken):
#if BATTERY_MONITORING
  uint32_t batteryVoltage = readBatteryVoltage();
#endif
// ... later ...
if (USING_BATTERY && batteryVoltage <= LOW_BATTERY_VOLTAGE) {
    // Immediate sleep logic (NOT IMPLEMENTED)
}

// AFTER (Fixed - Similar to Original):
#if USING_BATTERY
  uint32_t batteryVoltage = readBatteryVoltage();
  
  if (batteryVoltage <= LOW_BATTERY_VOLTAGE) {
    // Show low battery error screen
    initDisplay();
    do {
      drawError(battery_alert_0deg_196x196, TXT_LOW_BATTERY);
    } while (display.nextPage());
    powerOffDisplay();
    
    // Configure sleep based on severity
    if (batteryVoltage <= CRIT_LOW_BATTERY_VOLTAGE) {
      // Hibernate indefinitely
      esp_deep_sleep_start();
    } else if (batteryVoltage <= VERY_LOW_BATTERY_VOLTAGE) {
      esp_sleep_enable_timer_wakeup(VERY_LOW_BATTERY_SLEEP_INTERVAL 
                                    * 60ULL * 1000000ULL);
      esp_deep_sleep_start();
    } else {
      esp_sleep_enable_timer_wakeup(LOW_BATTERY_SLEEP_INTERVAL 
                                    * 60ULL * 1000000ULL);
      esp_deep_sleep_start();
    }
  }
#else
  uint32_t batteryVoltage = UINT32_MAX;
#endif

// BATTERY_MONITORING only controls display in renderer.cpp
```

### Simplified Alternative

Unify the flags:

```cpp
// config.h - Single flag approach
#define BATTERY_MODE 0  // 0 = USB (no battery)
                         // 1 = Battery with protection
                         // 2 = Battery monitoring only (test mode)
```

---

## Summary Table

| Aspect | Original (Single Flag) | Current (Dual Flag) | Recommendation |
|--------|----------------------|---------------------|----------------|
| **Configuration** | Simple, 1 flag | Complex, 2 flags | Unify to 1 flag |
| **Protection** | Complete | Broken | Fix or revert |
| **USB Support** | Works | Works | Keep |
| **Code Size** | ~50 lines | ~10 lines (incomplete) | Complete the code |
| **NVS Usage** | Tracks low battery | Not used | Re-add if needed |
| **User Safety** | High | Low (false sense of security) | Fix immediately |

---

## Conclusion

The dual-flag system was intended to provide more flexibility but resulted in:
1. **Fragmented logic** split across multiple files
2. **Incomplete protection** that gives false confidence
3. **Configuration confusion** (2 flags with overlapping purposes)

**Recommended Action:** Either complete the protection code for `USING_BATTERY` or revert to the single-flag approach which was simpler and fully functional.
