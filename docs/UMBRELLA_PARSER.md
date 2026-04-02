# Umbrella Widget Data Parser

## Overview

This document describes the data parser layer implemented for the Umbrella Widget to handle missing, null, or undefined API values with appropriate fallbacks, and to make the wind criterion optional in the umbrella recommendation logic.

## Problem Statement

### Previous Issues

1. **No Data Validation**: API values were used directly without checking for:
   - Null or missing values
   - NaN (Not a Number) in floating-point fields
   - Out-of-range values (e.g., negative wind speeds)
   - Invalid timestamps

2. **Mandatory Wind Criterion**: The original logic required wind data to make a decision:
   - If wind data was missing, the widget could behave unpredictably
   - No way to display "--" for missing wind values

3. **Inconsistent Display**: Missing values were not uniformly handled across the UI.

## Solution Architecture

### Core Components

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│   API Response  │────▶│ UmbrellaParser   │────▶│ drawUmbrella    │
│   (owm_hourly_t)│     │ (Validation +    │     │ Widget          │
│                 │     │  Fallback)       │     │ (Rendering)     │
└─────────────────┘     └──────────────────┘     └─────────────────┘
                              │
                              ▼
                        ┌──────────┐
                        │ Umbrella │
                        │  Data    │
                        │(Validated│
                        │  + State)│
                        └──────────┘
```

### 1. UmbrellaData Structure

A data structure that holds:
- **Validated numeric values** with explicit validity flags
- **Pre-formatted display strings** with "--" fallback
- **Derived state** (NO_RAIN, TAKE, TOO_WINDY, COMPACT)

```cpp
struct UmbrellaData {
  // Rain data with validity flag
  float maxPop;
  bool maxPopValid;
  
  // Wind data (optional) with validity flag
  float maxWindSpeed;
  bool windValid;
  String windDisplayStr;  // "15" or "--"
  String windUnitStr;     // "km/h" or "--"
  
  // Display strings with fallbacks
  String popDisplayStr;        // "75" or "--"
  String rainTimeDisplayStr;   // "14:30" or "--"
  
  // Derived state
  UmbrellaState state;
};
```

### 2. UmbrellaParser Class

**Responsibilities:**
- Validate API data (timestamps, POP ranges, wind speeds)
- Apply fallback values ("--" for strings, flags for numbers)
- Calculate derived values (POP percentage, minutes until rain)
- Determine final umbrella state with optional wind

**Key Method:**
```cpp
static UmbrellaData parse(const owm_hourly_t *hourly, 
                          int hours,
                          int64_t current_dt,
                          const char* rainTimeStr);
```

## Wind-Optional Logic

### Decision Flow

```
                    ┌─────────────────┐
                    │   Start         │
                    └────────┬────────┘
                             ▼
                    ┌─────────────────┐
                    │ maxPop < 0.20f ?│
                    │ (valid POP)     │
                    └────────┬────────┘
                             │
              ┌──────────────┴──────────────┐
              │ Yes                         │ No
              ▼                             ▼
     ┌─────────────────┐          ┌─────────────────┐
     │ UMBRELLA_NO_RAIN│          │ maxPop >= 0.70f?│
     │ (X over icon)   │          │                 │
     └─────────────────┘          └────────┬────────┘
                                           │
                            ┌─────────────┴─────────────┐
                            │ Yes                       │ No
                            ▼                           ▼
                   ┌─────────────────┐       ┌─────────────────────┐
                   │ UMBRELLA_TAKE   │       │ Wind data valid?    │
                   │ (Open umbrella) │       │ AND >= 18 km/h?     │
                   └─────────────────┘       └──────────┬──────────┘
                                                        │
                                         ┌──────────────┼──────────────┐
                                         │ Yes          │ No           │ (no wind data)
                                         ▼              ▼              ▼
                                ┌─────────────────┐ ┌─────────────────┐
                                │ UMBRELLA_TOO_   │ │ UMBRELLA_COMPACT│
                                │ WINDY           │ │ (Closed umbrella│
                                │ (Too windy)     │ │ no wind check)  │
                                └─────────────────┘ └─────────────────┘
```

### Wind Optional Behavior

| Scenario | Wind Data | Result |
|----------|-----------|--------|
| POP 20-69% + Wind >= 18 km/h | Valid | TOO_WINDY |
| POP 20-69% + Wind < 18 km/h | Valid | COMPACT |
| POP 20-69% + No wind data | Invalid/Missing | COMPACT (ignores wind) |
| POP 20-69% + Wind = NaN | Invalid | COMPACT (ignores wind) |

## Fallback Values

### Display Fallbacks

| Field | Valid Value Example | Fallback Value |
|-------|--------------------|----------------|
| POP percentage | "75" | "--" |
| Wind speed | "15" | "--" |
| Wind unit | "km/h" | "--" |
| Rain time | "14:30" | "--" |

### Validation Rules

**Timestamp Validation:**
```cpp
// Must be after Jan 1, 2020
if (hourly.dt < 1577836800) return false;
```

**POP Validation:**
```cpp
// Must be in range [0.0, 1.0] and not NaN
if (std::isnan(pop) || pop < 0.0f || pop > 1.0f) return false;
```

**Wind Speed Validation:**
```cpp
// Must be: not NaN, not negative, not unreasonably high (>100 m/s)
if (std::isnan(wind) || wind < 0.0f || wind > 100.0f) return false;
```

## Files Changed

| File | Changes |
|------|---------|
| `include/umbrella_parser.h` | New header with UmbrellaData struct and UmbrellaParser class |
| `src/umbrella_parser.cpp` | New implementation with validation and wind-optional logic |
| `src/renderer.cpp` | Refactored `drawUmbrellaWidget` to use UmbrellaParser |

## Migration Guide

### For Developers

**Old Pattern (Direct API access):**
```cpp
// Fragile - no validation
float maxPop = 0.0f;
for (int i = 0; i < hours; i++) {
  if (hourly[i].pop > maxPop) {
    maxPop = hourly[i].pop;  // Could be NaN!
  }
}
```

**New Pattern (Using Parser):**
```cpp
// Robust - validated with fallbacks
UmbrellaData data = UmbrellaParser::parse(hourly, hours, current_dt, rainTimeStr);
if (data.maxPopValid) {
  // Safe to use data.maxPop
}
// data.popDisplayStr is always safe ("--" if invalid)
```

### State Handling

**Old Pattern (Manual thresholds):**
```cpp
if (maxPop < 0.20f) {
  // No rain
} else if (maxPop >= 0.70f) {
  // Take umbrella
} else if (wind >= 18.0f) {  // Wind required!
  // Too windy
} else {
  // Compact
}
```

**New Pattern (Parser state):**
```cpp
switch (data.state) {
  case UMBRELLA_NO_RAIN:
    // No rain
    break;
  case UMBRELLA_TAKE:
    // Take umbrella
    break;
  case UMBRELLA_TOO_WINDY:
    // Too windy (only if wind valid)
    break;
  case UMBRELLA_COMPACT:
    // Compact (default for intermediate POP)
    break;
}
```

## Testing Scenarios

### Test Cases

1. **Missing POP data** → Should display "--" and default to NO_RAIN
2. **POP = 0.15 (valid, no rain)** → NO_RAIN state
3. **POP = 0.50 + Wind = 25 km/h** → TOO_WINDY state
4. **POP = 0.50 + Wind = NaN** → COMPACT state (ignores wind)
5. **POP = 0.50 + No wind field** → COMPACT state (ignores wind)
6. **POP = 0.75 + Any wind** → TAKE state (POP >= 70%)
7. **Negative wind speed** → Wind marked invalid, COMPACT state
8. **Future timestamp only** → Past hours correctly skipped

### Verification Steps

1. Check that "--" appears when API data is missing
2. Verify wind < 18 km/h with POP 20-69% shows COMPACT
3. Verify missing wind with POP 20-69% shows COMPACT (not TOO_WINDY)
4. Verify POP >= 70% always shows TAKE regardless of wind
5. Verify POP < 20% always shows NO_RAIN regardless of wind

## Constants Reference

All thresholds match the existing `renderer.cpp` values:

| Constant | Value | Description |
|----------|-------|-------------|
| `POP_THRESHOLD_NO_RAIN` | 0.20f | Below this: no rain needed |
| `POP_THRESHOLD_TAKE` | 0.70f | Above this: take umbrella |
| `POP_THRESHOLD_MIN_RAIN` | 0.20f | Minimum POP to consider as rain event |
| `WIND_THRESHOLD_KMH` | 18.0f | Wind speed threshold for "too windy" |
| `MAX_ANALYSIS_HOURS` | 24 | Maximum hours to analyze |

## Benefits

1. **Resilience**: Widget displays gracefully even with partial API data
2. **Consistency**: All missing values show "--" uniformly
3. **Flexibility**: Wind data optional - widget works without it
4. **Testability**: Parser can be unit tested independently
5. **Maintainability**: Validation logic centralized in one place

---

**Related Files:**
- `include/umbrella_parser.h`
- `src/umbrella_parser.cpp`
- `src/renderer.cpp` (drawUmbrellaWidget function)
