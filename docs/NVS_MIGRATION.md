# NVS Migration: WiFi Config from RTC RAM to NVS

## Commit

`02fd02ba` on branch `features/nvs_migration`

```
refactor: migrate WiFi config from RTC RAM to NVS, eliminate automatic AP mode
```

---

## Motivation

### The Bug

After days of normal operation, the device would spontaneously enter AP configuration
mode. The root cause was a **WDT (Watchdog Timer) reset during deep sleep entry** that
caused RTC RAM to be re-initialized on wake. This set `isFirstBoot = true`, and on the
first subsequent WiFi timeout the device entered AP mode — even though valid credentials
had been working for days.

### The Fix

- **RTC RAM** is not guaranteed to survive WDT resets, brownouts, or power glitches
- **NVS (Non-Volatile Storage)** survives everything: deep sleep, WDT, brownout, power cycle
- Move all configuration to NVS as the single source of truth
- Remove `isFirstBoot` entirely — no RTC-based heuristic for AP mode decision
- AP mode is entered **only** via explicit user action (GPIO0 button) or when NVS is empty

---

## Core Architecture Change

### Before

```
.env (compile-time)  ──→  RTC RAM  ──→  WiFi.begin(ramSSID, ramPassword)
                               │
                        isFirstBoot flag
                        (lost on WDT/power glitch)
```

### After

```
.env (compile-time)  ──→  NVS flash  ──→  WiFi.begin(configStore.ssid(), ...)
                               │
                        Preference API
                        (survives everything)

RTC RAM (legacy, read-only): failure counters only
  - connectionFailCycles
  - ntpFailCycles
  - apiFailCycles
  - isErrorState
```

---

## ConfigStore Class

New class in `include/wifi_manager.h` / `src/wifi_manager.cpp`.

**Namespace**: `"device"` (NVS namespace)
**Keys**:

| Key       | Type      | Content                          |
|-----------|-----------|----------------------------------|
| `ssid`    | String    | WiFi SSID (max 32 chars)         |
| `pass`    | String    | WiFi password (max 63 chars)     |
| `lat`     | String    | Latitude (max 20 chars)          |
| `lon`     | String    | Longitude (max 20 chars)         |
| `city`    | String    | City name (max 63 chars)         |
| `country` | String    | Country name (max 63 chars)      |
| `tz`      | String    | POSIX timezone (max 63 chars)    |
| `autoGeo` | Bool      | Auto-geolocation flag            |
| `tzMode`  | UChar     | TIMEZONE_MODE_AUTO (0) or MANUAL (1) |
| `prov`    | Bool      | Provisioned flag (loading screen suppression) |

**API**:

```cpp
configStore.loadFromNVS();          // Load all values into RAM buffers
configStore.saveToNVS();            // Persist all buffers to flash
configStore.hasValidWifiConfig();   // ssid non-empty → valid
configStore.migrateFromRtcIfNeeded(); // One-time copy from RTC→NVS
configStore.ssid();                 // Getter (returns const char*)
configStore.password();             // Getter
configStore.lat(), lon(), ...       // Getters for all fields
configStore.setWifiConfig(...);     // Set SSID + password
configStore.setLocation(...);       // Set lat/lon/city/country/tz
configStore.setAutoGeo(...);        // Set auto-geo flag
configStore.setTimezoneMode(...);   // Set timezone mode
configStore.provisioned();          // true after first successful weather cycle
configStore.setProvisioned(bool);   // Reset (e.g. on user re-config via portal)
```

**NVS open/close pattern**: Opens for read in `loadFromNVS()`, for write in `saveToNVS()`,
closes immediately after each operation. No persistent open handle.

---

## State Machine Changes

### Before

```
BOOT ──→ WIFI_CONNECTING ──→ AP_CONFIG_MODE (isFirstBoot + timeout)
  │                              │
  └──→ AP_CONFIG_MODE (no creds) └──→ ERROR (portal timeout)
```

Two ways to enter AP mode:
1. No credentials at boot (`ramSSID` empty)
2. WiFi timeout **AND** `isFirstBoot == true`

### After

```
CHECK_CONFIG ──→ BOOTSTRAP ──→ AP_CONFIG_MODE (only if no .env)
     │               │
     │               └──→ CHECK_CONFIG (if .env succeeded)
     │
     ├──→ AP_CONFIG_MODE (button pressed)
     ├──→ WIFI_CONNECTING (NVS valid)
     └──→ BOOTSTRAP (NVS empty)

WIFI_CONNECTING ──→ SLEEP_PENDING (normal retry)
                 └──→ ERROR (max failures)
                 ❌ NEVER → AP_CONFIG_MODE
```

Two ways to enter AP mode:
1. **GPIO0 button** held for `AP_MODE_HOLD_MS` (1500ms) after boot
2. **NVS empty + no .env** (bootstrap falls through to AP mode)

WiFi timeout **never** enters AP mode — it always goes to `SLEEP_PENDING`
or `STATE_ERROR`.

**AP mode after config save**: Goes to `STATE_CHECK_CONFIG` (not `STATE_BOOT`),
which re-evaluates NVS and either connects or enters AP mode again if save failed.

---

## New States

### `STATE_CHECK_CONFIG` (replaces `STATE_BOOT`)

Pure decision state — no initialization, no loading screens.

```
input:
  - apButtonPressed?  → AP_CONFIG_MODE
  - nvsValid?         → WIFI_CONNECTING
  - otherwise         → BOOTSTRAP
```

### `STATE_BOOTSTRAP` (new)

Tries to load factory defaults from `.env` into NVS if
`ALLOW_ENV_BOOTSTRAP_TO_NVS == 1`.

```
if .env had valid config and save succeeded:
    → STATE_CHECK_CONFIG (re-evaluate with populated NVS)
otherwise:
    → STATE_AP_CONFIG_MODE (user must provision)
```

---

## Removed Concepts

### `isFirstBoot` (removed entirely)

- Was a `bool` in RTC RAM, set to `true` on cold boot, `false` after first WiFi success
- **Problem**: RTC corruption could reset it to `true` at any time
- **Replaced by**: Nothing. No boot-relative logic exists. AP mode decisions are
  based solely on `nvsValid` and `apButtonPressed` at the moment of decision.

### `hasCredentials` (removed from DecisionInput)

- Was based on `ramSSID` length check at boot
- **Replaced by**: `nvsValid` — checks `configStore.hasValidWifiConfig()`
  which queries NVS directly

---

## AP Mode Entry: Refined Rules

| Scenario | Old Behavior | New Behavior |
|----------|-------------|--------------|
| Factory fresh (no config) | AP mode at boot | Bootstrap try, then AP mode |
| User holds GPIO0 button | N/A (no button) | AP mode |
| WiFi timeout, first boot | AP mode | Sleep + retry |
| WiFi timeout, normal | Sleep + retry | Sleep + retry (unchanged) |
| RTC/WDT corruption | AP mode (isFirstBoot=1) | Sleep + retry isFirstBoot |
| Config saved in portal | Reboot to BOOT | Reboot to CHECK_CONFIG |
| Portal timeout | ERROR (same) | ERROR (same) |

---

## GPIO0 Configuration Button

Added `readConfigButton()` called once in `wifiManagerSetup()`.

**Sequence**:
1. `pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP)` — GPIO0 has external pullup
2. `delay(500)` — wait for boot strapping to complete (GPIO0 is strapping pin)
3. Read pin. If HIGH → no button, return false
4. If LOW → wait for `AP_MODE_HOLD_MS` (1500ms) hold
5. If released early → return false (ignore brief glitches)
6. If held long enough → return true → `STATE_AP_CONFIG_MODE`

**Configuration**:

```cpp
#define CONFIG_BUTTON_PIN 0       // GPIO0 (BOOT button)
#define AP_MODE_HOLD_MS 1500      // 1.5 second hold
```

Note: GPIO0 is the strapping pin. It must be HIGH during reset to boot normally.
If the button is held during reset, the ESP32 enters download mode.
`readConfigButton()` reads the pin only after a 500ms delay — if the button
is held continuously from reset, the pin will still read LOW → AP mode triggered.

---

## Bootstrap from .env

New function `bootstrapFromEnv()` controlled by compile-time flag:

```cpp
#define ALLOW_ENV_BOOTSTRAP_TO_NVS 1   // default: enabled
```

When `STATE_BOOTSTRAP` runs:
1. If `ALLOW_ENV_BOOTSTRAP_TO_NVS == 0` → skip, fall through to AP mode
2. If `WIFI_SSID` macro is empty/null → skip, fall through to AP mode
3. Otherwise: write `.env` values into NVS via `configStore.saveToNVS()`
4. Return to `STATE_CHECK_CONFIG` to re-evaluate

This preserves the existing behavior where `.env` acts as a factory seed.
The difference: writing to NVS instead of RTC RAM, and it only runs when
NVS is empty (not on every cold boot).

---

## One-Time RTC → NVS Migration

In `wifiManagerSetup()`:

```cpp
configStore.loadFromNVS();
if (!configStore.hasValidWifiConfig()) {
    configStore.migrateFromRtcIfNeeded();
}
```

`migrateFromRtcIfNeeded()` checks if RTC RAM has valid SSID and copies
all fields to NVS. The legacy `ramSSID`, `ramPassword`, etc. variables
remain in RTC RAM as `RTC_DATA_ATTR` for this migration path.

After migration is confirmed stable, these legacy variables can be
removed in a future release.

---

## Key Files Changed

| File | Change Summary |
|------|---------------|
| `include/config.h` | Added `CONFIG_BUTTON_PIN`, `AP_MODE_HOLD_MS`, `NVS_NAMESPACE_DEVICE`, `ALLOW_ENV_BOOTSTRAP_TO_NVS` |
| `include/state_decision.h` | New enum: `STATE_CHECK_CONFIG`, `STATE_BOOTSTRAP`. New input: `nvsValid`, `apButtonPressed`. Removed: `isFirstBoot`, `hasCredentials` |
| `src/state_decision.cpp` | `STATE_BOOT` → `STATE_CHECK_CONFIG`. New `STATE_BOOTSTRAP`. WiFi timeout never returns AP mode. Only counter increment logic remains for WiFi failures |
| `include/wifi_manager.h` | New `ConfigStore` class (full declaration). `configStore` extern. `readConfigButton()`, `bootstrapFromEnv()` prototypes. Legacy RTC vars marked deprecated |
| `src/wifi_manager.cpp` | `STATE_CHECK_CONFIG` initial state. Full `ConfigStore` implementation. `readConfigButton()` implementation. `bootstrapFromEnv()`. `migrateFromRtcIfNeeded()`. `wifiManagerSetup()` loads from NVS. `wifiManagerLoop()` uses `configStore` |
| `src/wifi_manager_handlers.cpp` | Config save writes to NVS via `configStore.saveToNVS()` instead of RTC RAM. Error handling on NVS write failure |
| `src/client_utils.cpp` | `WiFi.begin()` uses `configStore.ssid()/password()`. API lat/lon from `configStore`. Timezone mode from `configStore` |
| `src/main.cpp` | Removed all RTC config loading (~60 lines). `updateWeather()` uses `configStore` for city, country, timezone, autoGeo. Removed `isFirstBoot` from loading screen condition |
| `src/time_coordinator.cpp` | Uses `configStore.timezoneMode()` and `configStore.timezone()` instead of `ramTimezoneMode`/`ramTimezone` |
| `src/api_response.cpp` | Uses `configStore.timezoneMode()` instead of `ramTimezoneMode` |
| `include/time_coordinator.h` | Comment updated |
| `include/client_utils.h` | Comment updated |
| `test/test_state_machine/test_state_machine.cpp` | 26 tests refactored: `STATE_BOOT` → `STATE_CHECK_CONFIG`, removed all `isFirstBoot` tests, added `STATE_BOOTSTRAP` tests, added regression test for "WiFi timeout never AP mode" |

---

## Configuration Variables (Before vs After)

### RTC RAM — Kept for failure counters

| Variable | Type | Purpose |
|----------|------|---------|
| `connectionFailCycles` | `uint8_t` | WiFi failures (unchanged) |
| `ntpFailCycles` | `uint8_t` | NTP failures (unchanged) |
| `apiFailCycles` | `uint8_t` | API failures (unchanged) |
| `isErrorState` | `bool` | Permanent error flag (unchanged) |

### RTC RAM — Legacy (deprecated, one-time migration)

| Variable | New Home |
|----------|----------|
| `ramSSID[33]` | NVS key `ssid` → `configStore.ssid()` |
| `ramPassword[64]` | NVS key `pass` → `configStore.password()` |
| `ramCity[64]` | NVS key `city` → `configStore.city()` |
| `ramCountry[64]` | NVS key `country` → `configStore.country()` |
| `ramLat[21]` | NVS key `lat` → `configStore.lat()` |
| `ramLon[21]` | NVS key `lon` → `configStore.lon()` |
| `ramTimezone[64]` | NVS key `tz` → `configStore.timezone()` |
| `ramAutoGeo` | NVS key `autoGeo` → `configStore.autoGeo()` |
| `ramTimezoneMode` | NVS key `tzMode` → `configStore.timezoneMode()` |
| `rtcInitialized` | Removed (not needed — NVS is always available) |

### RTC RAM — Removed entirely

| Variable | Reason |
|----------|--------|
| `isFirstBoot` | Root cause of the bug. No replacement. |

---

## Post-Migration Fixes

After the initial NVS migration (`02fd02ba`), five additional fixes were
identified through testing:

### 1. `provisioned` Flag (Commits `46ce6d50`, `36bfdda9`)

The initial migration removed `isFirstBoot` but did not account for **loading
screen suppression**. On the original code, loading screens ("Connecting to
Wi-Fi...", "Fetching weather...", "Wi-Fi Connected!") were gated by
`isFirstBoot && !SILENT_STATUS`. With `isFirstBoot` gone, these screens would
show on every boot — or, after a workaround, never show after the first cycle.

**Solution**: A new `provisioned` boolean was added to `ConfigStore`, persisted
in NVS via the `prov` key.

- **During `updateWeather()`**: After the first successful weather+display cycle,
  `setProvisioned(true)` + `saveToNVS()` is called. Subsequent boots skip the
  loading screens because `provisioned` is `true`.
- **In `handleConfigSave()`**: When the user re-configures via the AP portal,
  `setProvisioned(false)` is called **before** `saveToNVS()`. This ensures all
  loading screens appear on the next connection attempt — the user sees feedback
  that their new credentials are working.
- **Screen condition**: Every loading/status screen now checks
  `!configStore.provisioned() || !SILENT_STATUS`. With `SILENT_STATUS=true`,
  screens are shown only when `provisioned == false`.

This is more robust than `isFirstBoot` because NVS survives WDT, brownout,
and power cycle. After the first weather cycle, loading screens are permanently
suppressed — no risk of regressing to AP mode on a glitch.

### 2. `stopAP()` and WiFi Mode Reset (Commit `8a58a602`)

**Bug**: After saving config in AP mode, the state machine transitioned to
`STATE_CHECK_CONFIG` → `STATE_WIFI_CONNECTING` and called `WiFi.begin()`.
However, the ESP32 was still in **WiFi_AP** mode (softAP active, DNS + web
server running). `WiFi.begin()` called while in AP mode does not connect to
the station — the device appeared stuck on "Connecting to Wi-Fi...".

**Fix**: A `stopAP()` function was added that:
1. Stops the DNS server (`dnsServer.stop()`)
2. Ends the web server (`server.end()`)
3. Stops mDNS (`MDNS.end()`)
4. Disconnects softAP (`WiFi.softAPdisconnect(true)`)
5. Forces WiFi mode to station (`WiFi.mode(WIFI_STA)`)

`stopAP()` is called unconditionally on any transition out of
`STATE_AP_CONFIG_MODE`. Additionally, `WiFi.mode(WIFI_STA)` is called
immediately before `WiFi.begin()` in the `STATE_WIFI_CONNECTING` handler
as a defensive measure.

### 3. Button Event Consumption (Commit `0778c104`)

**Bug**: `g_apButtonPressed` was set once during boot via `readConfigButton()`
but never cleared. After saving config in AP mode, the state machine returned
to `STATE_CHECK_CONFIG`, saw `apButtonPressed=true`, and immediately re-entered
AP mode — creating an infinite loop.

**Fix**: The flag is reset to `false` in the `STATE_AP_CONFIG_MODE` handler,
before starting the AP. This ensures the button press is consumed exactly once
per boot.

### 4. Debug State Transition Logging (Commit `ec1887cd`)

A comprehensive `[STATE]` log line was added to every state transition:

```
[STATE] old->new | nvsValid=1 apButton=0 configSaved=1 portalTimeout=0
                   wifiConnected=0 wifiTimeout=0 | portalActive=0 apMode=0
                   | portalElapsed=0
```

This prints all `DecisionInput` fields plus runtime flags and elapsed portal
time, making it possible to diagnose unexpected transitions from serial logs
without a debugger.

### 5. Wakeup Source Error Fix (Commit `ec1887cd`)

**Bug**: The original code called
`esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TOUCHPAD)` and
`esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ULP)` in the deep sleep
path. These wakeup sources were never enabled, causing the ESP32 to log
`Incorrect wakeup source` errors.

**Fix**: Removed both calls. The only wakeup source that needs disabling is
`ESP_SLEEP_WAKEUP_TIMER`, which is still disabled for battery-critical or
terminal error states where indefinite sleep is required.

### 6. Button Reading Window (Commit `8a58a602`)

`readConfigButton()` was rewritten from a simple blocking read into a
**windowed polling** implementation:

```
delay(500)          — wait for strapping pin to stabilize
sample pin          — if HIGH, return false immediately
poll for 10 seconds — if pin released before AP_MODE_HOLD_MS, return false
                    — if held for AP_MODE_HOLD_MS (1500ms), return true
```

The polling window `BUTTON_READ_WINDOW_MS` was increased from 5000ms to
**10000ms** (10 seconds) and is now configurable in `config.h`. This gives
the user a wider window to press the button after power-on.

**Updated files** (for commits `36bfdda9` through `ec1887cd`):

| File | Change Summary |
|------|---------------|
| `include/wifi_manager.h` | Added `KEY_PROVISIONED = "prov"`, `provisioned_` member, `provisioned()` getter, `setProvisioned()` setter to `ConfigStore` class |
| `src/wifi_manager.cpp` | Added `provisioned_` initialization, NVS read/write of `prov` key; added `stopAP()` function; reset `g_apButtonPressed` in `STATE_AP_CONFIG_MODE` handler; call `stopAP()` on exit from AP mode; force `WiFi.mode(WIFI_STA)` before `WiFi.begin()`; gate "Connecting to Wi-Fi..." and "Wi-Fi Connected!" screens on `!configStore.provisioned() \|\| !SILENT_STATUS`; add comprehensive `[STATE]` transition logging |
| `src/wifi_manager_handlers.cpp` | Reset `provisioned` flag before `saveToNVS()` in `handleConfigSave()`; add `[PROVISION]` log on reset |
| `src/main.cpp` | Gate "Fetching weather..." screen on `!configStore.provisioned() \|\| !SILENT_STATUS`; call `setProvisioned(true)` + `saveToNVS()` after first successful weather cycle; remove spurious `esp_sleep_disable_wakeup_source(TOUCHPAD/ULP)` calls |
| `include/config.h` | Added `BUTTON_READ_WINDOW_MS` 10000ms define |
