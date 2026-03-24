# ESP32 Weather E-Paper Display

## Project Overview

This is an ESP32-based weather display application that fetches weather data from the OpenWeatherMap API and displays it on a 7.5" E-Paper (E-Ink) display. The application is designed for ultra-low power consumption, using deep sleep between updates to run on battery power for extended periods.

**Key Features:**
- Fetches current weather, hourly forecasts (up to 48 hours), daily forecasts (up to 8 days), and air quality data
- Displays data on 7.5" E-Paper displays (800x480 Black/White, with optional 3-color and 7-color support)
- Supports indoor environmental sensors (BME280 for temperature/humidity/pressure or BME680 for additional air quality sensing)
- Battery monitoring with configurable low-power hibernation modes
- 10 locale options with multi-language support for UI text and weather descriptions
- Multiple unit system support (Imperial/Metric) for temperature, wind speed, pressure, visibility, and precipitation
- Customizable widget layouts, fonts, and display options
- Deep sleep power management with configurable sleep intervals and "bed time" power saving
- National weather alerts display with urgency-based filtering

## Technology Stack

**Platform:**
- **Framework:** Arduino (via PlatformIO)
- **Platform:** Expressif32 (ESP32)
- **Build System:** PlatformIO (platformio.ini)
- **C++ Standard:** GNU++17
- **Target Board:** ESP32 dev board (esp32dev)

**Target Hardware:**
- ESP32 development board (FireBeetle 2 ESP32-E recommended)
- 7.5" E-Paper display (800x480 Black/White)
- BME280 or BME680 indoor sensor (optional)
- LiPo battery with voltage monitoring (optional)

**External Libraries (managed by PlatformIO):**
- `adafruit/Adafruit BME280 Library @ 2.3.0` - Indoor temperature/humidity/pressure sensor
- `adafruit/Adafruit BME680 Library @ 2.0.5` - Indoor air quality sensor
- `adafruit/Adafruit BusIO @ 1.17.4` - I2C/SPI communication
- `adafruit/Adafruit Unified Sensor @ 1.1.15` - Sensor abstraction
- `adafruit/Adafruit GFX Library @ 1.11.9` - Graphics library for fonts
- `bblanchon/ArduinoJson @ 7.4.2` - JSON parsing for API responses

**Local Libraries (in `lib/` directory):**
- `lib/esp32-weather-epd-assets/` - Bitmap fonts (15 font families, sizes 4pt-48pt) and weather icons (9 sizes from 16x16 to 196x196)
- `lib/pollutant-concentration-to-aqi/` - AQI calculation library (C) supporting 10 international standards

**APIs:**
- OpenWeatherMap One Call API 3.0 (or 2.5 for legacy users)
- OpenWeatherMap Air Pollution API

## Project Structure

```
.
├── platformio.ini           # PlatformIO configuration with build settings
├── src/                     # Source code implementation files
│   ├── main.cpp             # Application entry point, main setup() and loop()
│   ├── config.cpp           # Runtime configuration constants (pins, credentials, timing)
│   ├── api_response.cpp     # OpenWeatherMap API JSON deserialization
│   ├── client_utils.cpp     # WiFi connection, HTTP/HTTPS client, NTP time sync
│   ├── display_utils.cpp    # Display helper functions (battery, WiFi, icons, alerts)
│   ├── renderer_ws.cpp      # E-Paper display rendering (Waveshare library)
│   ├── conversions.cpp      # Unit conversion utilities
│   ├── locale.cpp           # Locale selection and inclusion
│   └── _strftime.cpp        # Custom strftime implementation for locale support
├── include/                 # Header files
│   ├── config.h             # Compile-time configuration macros and validation
│   ├── api_response.h       # API response data structures and typedefs
│   ├── client_utils.h       # Client utility function declarations
│   ├── display_utils.h      # Display utility function declarations
│   ├── renderer.h           # Rendering function and display object declarations
│   ├── renderer_gxepd.h     # Alternative GxEPD2 renderer (not currently used)
│   ├── _locale.h            # Locale text extern declarations
│   ├── _strftime.h          # Custom strftime declarations
│   ├── conversions.h        # Unit conversion function declarations
│   ├── cert.h               # SSL/TLS certificates for HTTPS verification
│   └── locales/             # Locale definition files (.inc extension)
│       ├── locale_en_US.inc # US English translations
│       ├── locale_en_GB.inc # UK English translations
│       ├── locale_de_DE.inc # German translations
│       ├── locale_es_ES.inc # Spanish translations
│       ├── locale_et_EE.inc # Estonian translations
│       ├── locale_fi_FI.inc # Finnish translations
│       ├── locale_fr_FR.inc # French translations
│       ├── locale_it_IT.inc # Italian translations
│       ├── locale_nl_BE.inc # Dutch (Belgium) translations
│       └── locale_pt_BR.inc # Portuguese (Brazil) translations
├── lib/                     # Local libraries (not managed by PlatformIO)
│   ├── esp32-weather-epd-assets/  # Bitmap fonts and weather icons
│   │   ├── fonts/           # Bitmap fonts (FreeMono, FreeSans, FreeSerif, Lato, 
│   │   │                    #   Montserrat, Open Sans, Poppins, Quicksand, Raleway, 
│   │   │                    #   Roboto, Roboto Mono, Roboto Slab, Ubuntu, Ubuntu Mono)
│   │   └── icons/           # Weather icons in multiple sizes (16x16 to 196x196)
│   └── pollutant-concentration-to-aqi/  # AQI calculation library
│       ├── aqi.c            # AQI calculation implementation
│       ├── aqi.h            # AQI calculation headers
│       ├── README.md        # Library documentation
│       └── LICENSE          # Library license
└── .vscode/                 # VS Code settings
    └── extensions.json      # Recommended extensions (PlatformIO IDE)
```

## Build and Upload Commands

**Prerequisites:**
- VS Code with PlatformIO extension (recommended), or
- PlatformIO Core CLI

**Build:**
```bash
pio run
```

**Upload:**
```bash
pio run --target upload
```

**Monitor serial output:**
```bash
pio device monitor --baud 115200
```

**Clean build:**
```bash
pio run --target clean
```

## Configuration

Configuration is split between **compile-time macros** (in `include/config.h`) and **runtime constants** (in `src/config.cpp`).

### Critical Configuration (src/config.cpp)

You **MUST** edit these before building:

```cpp
// WiFi credentials
const char *WIFI_SSID     = "your_ssid";
const char *WIFI_PASSWORD = "your_password";

// OpenWeatherMap API key (get from openweathermap.org)
const String OWM_APIKEY   = "your_api_key_here";

// Location coordinates
const String LAT = "40.7128";      // Latitude
const String LON = "-74.0060";     // Longitude
const String CITY_STRING = "New York"; // Display name

// Timezone (see https://github.com/nayarsystems/posix_tz_db)
const char *TIMEZONE = "EST5EDT,M3.2.0,M11.1.0";
```

### Hardware Options (include/config.h)

**Display Panel Selection** (uncomment exactly one):
- `DISP_BW_V2` - 7.5" Black/White 800x480 (default, recommended)
- `DISP_3C_B` - 7.5" Red/Black/White 800x480
- `DISP_7C_F` - 7.3" 7-Color 800x480
- `DISP_BW_V1` - 7.5" Black/White 640x384 (legacy)

**Driver Board:**
- `DRIVER_DESPI_C02` - DESPI-C02 (recommended)
- `DRIVER_WAVESHARE` - Waveshare rev2.2/2.3 (deprecated but currently used)

**Indoor Sensor:**
- `SENSOR_BME280` - Temperature/Humidity/Pressure (default)
- `SENSOR_BME680` - Temperature/Humidity/Pressure/Gas (air quality)

**Protocol Security:**
- `USE_HTTP` - Plain HTTP (less secure, lower power)
- `USE_HTTPS_NO_CERT_VERIF` - HTTPS without certificate verification
- `USE_HTTPS_WITH_CERT_VERIF` - HTTPS with certificate verification (default, requires cert updates)

### Runtime Settings (src/config.cpp)

```cpp
// Sleep duration between updates (minutes, range: 2-1440)
const int SLEEP_DURATION = 30;

// Bed time power saving (optional)
const int BED_TIME  = 00;  // Stop updates at midnight
const int WAKE_TIME = 06;  // Resume at 6 AM

// Battery voltage thresholds (millivolts)
const uint32_t LOW_BATTERY_VOLTAGE      = 3462;  // ~10%
const uint32_t VERY_LOW_BATTERY_VOLTAGE = 3442;  // ~8%
const uint32_t CRIT_LOW_BATTERY_VOLTAGE = 3404;  // ~5%

// Hourly outlook graph range (hours, range: 8-48)
const int HOURLY_GRAPH_MAX = 24;
```

## Code Style Guidelines

**Language:** C++17 (GNU extensions enabled)

**Naming Conventions:**
- **Macros/Constants:** `UPPER_CASE_WITH_UNDERSCORES` (e.g., `DISP_BW_V2`, `DEBUG_LEVEL`, `OWM_NUM_HOURLY`)
- **Type Definitions:** `lowercase_with_underscore_t` (e.g., `owm_current_t`, `alignment_t`, `owm_resp_onecall_t`)
- **Functions:** `camelCase` (e.g., `beginDeepSleep()`, `drawString()`, `getOWMonecall()`)
- **Variables:** `camelCase` (e.g., `batteryVoltage`, `sleepDuration`, `timeInfo`)
- **Global Variables:** Use `extern` declarations in headers, define in source files
- **Private/Internal Headers:** Prefixed with underscore (e.g., `_locale.h`, `_strftime.cpp`)

**File Organization:**
- Headers in `include/`, implementation in `src/`
- Each major module has its own `.h`/`.cpp` pair
- Locale files use `.inc` extension (included by `locale.cpp`, not compiled directly)
- Local libraries in `lib/` with their own structure

**Comments:**
- Multi-line GPL license header at the start of each file
- Single-line comments for inline explanations using `//`
- Configuration options extensively documented in `config.h`
- All functions end with `// end functionName` comment

**Code Structure:**
- Indentation: 2 spaces
- Braces: Opening brace on same line for functions and control structures

## Architecture Overview

### Application Flow

1. **setup()** (runs once per wake cycle, `main.cpp`):
   - Initialize serial communication (115200 baud)
   - Check battery voltage (enter low-power mode if critical)
   - Connect to WiFi (with timeout and error handling)
   - Synchronize time via NTP
   - Fetch weather data from OpenWeatherMap APIs (One Call, Air Pollution)
   - Read indoor sensor (BME280/BME680)
   - Initialize and render display (current conditions, forecast, alerts, status bar)
   - Enter deep sleep until next scheduled update

2. **Deep Sleep:**
   - ESP32 consumes <11μA during sleep
   - RTC memory persists
   - Wake timer configured for next update interval (aligned to SLEEP_DURATION)
   - "Bed time" feature can skip updates during specified hours

### Key Modules

**main.cpp:**
- Application entry point
- Deep sleep timing calculations with alignment to minute boundaries
- Error handling and recovery (displays error screens for failures)

**config.h/config.cpp:**
- Hardware pin definitions (SPI for display, I2C for sensor, ADC for battery)
- User-configurable settings (WiFi, API key, location, timing)
- Compile-time feature selection with validation macros

**api_response.h/api_response.cpp:**
- Data structures for OpenWeatherMap API responses
- JSON deserialization using ArduinoJson with filtering to reduce memory usage
- Uses `std::vector` for variable-length alert data

**client_utils.h/client_utils.cpp:**
- WiFi connection management with timeout
- HTTP/HTTPS client functionality (supports certificate verification)
- NTP time synchronization with multiple server fallback

**renderer.h/renderer_ws.cpp:**
- E-Paper display initialization and power management using Waveshare library
- High-level drawing functions (current conditions, forecast, alerts)
- Text alignment (LEFT, RIGHT, CENTER) and multi-line rendering
- Display buffer management (800x480 / 8 = 48KB)

**display_utils.h/display_utils.cpp:**
- Battery voltage reading and percentage calculation
- WiFi signal strength indicators
- Weather icon selection based on conditions
- Alert categorization and filtering by urgency
- Air Quality Index calculations using multiple international standards
- Moon phase calculation and display

**conversions.h/conversions.cpp:**
- Unit conversion utilities for temperature, wind speed, pressure, distance, precipitation

### Memory Management

- Large API response structures declared as `static` (not on stack) to avoid overflow
- Uses `std::vector` for variable-length alert data
- Non-volatile storage (NVS) used for low-battery state persistence
- Display buffering handled by Waveshare library (48KB for 800x480 display)
- ArduinoJson filtering reduces memory usage during API response parsing
- Partition scheme: `huge_app.csv` for increased heap memory

## Localization

The project supports multiple locales through compile-time selection:

**Available Locales:**
- `de_DE` - German (Germany)
- `en_GB` - English (United Kingdom)
- `en_US` - English (United States) [default]
- `es_ES` - Spanish (Spain)
- `et_EE` - Estonian (Estonia)
- `fi_FI` - Finnish (Finland)
- `fr_FR` - French (France)
- `it_IT` - Italian (Italy)
- `nl_BE` - Dutch (Belgium)
- `pt_BR` - Portuguese (Brazil)

**To add a locale:**
1. Copy `include/locales/locale_en_US.inc` to new file (e.g., `locale_ja_JP.inc`)
2. Translate all string constants
3. Set `LOCALE` macro in `include/config.h` to your locale
4. Update `LC_TIME` formats, month/day names, unit symbols, alert terminology, and AQI descriptions

**Note:** OpenWeatherMap returns weather alerts in English only. For regions where English is uncommon, set `DISPLAY_ALERTS 0` in `config.h` to disable alert display.

## Testing and Debugging

**Debug Levels (config.h):**
- `DEBUG_LEVEL 0` - Basic status information, assists troubleshooting (default)
- `DEBUG_LEVEL 1` - Increased verbosity, heap usage printing
- `DEBUG_LEVEL 2` - Print full API responses to serial

**Serial Monitor:**
- Baud rate: 115200
- Use PlatformIO monitor: `pio device monitor`

**Common Issues:**
- WiFi connection failures: Check credentials, signal strength, and `WIFI_TIMEOUT` setting
- Time sync failures: Increase `NTP_TIMEOUT` or change NTP servers
- API errors: Verify API key subscription status at openweathermap.org
- Certificate errors: Update `include/cert.h` if using HTTPS with verification (certificates expire)
- BME sensor not found: Check I2C wiring and address (0x76 vs 0x77 based on SDO pin)

## Security Considerations

**HTTPS Certificates:**
- When using `USE_HTTPS_WITH_CERT_VERIF`, certificates in `include/cert.h` expire periodically
- Current certificate for `api.openweathermap.org` valid until 2026-04-10
- Run `cert.py` (if available) or use OpenSSL to generate updated `cert.h` when needed:
  ```bash
  openssl s_client -verify 5 -showcerts -connect api.openweathermap.org:443 < /dev/null
  ```

**API Key Storage:**
- API key stored as plain text in `config.cpp`
- For production devices, consider implementing encrypted storage or secure provisioning

**WiFi Credentials:**
- Stored as plain text in source code
- Consider using WiFiManager library for credential provisioning in production

**HTTP vs HTTPS:**
- `USE_HTTP` consumes less power but offers no security
- `USE_HTTPS_NO_CERT_VERIF` offers encryption but vulnerable to MITM attacks
- `USE_HTTPS_WITH_CERT_VERIF` offers full security but requires certificate maintenance

## License

This project is licensed under the GNU General Public License v3.0 (GPL-3.0+).
See individual source files for copyright notices.

## Resources

- **Original Repository:** https://github.com/lmarzen/esp32-weather-epd
- **OpenWeatherMap:** https://openweathermap.org/api/one-call-api
- **PlatformIO:** https://platformio.org/
- **POSIX Timezone Database:** https://github.com/nayarsystems/posix_tz_db
