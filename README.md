# ESP32 E-Paper Weather Display

A low-power weather display powered by an ESP32 and a 7.5" e-paper panel. It features a modern web-based setup experience and uses **Open-Meteo** for free weather data (no API key required for personal use).

<p float="left">
  <img src="showcase/assembled-demo-raleigh-front.jpg" />
  <img src="showcase/assembled-demo-raleigh-side.jpg" width="49%" />
  <img src="showcase/assembled-demo-raleigh-back.jpg" width="49%" />
</p>

---

## Features

* **No API Key Required** – Powered by Open-Meteo (free for personal, non-commercial use)
* **Easy Setup** – Captive portal with automatic mobile detection
* **QR Code Onboarding** – Connect instantly by scanning from the display
* **Auto Location Detection** – Optional IP-based geolocation
* **Umbrella Indicator** – Quick visual cue for rain probability
* **Visual Error Feedback** – Distinct icons for WiFi, time sync, API, and battery errors
* **Multi-Display Support** – Works with multiple Waveshare / Good Display 7.5" panels
* **Offline Development Mode** – Full simulation without WiFi or API calls

> **Power Note**
> This project currently targets the **E-Paper ESP32 Driver Board Rev 3 (USB-powered)**.
> Deep sleep consumption is higher than ultra-low-power builds due to onboard components. Battery optimization has not yet been implemented.

---

## Requirements

### Hardware
- ESP32 Dev Module or FireBeetle ESP32
- 7.5" e-paper display (Waveshare or Good Display compatible)
- Compatible driver board (DESPI-C02 or Waveshare)
- Optional: BME280/BME680 sensor for indoor readings

### Software
- PlatformIO Core 6.0+ (with Arduino framework)
- Python 3.8+ (for build scripts)
- Git

### Dependencies
All dependencies are managed automatically by PlatformIO:
- GxEPD2 (e-paper display driver)
- ArduinoJson (JSON parsing)
- ESPAsyncWebServer (web portal)
- Adafruit sensor libraries

---

## Quick Start

### 1. Build & Flash

```bash
cd platformio

pio run
pio run --target upload
pio device monitor --baud 115200
```

### 2. First Boot

On first boot, the device creates a Wi-Fi access point for configuration.

---

## Setup

### Web Portal (Recommended)

1. **Power on the device**
2. Scan the QR code on the display
   *or connect manually to:*
   ```
   weather_eink-AP
   ```

3. Open the setup page:
   * http://weather.local
   * http://192.168.4.1

4. Configure:
   * Wi-Fi credentials
   * Location (or enable auto-detection)

5. Save → device restarts and begins updating weather

> ⚠️ **Security Notice**
> Setup runs over **unencrypted HTTP**.
> Perform configuration on a trusted network.

---

### Alternative Configuration

#### Manual Defaults (Firmware)

Edit:

```
platformio/src/config.cpp
```

Used for:
* Default values in the portal
* Fallback when configuration is missing

---

#### Environment Variables

Create:

```
.env
```

```env
WIFI_SSID=your_wifi
WIFI_PASSWORD=your_password
```

Loaded at build time via `platformio/load_env.py`. **Optional** - if not provided, fallback values are used and WiFi can be configured via the web portal.

---

## Hardware

Current development setup:

* **ESP32 Board**
  E-Paper ESP32 Driver Board Rev 3

* **Display**
  Waveshare 7.5" e-paper (800×480, Black/White)

* **Adapter**
  Waveshare e-Paper adapter

* **Power**
  USB-powered

---

## State Machine

The firmware uses a deterministic state machine to manage WiFi connectivity, configuration, and error recovery with persistent state across deep sleep. Transitions depend on timeouts and per-subsystem failure counters (WiFi, NTP, API). RTC memory preserves state across deep sleep cycles.

```
┌─────────┐    ┌─────────────────┐    ┌─────────────┐
│  BOOT   │───▶│ STATE_WIFI_CONNECTING │───▶│ STATE_NORMAL_MODE │
└─────────┘    └─────────────────┘    └─────────────┘
     │                │                      │
     │                ▼                      ▼
     │         ┌──────────────────┐        ┌──────────────────┐
     └────────▶│ STATE_AP_CONFIG_MODE │        │ STATE_SLEEP_PENDING│
                └──────────────────┘        └──────────────────┘
                       │                        │
                       ▼                        ▼
                ┌──────────────┐         ┌──────────────┐
                │ STATE_ERROR  │         │ Deep Sleep   │
                └──────────────┘         └──────────────┘
```

**Key behaviors:**
- **First boot**: AP configuration mode (captive portal) is available when ramSSID is empty (no credentials) or if WiFi connection fails during the first boot.
- **Post-first-boot**: WiFi failures follow a retry cycle; AP mode is not entered after a successful WiFi connection.
- **Error screens**: Distinct icons for WiFi, NTP, API, and Battery issues.
- **Deep sleep**: State persists in RTC memory across sleep cycles

📖 **Full documentation**: See [`docs/STATE_MACHINE.md`](docs/STATE_MACHINE.md) for complete state diagrams, failure cycle logic, and troubleshooting.

---

## Development & Testing

### Offline Mode (No WiFi)

Enable in `config.h`:

```c
#define USE_MOCKUP_DATA 1
```

Simulates full device lifecycle:

* Boot → WiFi → Fetch → Display → Sleep
* Deterministic timing (no network dependency)

Available scenarios:

* Weather: SUNNY, RAINY, SNOWY, CLOUDY, THUNDER
* Rain widget: NO_RAIN, COMPACT, TAKE, GRAPH_TEST

---

### Using Saved API Data

Skip live API calls and replay real responses:

1. Download data:
```bash
curl "https://api.open-meteo.com/..." -o api_response.json
```

2. Convert:
```bash
python json_to_header.py api_response.json
```

3. Enable:
```c
#define USE_SAVED_API_DATA 1
```

---

## Troubleshooting

| Issue | Fix |
|------|-----|
| Display not updating | Check SPI wiring and BUSY pin |
| White/washed display | Known panel limitation (pattern workaround applied) |
| WiFi fails | Verify credentials and signal strength |
| Time sync issues | Check timezone and NTP settings |

For deeper debugging, see `platformio/AGENTS.md`.

---

## Known Limitations

| Feature | Status | Notes |
|---------|--------|-------|
| **Weather Alerts** | ❌ Not implemented | Alert parsing from Open-Meteo is not yet implemented. The display infrastructure exists but the data pipeline is incomplete. |

---

## Project Background

This project is a significantly reworked fork of the excellent [esp32-weather-epd](https://github.com/lmarzen/esp32-weather-epd) by Luke Marzen.

### Key Changes

* Migrated to **Open-Meteo** (no API key required)
* Added web-based setup portal
* Introduced state machine architecture
* Implemented umbrella indicator
* Fixed display rendering issues on certain panels

---

## Credits & Acknowledgments

* Fork of [esp32-weather-epd](https://github.com/lmarzen/esp32-weather-epd) by **Luke Marzen**
* Libraries: GxEPD2, ArduinoJson, Adafruit ecosystem
* PlatformIO community

---

## License

Licensed under **GPL v3.0**.
See original project for full attribution of third-party assets.
