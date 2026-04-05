# ESP32 Weather E-Paper Display Project

## Project Overview

This is an ESP32-based weather station that displays weather information on an e-paper (e-ink) display. The device fetches weather data from the Open-Meteo API (free, no API key required) and displays current conditions, hourly outlook graphs, daily forecasts, air quality index, and indoor temperature/humidity readings.

Key features:
- E-paper display (7.5" 800x480px or 640x384px) with low power consumption
- Deep sleep mode for battery operation (< 11μA during sleep)
- Indoor environment sensing (BME280 or BME680)
- Battery monitoring with multiple protection levels
- Multiple locale and unit system support
- WiFi Configuration Portal with captive portal detection
- Automatic IP-based geolocation
- HTTPS support with certificate verification
- No API key required (uses Open-Meteo free weather API)

## Technology Stack

- **Platform**: ESP32 (Expressif32 platform)
- **Framework**: Arduino Framework
- **Build System**: PlatformIO
- **Language**: C++17 (GNU++17)
- **Target Board**: ESP32 Dev Module (default), also supports FireBeetle ESP32

### External Libraries

| Library | Version | Purpose |
|---------|---------|---------|
| GxEPD2 | 1.6.5 | E-paper display driver |
| Adafruit BME280 | 2.3.0 | Temperature/humidity/pressure sensor |
| Adafruit BME680 | 2.0.5 | Temperature/humidity/pressure/VOC sensor |
| Adafruit BusIO | 1.17.4 | I2C/SPI bus communication |
| Adafruit Unified Sensor | 1.1.15 | Sensor abstraction layer |
| ArduinoJson | 7.4.2 | JSON parsing for API responses |
| Adafruit GFX Library | 1.11.9 | Graphics primitives |
| AsyncTCP | 3.2.4 | Async TCP library for web server |
| ESPAsyncWebServer | 3.3.17 | Async web server for configuration portal |
| QRCode | 0.0.1 | QR code generation for WiFi setup |

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
│   ├── wifi_manager.cpp   # WiFi connection and AP mode management
│   ├── _strftime.cpp      # Custom strftime implementation
│   └── test_gxepd2.cpp    # Display test utilities
├── include/               # Header files
│   ├── config.h          # Compile-time configuration and feature flags
│   ├── api_response.h    # API response data structures
│   ├── renderer.h        # Display rendering declarations
│   ├── client_utils.h    # WiFi/HTTP utility declarations
│   ├── wifi_manager.h    # WiFi manager state machine declarations
│   ├── web_ui_data.h     # Auto-generated compressed web UI data
│   ├── _locale.h         # Localization strings and AQI scales
│   ├── _strftime.h       # Time formatting declarations
│   ├── conversions.h     # Unit conversion declarations
│   ├── display_utils.h   # Display utility declarations
│   ├── cert.h            # SSL certificates for HTTPS
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
├── data/                 # Web UI template files
│   ├── setup.html        # Configuration portal HTML template
│   └── saved_api_response.h # Saved API response data
├── docs/                 # Documentation (at project root)
├── .env                  # WiFi credentials (not versioned)
├── load_env.py           # Script to inject WiFi credentials from .env
├── build_web_ui.py       # Script to compress and embed web UI
└── .vscode/              # VS Code settings
```

## Core Modules

### main.cpp
Application entry point and main loop. Contains:
- `setup()`: Initializes WiFi manager, loads RTC RAM variables
- `loop()`: State machine for WiFi connection, weather updates, and deep sleep
- `updateWeather()`: Fetches weather data, reads sensors, renders display
- `beginDeepSleep()`: Power management and sleep scheduling
- `fillMockupData()`: Test data for development without WiFi

### config.h / config.cpp
Configuration management split across two files:
- **config.h**: Compile-time configuration using preprocessor macros
  - Hardware pin definitions
  - Feature flags (display type, sensor type, units, etc.)
  - API endpoints and credentials
  - Battery voltage thresholds
  - Compile-time validation of configuration
- **config.cpp**: Runtime configuration constants
  - Pin assignments for specific hardware wiring
  - Location coordinates (latitude/longitude)
  - City name for display
  - Timezone string
  - Sleep duration and bedtime settings
  - Battery voltage thresholds

### renderer.cpp / renderer.h
Display rendering module:
- E-paper display initialization and control via GxEPD2
- Drawing functions for weather widgets (current conditions, forecasts, graphs)
- Font and icon rendering
- Layout management for different display sizes
- Umbrella widget for rain probability

### api_response.cpp / api_response.h
Data structures and parsing:
- Open-Meteo API response structures (converted to OWM-compatible format)
- WMO weather code to OpenWeatherMap-compatible conversion
- JSON deserialization using ArduinoJson
- Support for loading saved API responses from header file

### wifi_manager.cpp / wifi_manager.h
WiFi connection and configuration portal:
- State machine: BOOT → WIFI_CONNECTING → AP_CONFIG_MODE → NORMAL_MODE → SLEEP_PENDING → ERROR_CONNECTION
- Captive portal with automatic device detection
- RTC RAM variables for persisting config across deep sleep
- IP-based geolocation for automatic location detection
- Web server for configuration interface

### client_utils.cpp / client_utils.h
Network utilities:
- WiFi connection management
- NTP time synchronization
- HTTP/HTTPS API requests to Open-Meteo

## Build System

### PlatformIO Configuration (platformio.ini)

```ini
[platformio]
default_envs = esp32dev

[env]
platform = espressif32
framework = arduino
build_unflags = '-std=gnu++11'
build_flags = '-Wall' '-std=gnu++17'

[env:esp32dev]
board = esp32dev
monitor_speed = 115200
board_build.partitions = huge_app.csv
board_build.f_cpu = 80000000L
extra_scripts = pre:load_env.py, pre:build_web_ui.py
```

### Build Scripts

1. **load_env.py**: Pre-build script that reads `.env` file and injects WiFi credentials as C++ macros
2. **build_web_ui.py**: Pre-build script that compresses `data/setup.html` and embeds it as a C array in `include/web_ui_data.h`

### Build Commands

```bash
# Build the project
pio run

# Upload to device
pio run --target upload

# Monitor Serial Output
pio device monitor --baud 115200

# Build for Specific Environment
pio run -e esp32dev
pio run -e firebeetle32

# Clean Build
pio run --target clean
```

## Configuration System

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

### Configuration Validation

The `config.h` file includes compile-time validation using `#error` directives. If you select invalid combinations (e.g., multiple display types), the build will fail with a descriptive error message.

## WiFi Manager and Configuration Portal

### Connection Flow

1. **Cold Boot**: Device attempts to connect using saved credentials from RTC RAM
2. **WiFi Connecting**: Shows loading screen while connecting
3. **Success**: Proceeds to normal weather update mode
4. **Timeout**: Falls back to AP Configuration Mode

### AP Configuration Mode

When WiFi connection fails or on first boot:
- Creates access point "weather_eink-AP"
- Serves configuration page at `192.168.4.1` or `weather.local`
- Captive portal detection for automatic redirect on mobile devices
- Allows setting:
  - WiFi SSID and password
  - City name and country
  - Latitude and longitude (manual or auto-detect)
- Auto-geolocation via ip-api.com when "Detect automatically" is checked
- Security timeout: AP mode exits after 5 minutes and enters deep sleep

### RTC RAM Persistence

Configuration is stored in RTC RAM (survives deep sleep but not power loss):
- `ramSSID[64]`, `ramPassword[64]`: WiFi credentials
- `ramCity[64]`, `ramCountry[64]`: Location names
- `ramLat[32]`, `ramLon[32]`: Coordinates
- `ramAutoGeo`: Flag for automatic geolocation
- `isFirstBoot`: First boot indicator for silent mode

## Display Types Supported

| Macro | Display | Resolution | Colors |
|-------|---------|------------|--------|
| `DISP_BW_V2` | 7.5" e-Paper (v2) | 800x480px | Black/White |
| `DISP_BW_V2_ALT` | 7.5" e-Paper (v2) | 800x480px | Black/White (Alternative driver for FPC-C001 panels) |
| `DISP_3C_B` | 7.5" e-Paper (B) | 800x480px | Red/Black/White |
| `DISP_7C_F` | 7.3" ACeP e-Paper (F) | 800x480px | 7-Color |
| `DISP_BW_V1` | 7.5" e-Paper (v1) | 640x384px | Black/White (legacy) |

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
- **Enum Values**: `UPPER_CASE` (e.g., `STATE_BOOT`, `MOCKUP_WEATHER_SUNNY`)

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

### Data Source Selection

The firmware supports three mutually exclusive data sources (configured in `config.h`):

| Mode | Config | Description | WiFi Required |
|------|--------|-------------|---------------|
| **Mockup Data** | `USE_MOCKUP_DATA 1` | Synthetic data generated in code | No |
| **Saved API Data** | `USE_SAVED_API_DATA 1` | Real data from `include/saved_api_response.h` | No |
| **Live API** | Both `0` | Fetch from Open-Meteo API | Yes |

**Note:** `USE_MOCKUP_DATA` and `USE_SAVED_API_DATA` are mutually exclusive. Enabling both will cause a compile-time error.

### Mockup Data Mode

Set `USE_MOCKUP_DATA 1` (and `USE_SAVED_API_DATA 0`) to test without WiFi/API:
- Uses synthetic weather data generated in code
- Bypasses all network operations
- Useful for testing display layout and rendering
- Multiple weather scenarios available:
  - `MOCKUP_WEATHER_SUNNY`: Clear sky, warm (25°C)
  - `MOCKUP_WEATHER_RAINY`: Moderate rain (15°C), 85% humidity
  - `MOCKUP_WEATHER_SNOWY`: Light snow (0°C)
  - `MOCKUP_WEATHER_CLOUDY`: Overcast clouds, mild (18°C)
  - `MOCKUP_WEATHER_THUNDER`: Thunderstorm, warm (20°C)
- Rain widget states for testing umbrella display:
  - `MOCKUP_RAIN_NO_RAIN`: POP < 30%, shows X over icon
  - `MOCKUP_RAIN_COMPACT`: POP 30-70%
  - `MOCKUP_RAIN_TAKE`: POP >= 70%
  - `MOCKUP_RAIN_GRAPH_TEST`: Varied POP for graph testing

### Loading Saved API Responses

Set `USE_SAVED_API_DATA 1` (and `USE_MOCKUP_DATA 0`) to load weather data from a header file:
- Loads real API data from `include/saved_api_response.h`
- **Note:** WiFi connection is still required - the device connects to WiFi for NTP time sync but skips the HTTP API call
- Useful for: avoiding API rate limits, faster wake cycles, testing with known/consistent data

**Generating the Header File:**

1. Download Open-Meteo API data (or use previously captured JSON):
```bash
curl "https://api.open-meteo.com/v1/forecast?latitude=XX.XX&longitude=YY.YY&hourly=temperature_2m,relative_humidity_2m,precipitation_probability,weather_code,wind_speed_10m&timezone=auto&forecast_days=2" \
  -o api_response.json
```

2. Convert JSON to C++ header:
```bash
cd platformio
python json_to_header.py api_response.json
```

3. Set `USE_SAVED_API_DATA 1` in `config.h` and rebuild

The device will still connect to WiFi but skip the API HTTP request, loading data directly from the compiled header.

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

**Option 1: Mockup Data (synthetic)**
1. Set `USE_MOCKUP_DATA 1` and `USE_SAVED_API_DATA 0` in `config.h`
2. Select desired weather scenario with `MOCKUP_CURRENT_WEATHER`
3. Build and upload
4. The display will show synthetic weather data

**Option 2: Saved API Data (real)**
1. Set `USE_SAVED_API_DATA 1` and `USE_MOCKUP_DATA 0` in `config.h`
2. Generate `include/saved_api_response.h` from a real API response (see "Saving API Responses" below)
3. Build and upload
4. The display will show real weather data without API calls

## Security Considerations

### HTTPS Certificate Verification

The project supports three HTTP modes:
1. `USE_HTTP`: No encryption (not recommended for production)
2. `USE_HTTPS_NO_CERT_VERIF`: Encryption without authentication (vulnerable to MITM)
3. `USE_HTTPS_WITH_CERT_VERIF`: Full encryption + authentication (recommended)

**Important**: When using `USE_HTTPS_WITH_CERT_VERIF`, the SSL certificate is embedded in `include/cert.h`. Certificates expire and must be updated periodically.

### Credential Storage

- WiFi credentials are loaded from `.env` file by `load_env.py`
- Credentials are injected as build macros, not stored in source code
- RTC RAM storage for runtime usage (cleared on power loss)
- Both `.env` and generated headers are in `.gitignore`

### Battery Safety

- Battery monitoring prevents deep discharge
- Multiple voltage thresholds trigger progressively longer sleep intervals
- Critical low voltage triggers indefinite hibernation (requires manual reset)

## Known Issues and Workarounds

### E-Paper Horizontal Lines Bug

When drawing precipitation bars using a horizontal dotted pattern (`y += 2` row skipping), the e-paper display renders as a completely white screen. However, a vertical dotted pattern (`x += 2` column skipping) works correctly.

**Workaround**: Use vertical dotted patterns or solid fills instead of horizontal row skipping. See `../docs/EPAPER_HORIZONTAL_LINES_BUG.md` for details.

## Deployment Process

### Hardware Requirements

1. **Microcontroller**: ESP32 Dev Module or FireBeetle ESP32
2. **Display**: 7.5" e-paper panel with compatible driver board
3. **Sensor**: BME280 or BME680 (I2C)
4. **Power**: 3.7V LiPo battery or USB power
5. **Optional**: Battery voltage divider circuit for monitoring

### Wiring Reference (Default Configuration)

| Function | Pin |
|----------|-----|
| Battery ADC | 36 (ADC1_CH0) |
| EPD Busy | 25 |
| EPD CS | 15 |
| EPD RST | 26 |
| EPD DC | 27 |
| EPD SCK | 13 |
| EPD MISO | 19 (Not used) |
| EPD MOSI | 14 |
| EPD PWR | 0 (Not used) |
| BME SDA | 21 |
| BME SCL | 22 |
| BME PWR | 0 (Not used) |

### Flashing Steps

1. Create `.env` file with your WiFi credentials
2. Configure `src/config.cpp` with your location coordinates
3. Configure `include/config.h` for your hardware (display type, sensor)
4. Build and upload: `pio run --target upload`
5. Monitor serial output: `pio device monitor`

### First Run

1. If no WiFi credentials are saved, device starts in AP mode
2. Connect to "weather_eink-AP" WiFi network
3. Navigate to `192.168.4.1` or `weather.local`
4. Enter WiFi credentials and location (or use auto-detect)
5. Device will restart and connect to WiFi
6. First weather fetch and display update

### Battery Operation

- Set `USING_BATTERY 1` in `config.h` to enable low-voltage protection
- Set `BATTERY_MONITORING 1` to enable voltage monitoring
- Adjust voltage thresholds in `config.cpp` for your battery

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
- Use serial monitor to check for error messages

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
