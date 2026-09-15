# ESP32 NAT Router Extended — High Performance & CYD Galaxy Edition 🚀

[![Build Status](https://img.shields.io/badge/PlatformIO-ESP--IDF-orange.svg)](https://platformio.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![WiFi Power Save](https://img.shields.io/badge/WiFi_Power_Save-Disabled_(Zero_Latency)-green.svg)]()
[![TX Power](https://img.shields.io/badge/Max_TX_Power-20dBm_(Max_Range)-blue.svg)]()
[![24/7 Stability](https://img.shields.io/badge/Watchdog-24%2F7_Auto--Reconnect-brightgreen.svg)]()

This is an optimized, feature-rich firmware to use the ESP32 as a high-performance **WiFi NAT Router / Repeater**. Designed for maximum signal quality, ultra-low latency, 24/7 unbreakable internet connections, multi-AP auto-failover, and full support for Cheap Yellow Display (CYD / ESP32-2432S028R) touchscreens!

---

## ⚡ High Performance & Low Latency Optimizations

- **Zero-Latency Wi-Fi (`WIFI_PS_NONE`):** Wi-Fi modem power saving is disabled by default to eliminate delay spikes, ping jitter, and packet buffering stalls during active throughput.
- **Maximum Transmit Power (20 dBm / 80):** Wi-Fi TX power defaults to maximum output power to ensure stable connections even through walls or weak signals.
- **24/7 Smart Ping Watchdog:** Background health checker actively pings upstream connectivity. If 3 consecutive failures occur, it automatically triggers auto-recovery or switches to a saved AP.
- **Enhanced Multi-AP Auto-Shift:** Multi-network failover automatically scans and connects to the strongest saved Wi-Fi access point if your primary network goes down.
- **CYD Display Real-Time Dashboard (ESP32-2432S028R):** ILI9341 LCD + XPT2046 touchscreen UI displaying connected clients, signal RSSI, live ping latency in ms, battery percentage, and active network profiles.

---

## 🌐 Features & Use Cases
- **Range Extender / Repeater:** Extend existing Wi-Fi networks easily.
- **Guest / IoT Isolated Network:** Create separate SSIDs with custom subnets and passwords.
- **Bypass Captive Portals & Device Limits:** Mask multiple clients behind a single MAC / IP address.
- **WPA2 Enterprise Support:** EAP-TLS / PEAP support for university and corporate networks.
- **Port Mapping / Forwarding:** Map internal ports for remote server access or gaming.
- **Mobile-Friendly Web UI & OTA Updates:** Simple browser interface at `192.168.4.1` with fast over-the-air update capability.

---

## 🖥️ Web Config Interface & Screenshots

Connect to the SSID `ESP32_NAT_Router` and visit `http://192.168.4.1`.

![Main Interface](docs/index.png)

---

## ⚡ One-Click Browser Flasher (Web Installer)

You can flash this modified high-performance firmware directly to your ESP32 from your browser over USB (no command line required!):

👉 **[Launch Web Installer in Browser (docs/install.html)](docs/install.html)**
*(Requires Google Chrome, Microsoft Edge, or Opera on Desktop)*

*To enable direct web flashing on your GitHub repository:*
1. Go to repository **Settings** -> **Pages**.
2. Set Source to `Deploy from a branch` and select `main` (or default branch) `/docs` folder.
3. Your live browser installer will be hosted at `https://<your-username>.github.io/<your-repo>/install.html`.

---

## 🛠️ Flashing Pre-built Binaries (Manual)

### Option 1: esptool.py (Command Line)
```bash
# Erase flash (recommended for fresh install)
esptool.py erase_flash

# Flash full binary at address 0x0
esptool.py write_flash 0x0 esp32nat_extended_full_vX.X.X.bin
```

### Option 2: PlatformIO
```bash
# Build binary
pio run -e esp32

# Upload to board
pio run -e esp32 -t upload
```

---

## 📜 License
This project is licensed under the MIT License — see the original repositories [martin-ger/esp32_nat_router](https://github.com/martin-ger/esp32_nat_router) and [dchristl/esp32_nat_router_extended](https://github.com/dchristl/esp32_nat_router_extended).
