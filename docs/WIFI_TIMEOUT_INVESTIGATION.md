# WiFi Timeout and State Machine Investigation

## Overview

This document summarizes the investigation of WiFi connection timeout behavior and terminal error state handling in the ESP32 Weather Portal firmware.

The investigation found that the device can enter a permanent `STATE_ERROR` terminal state when WiFi connection attempts repeatedly exceed the configured timeout and failure count, even if the wireless environment is only temporarily slow.

## Findings

### Timeout behavior

- The firmware uses two timeout mechanisms:
  - `platformio/src/config.cpp` defines `WIFI_TIMEOUT` for legacy WiFi connection paths.
  - `platformio/src/wifi_manager.cpp` defines `wifiConfig.wifiConnectTimeout` for the non-blocking WiFi state machine.
- Both values were effectively 20 seconds.
- This was too aggressive for some real-world WiFi environments where association, DHCP, or AP handshakes can take longer.

### State machine behavior

- The non-blocking state machine lives in `wifi_manager.cpp` and `state_decision.cpp`.
- On each loop, the state machine checks:
  - `WiFi.status() == WL_CONNECTED`
  - whether `millis() - runtime.wifiStartTime > wifiConfig.wifiConnectTimeout * 1000`
- If a timeout occurs in `STATE_WIFI_CONNECTING`:
  - first boot devices go to `STATE_AP_CONFIG_MODE`
  - subsequent boot devices increment `connectionFailCycles`
  - if `connectionFailCycles >= MAX_WIFI_FAIL_CYCLES`, the device enters `STATE_ERROR`

### Error handling problems

- The device can show a generic error screen on `STATE_ERROR` even when a more specific failure screen was already drawn.
- The terminal state display logic in `main.cpp` could overwrite a failure-specific screen with the generic "device took too long to connect" message.

### Root cause of the observed issue

- The observed message "timeout, demorou muito tempo para se conectar, reinicie o sistema" is consistent with `STATE_ERROR` triggered by repeated WiFi timeouts.
- Real-world WiFi environments with slow association or DHCP can easily exceed 20 seconds.
- The aggressive timeout made the device more likely to count a valid but slow connection attempt as a failure.

## Fixes applied

### Timeout increase

Updated timeout values to 60 seconds in both locations:

- `platformio/src/config.cpp`
  - `WIFI_TIMEOUT` changed from `20000` ms to `60000` ms
- `platformio/src/wifi_manager.cpp`
  - `wifiConfig.wifiConnectTimeout` changed from `20` seconds to `60` seconds

This makes the WiFi connection window more tolerant of slow AP responses and DHCP delays.

### Documentation

A new investigation document was added in `docs/WIFI_TIMEOUT_INVESTIGATION.md` describing the state machine, root cause, and fix.

## Recommended next steps

- Evaluate whether `MAX_WIFI_FAIL_CYCLES` should be reduced, increased, or made configurable.
- Consider introducing an exponential backoff or a longer first-attempt timeout separate from later retries.
- Add telemetry logs for:
  - `runtime.wifiStartTime`
  - `WiFi.status()` during `STATE_WIFI_CONNECTING`
  - `connectionFailCycles`

## Files changed

- `platformio/src/config.cpp`
- `platformio/src/wifi_manager.cpp`
- `docs/WIFI_TIMEOUT_INVESTIGATION.md`
