# Umbrella Widget

## Overview

The Umbrella Widget provides recommendations on whether to take an umbrella based on precipitation probability (POP) and wind conditions for the next 12 hours.

## Decision Logic

The widget analyzes hourly forecast data and makes a decision based on two factors:

1. **Maximum POP** (Probability of Precipitation) in the forecast window
2. **Maximum Wind Speed** in the forecast window

### Decision Matrix

| POP Range | Wind Speed | Recommendation | Display Icon | Rationale |
|-----------|------------|----------------|--------------|-----------|
| < 30% | Any | No rain expected | Umbrella with X | Low probability, no need |
| 30-69% | < 18 km/h | Take umbrella (closed) | Closed umbrella | Rain likely, calm wind |
| 30-69% | ≥ 18 km/h | Take umbrella (open) | Open umbrella | Rain + strong wind = use immediately |
| ≥ 70% | Any | Take umbrella (open) | Open umbrella | High probability, definitely needed |

### Wind Threshold

```cpp
const float WIND_THRESHOLD_KMH = 18.0f;  // ~10 m/s
```

When wind exceeds this threshold with moderate rain probability, the recommendation changes from "carry closed" to "use open" because:
- Strong wind makes opening an umbrella difficult
- Better to have it ready/usable immediately

## Data Validation

The parser validates API data before use:

| Check | Invalid Value Handling |
|-------|----------------------|
| Null wind speed | Displays "--" |
| Negative wind speed | Clamped to 0 |
| NaN POP | Treated as 0% |
| Missing hourly data | Skips that hour |

## Visual States

### State 1: No Rain Needed (POP < 30%)
- Icon: Umbrella with X mark overlay
- Text: "No rain (X%)" or rain start time if within window

### State 2: Take Closed (30-69% POP, calm wind)
- Icon: Closed umbrella
- Text: "Compact (X%)" or "Rain in Y min"

### State 3: Take Open (≥70% POP OR strong wind)
- Icon: Open umbrella
- Text: "Take (X%)" or "Rain in Y min"

## Code Implementation

Key function: `drawUmbrellaWidget()` in `src/renderer.cpp`

```cpp
// Pseudocode of core logic
float maxPop = 0;
float maxWindMs = 0;

// Scan next 12 hours
for (int i = 0; i < 12; i++) {
    maxPop = max(maxPop, hourly[i].pop);
    maxWindMs = max(maxWindMs, hourly[i].wind_speed);
}

float maxWindKmh = maxWindMs * 3.6f;

// Decision
if (maxPop < 0.30f) {
    state = NO_RAIN;
} else if (maxPop >= 0.70f) {
    state = TAKE_OPEN;
} else if (maxWindKmh >= 18.0f) {
    state = TAKE_OPEN;  // Strong wind = use immediately
} else {
    state = TAKE_CLOSED;
}
```

## Configuration

No compile-time configuration needed. All thresholds are constants in the renderer module.
