# ESP32 E-Paper Weather Display

A low-power weather station that displays real-time conditions and forecasts on a 7.5" e-paper display. Powered by ESP32 and Open-Meteo — no API key required for personal use.

<p float="left">
  <img src="showcase/assembled-demo-raleigh-front.jpg" />
  <img src="showcase/assembled-demo-raleigh-side.jpg" width="49%" />
  <img src="showcase/assembled-demo-raleigh-back.jpg" width="49%" />
</p>

---

## Features

- **No API Key** — Free weather data via Open-Meteo (no API key required for personal use)
- **Easy Setup** — QR code + captive portal for mobile
- **Auto Location** — IP-based geolocation (optional)
- **Rain Alert** — Umbrella indicator with probability
- **Low Power** — Deep sleep between updates
- **Offline Dev** — Mock data mode for testing
- **Multi-locale** — 10 language support

---

## Requirements

### Hardware
- **ESP32 Board**: E-Paper ESP32 Driver Board Rev 3 (or compatible)
- **Display**: Waveshare 7.5" e-paper (800x480, Black/White)
- **Adapter**: Waveshare e-Paper adapter
- **Optional**: BME280/BME680 sensor for indoor readings

### Software
- [PlatformIO Core](https://platformio.org/install) 6.0+
- Python 3.8+
- Git

---

## Quick Start

```bash
# Clone and build
cd platformio
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

On first boot, the device creates a Wi-Fi access point named `weather_eink-AP`. Scan the QR code on the display or connect manually to configure.

---

## Setup

### Web Portal (Recommended)

1. Power on the device
2. Scan the QR code on screen, or connect to `weather_eink-AP`
3. Open http://192.168.4.1 or http://weather.local
4. Enter Wi-Fi credentials and location
5. Save — device restarts and begins updating

> **Security Note**: Setup runs over unencrypted HTTP. Perform on a trusted network.

### Alternative: Environment Variables

Create `platformio/.env`:

```env
WIFI_SSID=your_wifi
WIFI_PASSWORD=your_password
```

---

## How It Works

The firmware runs on a deterministic state machine that:

1. Connects to Wi-Fi (or enters setup mode on first boot)
2. Fetches weather data from Open-Meteo
3. Renders current conditions and forecast to the e-paper display
4. Enters deep sleep to conserve power
5. Wakes up after the configured interval and repeats

Persistent state (credentials, failure counters) survives deep sleep in RTC memory.

**Read more**: [State Machine Documentation](docs/STATE_MACHINE.md)

---

## Development & Testing

### Offline Mode (No WiFi)

Enable in `include/config.h`:

```c
#define USE_MOCKUP_DATA 1
```

Available scenarios: SUNNY, RAINY, SNOWY, CLOUDY, THUNDER

### Testing with Saved API Data

Test with real API responses without making live calls:

```bash
# 1. Download an API response (example for Manaus, Brazil)
curl "https://api.open-meteo.com/v1/forecast?latitude=-3.10&longitude=-60.02&current=temperature_2m,apparent_temperature,is_day,weather_code,precipitation,rain,showers,snowfall,relative_humidity_2m,wind_speed_10m,wind_direction_10m,wind_gusts_10m,pressure_msl,surface_pressure&hourly=temperature_2m,precipitation_probability,precipitation,wind_speed_10m,wind_direction_10m,wind_gusts_10m,weather_code,is_day&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,precipitation_probability_max,precipitation_sum&timezone=auto&forecast_days=8" \
  -o api_response.json

# 2. Convert to C++ header
cd platformio
python json_to_header.py api_response.json

# 3. Enable saved data mode in include/config.h
#define USE_SAVED_API_DATA 1

# 4. Build and run
pio run -e esp32dev
```

Useful for:
- Testing with consistent/known data
- Avoiding API rate limits
- Faster development cycles

### Local Testing with act

Run GitHub Actions workflows locally using [act](https://github.com/nektos/act):

```bash
# Install act
brew install act  # macOS
curl -s https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash  # Linux

# Run a specific job
act -j <job-name> -P ubuntu-latest=catthehacker/ubuntu:act-latest

# List available jobs
act -l
```

**Available Workflows:**

| Workflow | Job Name | Description | Requirements |
|----------|----------|-------------|--------------|
| `state-machine.yml` | `test-state-machine` | Unit tests (fast) | None |
| `blackbox.yml` | `blackbox-test` | Single integration test | Docker |
| `blackbox-matrix.yml` | `blackbox-matrix` | Matrix tests (14 envs) | Docker + artifact path |

**Examples:**

```bash
# Fast unit tests - no Docker required
act -j test-state-machine -P ubuntu-latest=catthehacker/ubuntu:act-latest

# Integration tests - requires Docker
act -j blackbox-test -P ubuntu-latest=catthehacker/ubuntu:act-latest

# Full matrix tests - save artifacts
act -j blackbox-matrix -P ubuntu-latest=catthehacker/ubuntu:act-latest --artifact-server-path /tmp/artifacts
```

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Display not updating | Check SPI wiring and BUSY pin connection |
| Wi-Fi won't connect | Verify credentials; check signal strength |
| Time wrong | Check timezone setting in config |
| Stuck in setup mode | Ensure first boot (RTC clears on power loss) |

For detailed debugging, see [AGENTS.md](platformio/AGENTS.md).

---

## License

GPL v3.0. A fork of the excellent [esp32-weather-epd](https://github.com/lmarzen/esp32-weather-epd) project by Luke Marzen.
