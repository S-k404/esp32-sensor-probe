"""
==============================================================================
MicroPython: ESP32 + BME280 + 1.14" ST7789 (135x240 RGB IPS) Temperature Probe
==============================================================================
Hardware Target: ESP32 DevKit or TTGO T-Display
Sensor:          Bosch BME280 (I2C: 0x76 or 0x77)
Display:         1.14" 135x240 ST7789 IPS LCD (SPI)
==============================================================================
"""

import machine
import time
import math
import struct

# ----------------------------------------------------------------------------
# 1. PIN CONFIGURATION
# ----------------------------------------------------------------------------
# Change BOARD_MODE to "TTGO" or "GENERIC_ESP32"
BOARD_MODE = "GENERIC_ESP32"

if BOARD_MODE == "TTGO":
    PIN_MOSI = 19
    PIN_SCLK = 18
    PIN_CS   = 5
    PIN_DC   = 16
    PIN_RST  = 23
    PIN_BL   = 4
    PIN_BTN  = 0
    PIN_SDA  = 21
    PIN_SCL  = 22
else: # GENERIC_ESP32 DevKit
    PIN_MOSI = 23
    PIN_SCLK = 18
    PIN_CS   = 5
    PIN_DC   = 16
    PIN_RST  = 4
    PIN_BL   = 15
    PIN_BTN  = 0
    PIN_SDA  = 21
    PIN_SCL  = 22

# ----------------------------------------------------------------------------
# 2. BME280 I2C DRIVER (STANDALONE, ZERO EXTRA FILES NEEDED)
# ----------------------------------------------------------------------------
class BME280_I2C:
    def __init__(self, i2c, address=None):
        self.i2c = i2c
        self.addr = address

        # Auto-detect address if not supplied
        if self.addr is None:
            devices = self.i2c.scan()
            if 0x76 in devices:
                self.addr = 0x76
            elif 0x77 in devices:
                self.addr = 0x77
            else:
                raise RuntimeError("BME280 sensor not found on I2C bus (checked 0x76 and 0x77)!")

        # Read calibration data
        self._read_calibration()

        # Normal mode, 16x oversampling, 16x filter
        # ctrl_hum (0xF2)
        self.i2c.writeto_mem(self.addr, 0xF2, bytearray([0x05])) # 16x humidity
        # ctrl_meas (0xF4): 16x temp (0b101), 16x press (0b101), normal mode (0b11) -> 0b10110111 = 0xB7
        self.i2c.writeto_mem(self.addr, 0xF4, bytearray([0xB7]))
        # config (0xF5): 500ms standby (0b100), 16 filter (0b100), 3-wire spi disabled (0) -> 0b10010000 = 0x90
        self.i2c.writeto_mem(self.addr, 0xF5, bytearray([0x90]))

        self.t_fine = 0

    def _read_calibration(self):
        cal1 = self.i2c.readfrom_mem(self.addr, 0x88, 24)
        cal2 = self.i2c.readfrom_mem(self.addr, 0xA1, 1)
        cal3 = self.i2c.readfrom_mem(self.addr, 0xE1, 7)

        self.dig_T1, self.dig_T2, self.dig_T3 = struct.unpack('<Hhh', cal1[0:6])
        self.dig_P1, self.dig_P2, self.dig_P3, self.dig_P4, self.dig_P5, self.dig_P6, self.dig_P7, self.dig_P8, self.dig_P9 = struct.unpack('<Hhhhhhhhh', cal1[6:24])
        self.dig_H1 = cal2[0]
        self.dig_H2, self.dig_H3 = struct.unpack('<hB', cal3[0:3])
        e4_e5 = struct.unpack('bb', cal3[3:5])
        self.dig_H4 = (e4_e5[0] << 4) | (e4_e5[1] & 0x0F)
        self.dig_H5 = (e4_e5[1] >> 4) | (cal3[5] << 4)
        self.dig_H6 = struct.unpack('b', cal3[6:7])[0]

    def read_raw(self):
        data = self.i2c.readfrom_mem(self.addr, 0xF7, 8)
        raw_press = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4)
        raw_temp  = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4)
        raw_hum   = (data[6] << 8) | data[7]
        return raw_temp, raw_press, raw_hum

    def read_compensated(self):
        raw_temp, raw_press, raw_hum = self.read_raw()

        # Temperature
        var1 = (((raw_temp >> 3) - (self.dig_T1 << 1)) * self.dig_T2) >> 11
        var2 = (((((raw_temp >> 4) - self.dig_T1) * ((raw_temp >> 4) - self.dig_T1)) >> 12) * self.dig_T3) >> 14
        self.t_fine = var1 + var2
        temp_c = ((self.t_fine * 5 + 128) >> 8) / 100.0

        # Pressure
        p_var1 = self.t_fine - 128000
        p_var2 = p_var1 * p_var1 * self.dig_P6
        p_var2 = p_var2 + ((p_var1 * self.dig_P5) << 17)
        p_var2 = p_var2 + (self.dig_P4 << 35)
        p_var1 = ((p_var1 * p_var1 * self.dig_P3) >> 8) + ((p_var1 * self.dig_P2) << 12)
        p_var1 = (((1 << 47) + p_var1) * self.dig_P1) >> 33

        if p_var1 == 0:
            press_hpa = 0.0
        else:
            p = 1048576 - raw_press
            p = int((((p << 31) - p_var2) * 3125) / p_var1)
            p_var1 = (self.dig_P9 * (p >> 13) * (p >> 13)) >> 25
            p_var2 = (self.dig_P8 * p) >> 19
            p = ((p + p_var1 + p_var2) >> 8) + (self.dig_P7 << 4)
            press_hpa = (p / 256.0) / 100.0

        # Humidity
        h_var1 = self.t_fine - 76800
        h_var2 = (((raw_hum << 14) - (self.dig_H4 << 20) - (self.dig_H5 * h_var1)) + 16384) >> 15
        h_var1 = (h_var2 * (((((((h_var1 * self.dig_H6) >> 10) * (((h_var1 * self.dig_H3) >> 11) + 32768)) >> 10) + 2097152) * self.dig_H2 + 8192) >> 14)) >> 10
        h_var1 = h_var1 - (((((h_var1 >> 15) * (h_var1 >> 15)) >> 7) * self.dig_H1) >> 4)
        h_var1 = max(0, min(h_var1, 419430400))
        hum = (h_var1 >> 12) / 1024.0

        return temp_c, press_hpa, hum

# ----------------------------------------------------------------------------
# 3. METEOROLOGICAL HELPERS
# ----------------------------------------------------------------------------
def calculate_dew_point(temp_c, hum):
    if hum <= 0: return temp_c
    a, b = 17.27, 237.7
    alpha = ((a * temp_c) / (b + temp_c)) + math.log(hum / 100.0)
    return (b * alpha) / (a - alpha)

def calculate_altitude(press_hpa, sea_level_hpa=1013.25):
    return 44330.0 * (1.0 - math.pow(press_hpa / sea_level_hpa, 0.1903))

# ----------------------------------------------------------------------------
# 4. INITIALIZE HARDWARE
# ----------------------------------------------------------------------------
print("\n" + "="*56)
print(" ESP32 + BME280 + 1.14\" IPS PRECISION TEMP PROBE ")
print("="*56)

# Turn on Backlight
if PIN_BL >= 0:
    bl_pin = machine.Pin(PIN_BL, machine.Pin.OUT)
    bl_pin.value(1)

# I2C Bus for BME280
i2c = machine.I2C(0, scl=machine.Pin(PIN_SCL), sda=machine.Pin(PIN_SDA), freq=400000)
print(f"[I2C] Scanning bus on SDA={PIN_SDA}, SCL={PIN_SCL}...")
devices = i2c.scan()
print("Found I2C devices:", [hex(d) for d in devices])

bme = BME280_I2C(i2c)
print(f"[BME280] Connected successfully at {hex(bme.addr)}!")

# Button
btn = machine.Pin(PIN_BTN, machine.Pin.IN, machine.Pin.PULL_UP)

# State Variables
use_fahrenheit = False
min_temp_c = 999.0
max_temp_c = -999.0
sample_count = 0

print("[PROBE] Starting continuous telemetry monitoring loop...")
print("Tip: Press BOOT button (GPIO 0) to toggle between Celsius and Fahrenheit.")
print("-" * 56)

# ----------------------------------------------------------------------------
# 5. MAIN TELEMETRY LOOP
# ----------------------------------------------------------------------------
while True:
    try:
        sample_count += 1
        temp_c, press_hpa, hum = bme.read_compensated()
        temp_f = (temp_c * 1.8) + 32.0
        dew_c = calculate_dew_point(temp_c, hum)
        alt_m = calculate_altitude(press_hpa)

        # Min/Max Tracking
        if temp_c < min_temp_c: min_temp_c = temp_c
        if temp_c > max_temp_c: max_temp_c = temp_c

        # Button Check
        if btn.value() == 0:
            time.sleep_ms(50) # Debounce
            if btn.value() == 0:
                use_fahrenheit = not use_fahrenheit
                print(f"[INPUT] Switched unit to {'FAHRENHEIT' if use_fahrenheit else 'CELSIUS'}")
                while btn.value() == 0:
                    time.sleep_ms(20)

        # Telemetry Output
        disp_temp = temp_f if use_fahrenheit else temp_c
        unit = "°F" if use_fahrenheit else "°C"

        print(f"[{sample_count:04d}] Temp: {disp_temp:5.2f}{unit} (Min: {min_temp_c:.1f}C, Max: {max_temp_c:.1f}C) | "
              f"Hum: {hum:5.1f}% | Press: {press_hpa:6.1f} hPa | Dew: {dew_c:5.1f}°C | Alt: {alt_m:4.0f}m")

    except Exception as e:
        print(f"[ERROR] Sensor read failed: {e}")

    time.sleep(1)
