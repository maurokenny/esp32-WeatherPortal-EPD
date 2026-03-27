# ESP32 Weather E-Paper Display Project

## Project Overview

This is an ESP32-based weather station that displays weather information on an e-paper (e-ink) display. The device fetches weather data from the Open-Meteo API (free, no API key required) and displays current conditions, hourly outlook graphs, daily forecasts, air quality index, and indoor temperature/humidity readings.

Key features:
- E-paper display (7.5" 800x480px or 640x384px) with low power consumption
- Deep sleep mode for battery operation (< 11μA during sleep)
- Indoor environment sensing (BME280 or BME680)
- Battery monitoring with multiple protection levels
- Multiple locale and unit system support
- HTTPS support with certificate verification
- No API key required (uses Open-Meteo free weather API)

## Technology Stack

- **Platform**: ESP32 (Expressif32 platform v6.12.0)
- **Framework**: Arduino Framework
- **Build System**: PlatformIO
- **Language**: C++17 (GNU++17)
- **Target Board**: DFRobot FireBeetle 2 ESP32-E (default), also supports FireBeetle ESP32

### External Libraries

| Library | Version | Purpose |
|---------|---------|---------|
| GxEPD2 | 1.6.5 | E-paper display driver |
| Adafruit BME280 | 2.3.0 | Temperature/humidity/pressure sensor |
| Adafruit BME680 | 2.0.5 | Temperature/humidity/pressure/VOC sensor |
| Adafruit BusIO | 1.17.4 | I2C/SPI bus communication |
| Adafruit Unified Sensor | 1.1.15 | Sensor abstraction layer |
| ArduinoJson | 7.4.2 | JSON parsing for API responses |

## Project Structure

```
platformio/
├── platformio.ini          # PlatformIO project configuration
├── src/                    # Source code
│   ├── main.cpp           # Application entry point and main logic
│   ├── config.cpp         # Runtime configuration constants
│   ├── api_response.cpp   # API response parsing (Open-Meteo)
│   ├── client_utils.cpp   # WiFi and HTTP client utilities
│   ├── renderer.cpp       # Display rendering functions
│   ├── display_utils.cpp  # Display helper functions
│   ├── conversions.cpp    # Unit conversion utilities
│   ├── locale.cpp         # Locale-specific formatting
│   ├── _strftime.cpp      # Custom strftime implementation
│   └── test_gxepd2.cpp    # Display test utilities
├── include/               # Header files
│   ├── config.h          # Compile-time configuration and feature flags
│   ├── api_response.h    # API response data structures
│   ├── renderer.h        # Display rendering declarations
│   ├── client_utils.h    # WiFi/HTTP utility declarations
│   ├── _locale.h         # Localization strings and AQI scales
│   ├── _strftime.h       # Time formatting declarations
│   ├── conversions.h     # Unit conversion declarations
│   ├── display_utils.h   # Display utility declarations
│   ├── cert.h            # SSL certificates for HTTPS
│   ├── wifi_credentials.h # WiFi credentials (auto-generated from .env)
│   ├── saved_api_response.h # Saved API response for offline testing
│   ├── test_gxepd2.h     # Display test declarations
│   └── locales/          # Locale definition files (*.inc)
│       ├── locale_de_DE.inc
│       ├── locale_en_GB.inc
│       ├── locale_en_US.inc
│       ├── locale_es_ES.inc
│       ├── locale_et_EE.inc
│       ├── locale_fi_FI.inc
│       ├── locale_fr_FR.inc
│       ├── locale_it_IT.inc
│       ├── locale_nl_BE.inc
│       └── locale_pt_BR.inc
├── lib/                  # Private libraries
│   └── esp32-weather-epd-assets/
│       ├── fonts/        # Font files (multiple typefaces, multiple sizes)
│       └── icons/        # Weather and UI icons (multiple resolutions)
├── docs/                 # Documentation
│   └── EPAPER_HORIZONTAL_LINES_BUG.md  # Known e-paper display issue
├── .env                  # WiFi credentials (not versioned)
├── load_env.py           # Script to generate wifi_credentials.h from .env
└── .vscode/              # VS Code settings
```

## Code Organization

### Core Modules

1. **main.cpp**: Application entry point. Contains:
   - `setup()`: WiFi connection, time sync, API requests, sensor reading, rendering
   - `beginDeepSleep()`: Power management and sleep scheduling
   - `fillMockupData()`: Test data for development without WiFi

2. **config.h / config.cpp**: Configuration management
   - Hardware pin definitions
   - Feature flags (display type, sensor type, units, etc.)
   - API endpoints and credentials
   - Battery voltage thresholds
   - Compile-time validation of configuration

3. **renderer.cpp / renderer.h**: Display rendering
   - E-paper display initialization and control
   - Drawing functions for weather widgets
   - Font and icon rendering
   - Layout management for different display sizes

4. **api_response.cpp / api_response.h**: Data structures and parsing
   - Open-Meteo API response structures
   - WMO weather code to OpenWeatherMap-compatible conversion
   - JSON deserialization using ArduinoJson
   - Support for loading saved API responses from header file

5. **client_utils.cpp / client_utils.h**: Network utilities
   - WiFi connection management
   - NTP time synchronization
   - HTTP/HTTPS API requests to Open-Meteo

### Display Types Supported

- `DISP_BW_V2`: 7.5" Black/White 800x480px (default)
- `DISP_BW_V2_ALT`: 7.5" Black/White 800x480px (alternative driver for FPC-C001 panels)
- `DISP_3C_B`: 7.5" Red/Black/White 800x480px
- `DISP_7C_F`: 7.3" 7-Color 800x480px
- `DISP_BW_V1`: 7.5" Black/White 640x384px (legacy)

### Driver Boards Supported

- `DRIVER_DESPI_C02`: Officially supported
- `DRIVER_WAVESHARE`: Supported with potential contrast issues

## Build Commands

### Build the Project
```bash
pio run
```

### Upload to Device
```bash
pio run --target upload
```

### Monitor Serial Output
```bash
pio device monitor --baud 115200
```

### Build for Specific Environment
```bash
# Default environment (dfrobot_firebeetle2_esp32e)
pio run -e dfrobot_firebeetle2_esp32e

# Legacy FireBeetle ESP32
pio run -e firebeetle32
```

### Clean Build
```bash
pio run --target clean
```

## Configuration Guide

### Essential Configuration Files

1. **include/config.h**: Compile-time configuration
   - Display type selection (`DISP_BW_V2`, `DISP_BW_V2_ALT`, `DISP_3C_B`, etc.)
   - Driver board selection (`DRIVER_WAVESHARE`, `DRIVER_DESPI_C02`)
   - Sensor type (`SENSOR_BME280` or `SENSOR_BME680`)
   - Locale (`LOCALE en_US`, `LOCALE fr_FR`, etc.)
   - Units (temperature, wind speed, pressure, distance)
   - HTTP mode (HTTP, HTTPS no verify, HTTPS with cert verify)
   - Feature toggles (alerts, battery monitoring, etc.)
   - Mockup data settings for testing

2. **src/config.cpp**: Runtime configuration
   - Pin definitions (modify for your wiring)
   - Location coordinates (latitude/longitude)
   - City name for display
   - Timezone string
   - Sleep duration and bedtime settings
   - Battery voltage thresholds

3. **.env**: WiFi credentials (alternative to hardcoding)
   ```
   WIFI_SSID=YourNetworkName
   WIFI_PASSWORD=YourPassword
   ```
   This file is processed by `load_env.py` to generate `include/wifi_credentials.h`

### Configuration Validation

The `config.h` file includes compile-time validation. If you select invalid combinations (e.g., multiple display types), the build will fail with a descriptive error message.

## Code Style Guidelines

### Copyright Headers
All files include GPL v3 license headers:
```cpp
/* Filename for esp32-weather-epd.
 * Copyright (C) 2022-2025  Luke Marzen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
```

### Comments
- Use `/* ... */` style for multi-line comments
- Use `//` for single-line comments

### Naming Conventions
- **Constants**: `UPPER_CASE` (e.g., `PIN_BAT_ADC`, `DISP_WIDTH`)
- **Types**: `snake_case_t` suffix (e.g., `owm_current_t`, `alignment_t`)
- **Functions**: `camelCase` (e.g., `beginDeepSleep`, `drawString`)
- **Variables**: `camelCase` (e.g., `batteryVoltage`, `timeInfo`)
- **Macros**: `UPPER_CASE` with underscores

### File Organization
- Headers in `include/`, implementations in `src/`
- One module per file (e.g., `renderer.cpp` + `renderer.h`)
- Locale data in separate `.inc` files under `include/locales/`

### Memory Management
- Large data structures are allocated statically (not on stack):
  ```cpp
  static owm_resp_onecall_t owm_onecall;
  static owm_resp_air_pollution_t owm_air_pollution;
  ```
- E-paper uses partial buffering for memory efficiency
- Deep sleep is used extensively to minimize power consumption

### Error Handling
- WiFi connection failures display error screens
- API errors show HTTP status codes and messages
- Sensor read failures display dashes ('-') for missing data
- Battery voltage monitoring with graduated responses

## Testing Strategies

### Mockup Data Mode

Set `USE_MOCKUP_DATA 1` in `config.h` to test without WiFi/API:
- Uses synthetic weather data
- Bypasses all network operations
- Useful for testing display layout and rendering
- Multiple weather scenarios available (sunny, rainy, snowy, cloudy, thunder)
- Rain widget states for testing umbrella display

### Loading Saved API Responses

Set `LOAD_API_FROM_HEADER 1` in `config.h` (requires `USE_MOCKUP_DATA 1`):
- Loads real captured API data from `include/saved_api_response.h`
- Allows testing with actual weather data without API calls
- Useful for offline development and debugging specific weather conditions

### Saving API Responses

Set `SAVE_API_RESPONSE_TO_HEADER 1` in `config.h`:
- Prints API response to Serial in header file format
- Copy output between markers to `include/saved_api_response.h`
- Useful for capturing real weather data for later testing

### Debug Levels

Set `DEBUG_LEVEL` in `config.h`:
- `0`: Basic status information (default)
- `1`: Increased verbosity, heap usage reporting
- `2`: Print full API responses to serial

### Serial Monitor

Connect at 115200 baud to view debug output:
```bash
pio device monitor --baud 115200
```

### Testing Display Without Weather API

1. Enable `USE_MOCKUP_DATA 1` in `config.h`
2. Select desired weather scenario with `MOCKUP_CURRENT_WEATHER`
3. Build and upload
4. The display will show synthetic weather data

## Security Considerations

### HTTPS Certificate Verification

The project supports three HTTP modes:
1. `USE_HTTP`: No encryption (not recommended for production)
2. `USE_HTTPS_NO_CERT_VERIF`: Encryption without authentication (vulnerable to MITM)
3. `USE_HTTPS_WITH_CERT_VERIF`: Full encryption + authentication (recommended)

**Important**: When using `USE_HTTPS_WITH_CERT_VERIF`, the SSL certificate is embedded in `include/cert.h`. Certificates expire and must be updated periodically.

### Credential Storage

- WiFi credentials are loaded from `.env` file by `load_env.py`
- The generated `include/wifi_credentials.h` should NOT be committed to version control
- Both `.env` and `wifi_credentials.h` are in `.gitignore`

### Battery Safety

- Battery monitoring prevents deep discharge
- Multiple voltage thresholds trigger progressively longer sleep intervals
- Critical low voltage triggers indefinite hibernation (requires manual reset)

## Deployment Process

### Hardware Requirements

1. **Microcontroller**: FireBeetle 2 ESP32-E (recommended) or FireBeetle ESP32
2. **Display**: 7.5" e-paper panel with compatible driver board
3. **Sensor**: BME280 or BME680 (I2C)
4. **Power**: 3.7V LiPo battery or USB power
5. **Optional**: Battery voltage divider circuit for monitoring

### Wiring Reference (Default Configuration)

| Function | Pin |
|----------|-----|
| Battery ADC | A2 |
| EPD Busy | 25 |
| EPD CS | 15 |
| EPD RST | 26 |
| EPD DC | 27 |
| EPD SCK | 13 |
| EPD MOSI | 14 |
| BME SDA | 21 |
| BME SCL | 22 |

### Flashing Steps

1. Create `.env` file with your WiFi credentials
2. Configure `src/config.cpp` with your location coordinates
3. Configure `include/config.h` for your hardware (display type, sensor)
4. Build and upload: `pio run --target upload`
5. Monitor serial output: `pio device monitor`

### First Run

1. The device will connect to WiFi
2. Synchronize time via NTP
3. Fetch weather data from Open-Meteo
4. Render display
5. Enter deep sleep

### Battery Operation

- Set `USING_BATTERY 1` in `config.h` to enable low-voltage protection
- Set `BATTERY_MONITORING 1` to enable voltage monitoring
- Adjust voltage thresholds in `config.cpp` for your battery

## Known Issues

### E-Paper Horizontal Lines Bug

See `docs/EPAPER_HORIZONTAL_LINES_BUG.md` for details.

When drawing precipitation bars using a horizontal dotted pattern (`y += 2` row skipping), the e-paper display renders as a completely white screen. However, a vertical dotted pattern (`x += 2` column skipping) works correctly.

**Workaround**: Use vertical dotted patterns or solid fills instead of horizontal row skipping.

## Common Issues

### BME Sensor Not Detected
- Check I2C wiring (SDA/SCL)
- Verify `BME_ADDRESS` (0x76 or 0x77 depending on SDO pin)
- Try adding `SENSOR_INIT_DELAY_MS 300` in `config.h`

### Display Not Updating
- Check SPI wiring
- Verify correct display type in `config.h`
- Check BUSY pin connection

### WiFi Connection Fails
- Verify credentials in `.env` file
- Check WiFi signal strength
- Verify `WIFI_TIMEOUT` is sufficient

### Time Sync Fails
- Verify `TIMEZONE` string is correct
- Increase `NTP_TIMEOUT`
- Try different NTP servers

## Useful Resources

- [PlatformIO Documentation](https://docs.platformio.org/)
- [Open-Meteo API](https://open-meteo.com/)
- [ESP32 Deep Sleep](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/sleep_modes.html)
- [GxEPD2 Library](https://github.com/ZinggJM/GxEPD2)
- [POSIX Timezone Strings](https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv)
