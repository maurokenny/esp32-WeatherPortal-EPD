# ESP32 E-Paper Weather Display (Modernized)

A low-power weather display using a Wi-Fi-enabled ESP32 microcontroller and a 7.5" E-Paper display. Featuring a modern web-based setup portal and weather data provided by **Open-Meteo**.

<p float="left">
  <img src="showcase/assembled-demo-raleigh-front.jpg" />
  <img src="showcase/assembled-demo-raleigh-side.jpg" width="49%" />
  <img src="showcase/assembled-demo-raleigh-back.jpg" width="49%" />
</p>

## Highlights (New Features)

* **Open-Meteo Integration**: No more API keys! The project now uses Open-Meteo for high-quality weather data without the hassle of subscriptions.
* **Interactive Captive Portal**: First-time setup is now handled via a web interface. No need to hardcode Wi-Fi credentials.
* **Visual QR Code Setup**: Scannable QR code on the E-ink display for instant mobile connection.
* **Auto-Geolocation**: Optionally detect your city and coordinates automatically using your IP address.
* **Ultra-low power consumption**: Optimized for long battery life (6–12 months), achieving ~14μA in deep sleep when using a FireBeetle ESP32.

> **Warning: Hardware Limitation**
> When using the E-Paper ESP32 Driver Board (Rev 3), power consumption will be significantly higher due to its less efficient voltage regulator and USB-to-Serial circuit. The ~14μA deep sleep target is **not achievable** with this board.

---

## Notes on This Version

This fork includes several practical changes and adaptations based on available hardware and observed issues:

* **Different Hardware Setup**
  This version uses:

  * **E-Paper ESP32 Driver Board Rev 3**
  * **Waveshare e-paper adapter**

  The original low-power reference setup (e.g., FireBeetle ESP32) is not used in this build.

* **No Battery / Sensor Integration (Yet)**
  This implementation currently runs:

  * Without a battery
  * Without external environmental sensors

  These may be added in future iterations.

* **Planned Ultra-Low Power Version**
  I plan to gain access to a **FireBeetle ESP32** and the original hardware configuration proposed in the reference project.
  The goal is to build a **fully optimized ultra-low-power version with battery support**, matching the intended long-term autonomous operation.

* **Precipitation Probability Display Workaround**
  A bug was encountered where the display would turn completely white when rendering **precipitation probability**.
  As a workaround, the **visual pattern used to represent precipitation probability was modified**, which resolves the issue on this hardware.

* **Umbrella Widget**
  A custom **umbrella widget** was added — this was the original motivation for the project for personal use.
  It provides a quick, intuitive visual indicator of precipitation likelihood.

* **Optional `.env` Configuration**
  In addition to the web portal and `config.cpp`, you can define:

  ```
  WIFI_SSID=your_wifi
  WIFI_PASSWORD=your_password
  ```

  This helps avoid hardcoding credentials and improves local development workflows.

---

## Setup Guide

### 1. First-Time Configuration (Web Portal)

The easiest way to configure your display is through the interactive setup portal:

1. **Power On**
   On the first boot (or if Wi-Fi fails), the display will show a **Setup Mode** screen with a large QR Code.

2. **Connect**
   Scan the QR code with your phone or connect manually to the Wi-Fi network:

   ```
   esp32-weather-setup
   ```

3. **Configure**
   Your device should automatically open the setup page. If not, visit:

   * [http://weather.local](http://weather.local)
   * [http://192.168.4.1](http://192.168.4.1)

4. **Save**

   * Enter your Wi-Fi password
   * Select your location
   * Optionally enable **"Detect location automatically"**

5. **Finish**
   The device will restart, connect to your Wi-Fi, and begin displaying weather data.

---

### 2. Manual Configuration (Developers)

If you prefer to "bake in" your settings:

* Edit:

  ```
  platformio/src/config.cpp
  ```
* Set your:

  * Default SSID
  * City
  * Coordinates

These values will act as defaults in the web portal and during cold boots.

---

### 3. Environment Variables (.env) (Optional)

You can also configure Wi-Fi credentials using a `.env` file:

```
WIFI_SSID=your_wifi
WIFI_PASSWORD=your_password
```

This is useful for:

* Keeping credentials out of version control
* Cleaner development setup

---

### 4. API Data Caching (Optional)

Skip the HTTP API call by using pre-saved weather data. This is useful for:
* Avoiding API rate limits during development
* Faster display updates (skips HTTP request)
* Testing with consistent/known weather data

**Note:** WiFi connection is still required for NTP time synchronization - only the API HTTP call is skipped.

**Setup:**

1. **Download Open-Meteo Data:**
   ```bash
   curl "https://api.open-meteo.com/v1/forecast?latitude=YOUR_LAT&longitude=YOUR_LON&hourly=temperature_2m,relative_humidity_2m,precipitation_probability,weather_code,wind_speed_10m&timezone=auto&forecast_days=2" \
     -o api_response.json
   ```

2. **Convert to C++ Header:**
   ```bash
   cd platformio
   python json_to_header.py api_response.json
   ```

3. **Enable Cached Data:**
   Set `USE_SAVED_API_DATA 1` in `platformio/include/config.h` and rebuild.

**For Display Testing:**
Use `USE_MOCKUP_DATA 1` for synthetic weather data without API calls. This generates fake data in code (temperature, conditions, etc.) while still connecting to WiFi for time sync.

---

> **Warning: Security Notice**
> The configuration Access Point (AP) created by the ESP32 operates over unencrypted **HTTP**.
> Wi-Fi passwords entered in the portal are transmitted in **plain text** over the air.
> It is recommended to perform the initial setup in a secure/private environment.

---

## Hardware Requirements

* **ESP32**

  * E-Paper ESP32 Driver Board Rev 3
  * Waveshare e-paper adapter

* **E-Paper Display**

  * Waveshare / Good Display 7.5" (800x480)

---

## Acknowledgments

This project is a modernized fork of the excellent work by **Luke Marzen**
[https://github.com/lmarzen/esp32-weather-epd](https://github.com/lmarzen/esp32-weather-epd)

Special thanks for providing the original foundation.

---

## Licensing

Licensed under the **GNU General Public License v3.0**.

Includes assets and libraries from various authors (see original README or LICENSE file for full details).
