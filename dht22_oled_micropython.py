"""
===============================================================================
MicroPython Test Code: DHT22 + I2C SSD1306 OLED (128x64)
Compatible with: ESP32, ESP8266, Raspberry Pi Pico
===============================================================================
Prerequisites:
Ensure 'ssd1306.py' is uploaded to your board (standard MicroPython driver).
===============================================================================
"""

import machine
import time
import dht
import sys

# Try importing ssd1306 driver
try:
    import ssd1306
except ImportError:
    print("[ERROR] 'ssd1306.py' library not found! Please upload ssd1306.py to your board.")
    sys.exit()

# -------------------------------------------------------------
# Pin Configuration (Adjust pins according to your board)
# -------------------------------------------------------------
# ESP32 Defaults:
I2C_SDA_PIN = 21
I2C_SCL_PIN = 22
DHT_PIN = 4

# For Raspberry Pi Pico, uncomment these:
# I2C_SDA_PIN = 8
# I2C_SCL_PIN = 9
# DHT_PIN = 15

# Display specifications
SCREEN_WIDTH = 128
SCREEN_HEIGHT = 64

# -------------------------------------------------------------
# Hardware Initialization
# -------------------------------------------------------------
i2c = machine.I2C(0, scl=machine.Pin(I2C_SCL_PIN), sda=machine.Pin(I2C_SDA_PIN), freq=400000)
oled = ssd1306.SSD1306_I2C(SCREEN_WIDTH, SCREEN_HEIGHT, i2c)
sensor = dht.DHT22(machine.Pin(DHT_PIN))

print("=== DHT22 + OLED MicroPython Test ===")
print("Scanning I2C bus...")
devices = i2c.scan()
if devices:
    for d in devices:
        print(f"Found I2C device at address: {hex(d)}")
else:
    print("[WARNING] No I2C devices found! Check wiring.")

# Splash Screen
oled.fill(0)
oled.text("DHT22 + OLED", 15, 20)
oled.text("Initializing...", 10, 36)
oled.show()
time.sleep(2)

heartbeat = False
sample_count = 0

# -------------------------------------------------------------
# Main Loop
# -------------------------------------------------------------
while True:
    try:
        sample_count += 1
        heartbeat = not heartbeat

        # Measure sensor values
        sensor.measure()
        temp_c = sensor.temperature()
        temp_f = temp_c * 9.0 / 5.0 + 32.0
        humidity = sensor.humidity()

        print(f"[{sample_count}] Temp: {temp_c:.1f}°C ({temp_f:.1f}°F) | Humid: {humidity:.1f}%")

        # Update Display
        oled.fill(0)

        # Header
        oled.fill_rect(0, 0, 128, 12, 1)
        oled.text("WEATHER MONITOR", 5, 2, 0) # Inverted text (black on white)

        # Temperature
        oled.text("TEMP:", 0, 18, 1)
        oled.text(f"{temp_c:.1f} C ({temp_f:.0f} F)", 45, 18, 1)

        # Humidity
        oled.text("HUMID:", 0, 34, 1)
        oled.text(f"{humidity:.1f} %", 48, 34, 1)

        # Divider
        oled.hline(0, 48, 128, 1)

        # Status & Heartbeat
        oled.text(f"Samples: {sample_count}", 0, 53, 1)
        if heartbeat:
            oled.fill_rect(120, 54, 4, 4, 1)
        else:
            oled.rect(120, 54, 4, 4, 1)

        oled.show()

    except OSError as e:
        print(f"[ERROR] Failed to read sensor: {e}")
        oled.fill(0)
        oled.rect(0, 0, 128, 64, 1)
        oled.text("SENSOR ERROR", 16, 15, 1)
        oled.text("Check wiring to", 5, 32, 1)
        oled.text(f"DHT22 (Pin {DHT_PIN})", 5, 45, 1)
        oled.show()

    time.sleep(2) # DHT22 needs at least 2 seconds between readings
