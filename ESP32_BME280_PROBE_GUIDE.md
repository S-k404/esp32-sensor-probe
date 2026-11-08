# ESP32 + BME280 + 1.14" (135x240 RGB IPS) Temperature Probe

A commercial-grade, multi-screen digital temperature probe and environmental monitor instrument powered by an **ESP32**, a high-precision **Bosch BME280** sensor, and a vibrant **1.14" 135x240 IPS Color Display** (ST7789 controller).

---

## 🌟 Key Features

1. **4 Interactive Display Modes**:
   - **Mode 0: Probe Dashboard**: Large bold temperature reading, dynamic thermal border color, comfort indicator badge (`OPTIMAL`, `CHILLY`, `HOT`, `DRY AIR`), humidity progress gauge, barometric pressure in hPa, dew point, and estimated altitude.
   - **Mode 1: Thermal Trend Sparkline**: Real-time 48-sample historical rolling graph with auto-scaling Y-axis, dotted grid lines, and live rate of change (`°C/min` or `°F/min`).
   - **Mode 2: Meteorology & Diagnostics**: Deep breakdown including Dew Point (°C/°F), Heat Index ("Feels Like"), Barometric Pressure in inHg, Altitude in meters & feet, I2C bus address & speed, Uptime, and total sample count.
   - **Mode 3: Big Digits Field Probe**: High-contrast, maximum-scale digits designed for distant viewing or outdoor field inspection.
2. **Zero Screen Flicker**: Uses differential bounding-box redrawing instead of full-screen clears.
3. **Smart Button Navigation**:
   - **Short Press BOOT Button (GPIO 0)**: Cycle through display screens.
   - **Long Press BOOT Button (Hold > 1s)**: Instant toggle between **Celsius (°C)** and **Fahrenheit (°F)**.
   - **Secondary Button (GPIO 35)**: Reset Min/Max peak records.
4. **Resilient Hardware Engine**:
   - Automatically scans and connects to BME280 on **0x76** or **0x77**.
   - If the sensor is unplugged or loose, shows an on-screen recovery alert and auto-reconnects as soon as the cable is re-seated without crashing.
5. **Universal Board Support**: Single-switch configuration for **Generic ESP32 DevKit + Standalone 1.14" IPS** or **LilyGO TTGO T-Display** (ESP32 with built-in 1.14" IPS).

---

## 🔌 Hardware Wiring Guide

### Option A: Standard ESP32 DevKit (30-pin or 38-pin) + Standalone 1.14" ST7789 Module + BME280

#### 1. Display Wiring (1.14" 135x240 IPS ST7789 SPI - 7 or 8 pin)
| Display Pin | ESP32 Pin | Function / Description |
| :--- | :--- | :--- |
| **VCC** | **3.3V** | Power (Do NOT connect to 5V unless module has 5V regulator) |
| **GND** | **GND** | Ground |
| **SCL / SCK / CLK** | **GPIO 18** | Hardware VSPI Clock |
| **SDA / MOSI / DIN**| **GPIO 23** | Hardware VSPI Data |
| **RES / RST** | **GPIO 4** | Display Reset |
| **DC** | **GPIO 16** | Data / Command Selection |
| **CS** | **GPIO 5** | Chip Select |
| **BLK / BL / LED** | **GPIO 15** *(or 3.3V)* | Backlight control (Active HIGH) |

#### 2. BME280 Sensor Wiring (I2C)
| BME280 Pin | ESP32 Pin | Notes |
| :--- | :--- | :--- |
| **VCC** | **3.3V** | Sensor Power |
| **GND** | **GND** | Ground |
| **SCL** | **GPIO 22** | Hardware I2C Clock |
| **SDA** | **GPIO 21** | Hardware I2C Data |
| **CSB / CS** | **3.3V** *(or leave float)*| High enables I2C mode |
| **SDO** | **GND** *(for 0x76)* or **3.3V** *(for 0x77)* | Sets I2C address |

---

### Option B: LilyGO TTGO T-Display (ESP32 with Built-in 1.14" ST7789)

If you have a **TTGO T-Display**, the display, backlight, and buttons are already integrated on-board! You only need to wire the BME280:

1. Open `esp32_bme280_probe.ino` and set:
   ```cpp
   #define BOARD_PROFILE PROFILE_TTGO_T_DISPLAY
   ```
2. Wire the BME280:
   - **BME280 VCC** -> TTGO **3.3V**
   - **BME280 GND** -> TTGO **GND**
   - **BME280 SDA** -> TTGO **GPIO 21**
   - **BME280 SCL** -> TTGO **GPIO 22**

---

## 📦 Required Arduino Libraries

In **Arduino IDE**, open **Library Manager** (`Cmd + Shift + I` on macOS or `Ctrl + Shift + I` on Windows/Linux) and install:

1. **Adafruit GFX Library** by Adafruit (Click *Install All* when prompted for dependencies).
2. **Adafruit ST7735 and ST7789 Library** by Adafruit.
3. **Adafruit BME280 Library** by Adafruit.
4. **Adafruit Unified Sensor** by Adafruit.

---

## 🚀 How to Flash with Arduino IDE

1. Open `esp32_bme280_probe/esp32_bme280_probe.ino` in Arduino IDE.
2. In the **Tools** menu:
   - **Board**: Select **ESP32 Dev Module** (or **TTGO T-Display** if available).
   - **Upload Speed**: `921600` (or `115200`).
   - **CPU Frequency**: `240MHz` (or `80MHz` / `160MHz`).
   - **Port**: Select your ESP32 serial port (e.g., `/dev/cu.usbserial-...` or `COMx`).
3. Click the **Upload** arrow icon.
4. Open the **Serial Monitor** (`Tools > Serial Monitor`) and set baud rate to **115200 baud** to view real-time diagnostics.

---

## 🐍 MicroPython Option

If your ESP32 is flashed with MicroPython:
1. Upload [esp32_bme280_probe_micropython.py](esp32_bme280_probe_micropython.py) to your ESP32 using Thonny IDE or `mpremote`.
2. The script includes a self-contained, zero-dependency BME280 driver with automatic I2C address detection (`0x76`/`0x77`), altitude calculation, dew point estimation, and min/max telemetry streaming over REPL.

---

## 🔧 Fine-Tuning & Customization

Inside `esp32_bme280_probe.ino`:
- **Inverted Colors**: If your IPS screen shows blacks as whites or odd colors, toggle:
  ```cpp
  #define INVERT_COLORS true  // or false
  ```
- **Orientation (180° Flip)**: If your display is upside-down relative to your enclosure or probe handle, change:
  ```cpp
  #define DISPLAY_ROTATION 3  // 1 or 3
  ```
- **Temperature Calibration**: If you have a certified reference thermometer and wish to offset the reading:
  ```cpp
  #define TEMP_CAL_OFFSET_C (-0.5f) // Adjusts all readings by -0.5°C
  ```
