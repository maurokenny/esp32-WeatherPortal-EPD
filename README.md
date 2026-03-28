# ESP32 E-Paper Weather Display (Modernized)

A low-power weather display using a wifi-enabled ESP32 microcontroller and a 7.5" E-Paper display. Featuring a modern web-based setup portal and powered by **Open-Meteo**.

<p float="left">
  <img src="showcase/assembled-demo-raleigh-front.jpg" />
  <img src="showcase/assembled-demo-raleigh-side.jpg" width="49%" />
  <img src="showcase/assembled-demo-raleigh-back.jpg" width="49%" />
</p>

## ✨ Highlights (New Features)

- **Open-Meteo Integration**: No more API keys! The project now uses Open-Meteo for high-quality weather data without the hassle of subscriptions.
- **Interactive Captive Portal**: First-time setup is now handled via a web interface. No need to hardcode Wi-Fi credentials.
- **Visual QR Code Setup**: Scannable QR code on the E-ink display for instant mobile connection.
- **Auto-Geolocation**: Optionally detect your city and coordinates automatically using your IP address.
- **Ultra-low power consumption**: ~14μA in sleep, optimized for long battery life (6-12 months).

## 🚀 Setup Guide

### 1. First-Time Configuration (Web Portal)
The easiest way to configure your display is through the interactive setup portal:

1.  **Power On**: On the first boot (or if Wi-Fi fail), the display will show a **Setup Mode** screen with a large QR Code.
2.  **Connect**: Scan the QR code with your phone or connect manually to the Wi-Fi network `esp32-weather-setup`.
3.  **Configure**: Your device should automatically open the setup page. If not, visit `http://weather.local` or `192.168.4.1`.
4.  **Save**: Enter your Wi-Fi password and select your location. You can toggle **"Detect location automatically"** to let the device find your city based on your IP.
5.  **Finish**: The device will restart, connect to your Wi-Fi, and start displaying the weather!

### 2. Manual Configuration (Developers)
If you prefer to "bake in" your settings, edit [config.cpp](platformio/src/config.cpp) to set your default SSID, City, and Coordinates. These values will be used as defaults in the web portal and during cold boots.

> [!WARNING]
> **Security Notice**: The configuration Access Point (AP) created by the ESP32 operates over unencrypted **HTTP**. Wi-Fi passwords entered in the portal are transmitted in **plain text** over the local airwaves. It is recommended to perform this setup in a private, trusted environment.

## 🛠️ Hardware Requirements
- **ESP32**: FireBeetle 2 ESP32-E (or compatible).
- **E-Paper Display**: Waveshare/Good Display 7.5" (800x480).
- **Sensor**: BME280 (for indoor temp/humidity).
- **Battery**: 3.7V LiPo.

## 🤝 Acknowledgments
This project is a modernize fork of the excellent work by **Luke Marzen** ([lmarzen/esp32-weather-epd](https://github.com/lmarzen/esp32-weather-epd)). 

Special thanks to Luke for the robust foundation and the beautiful rendering engine that makes this display possible.

## ⚖️ Licensing
Licensed under the [GNU General Public License v3.0](LICENSE). 
Includes assets and libraries from various authors (see original README or LICENSE file for full details).
