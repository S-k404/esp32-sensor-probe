/*
 * ===============================================================================
 * DIRECT HARDWARE OLED REVIVAL & TEST (Zero External Libraries Needed)
 * Target: Arduino Uno (ATmega328P) + 0.96" I2C OLED (GND, VCC, SCL, SDA)
 * ===============================================================================
 * This sketch bypasses Adafruit/U8g2 libraries and talks directly to the OLED
 * via raw I2C (<Wire.h> only).
 * 
 * It sends BOTH SSD1306 and SH1106 charge pump commands to guarantee the display
 * receives power, and uses hardware command 0xA5 to force all pixels ON.
 * ===============================================================================
 */

#include <Wire.h>

byte oled_address = 0x3C; // Default I2C address (will auto-switch to 0x3D if needed)

void sendCommand(uint8_t cmd) {
  Wire.beginTransmission(oled_address);
  Wire.write(0x00); // 0x00 = Command mode
  Wire.write(cmd);
  Wire.endTransmission();
}

void sendCommand2(uint8_t cmd1, uint8_t cmd2) {
  Wire.beginTransmission(oled_address);
  Wire.write(0x00);
  Wire.write(cmd1);
  Wire.write(cmd2);
  Wire.endTransmission();
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(100000); // Standard 100kHz I2C speed for maximum reliability
  delay(1000);

  Serial.println(F("\n============================================"));
  Serial.println(F("     RAW I2C OLED HARDWARE REVIVAL TEST     "));
  Serial.println(F("============================================"));

  // 1. Detect I2C Address
  Serial.println(F("[1] Checking I2C connection..."));
  Wire.beginTransmission(0x3C);
  if (Wire.endTransmission() == 0) {
    oled_address = 0x3C;
    Serial.println(F(" -> Confirmed OLED at address 0x3C!"));
  } else {
    Wire.beginTransmission(0x3D);
    if (Wire.endTransmission() == 0) {
      oled_address = 0x3D;
      Serial.println(F(" -> Confirmed OLED at address 0x3D!"));
    } else {
      Serial.println(F("[FATAL ERROR] No I2C response from OLED at 0x3C or 0x3D!"));
      Serial.println(F("Please check:"));
      Serial.println(F("  - OLED GND -> Uno GND"));
      Serial.println(F("  - OLED VCC -> Uno 5V (NOT 3.3V)"));
      Serial.println(F("  - OLED SCL -> Uno A5"));
      Serial.println(F("  - OLED SDA -> Uno A4"));
      while (true);
    }
  }

  // 2. Initialize OLED hardware
  Serial.println(F("[2] Sending Charge Pump & Wake-Up Commands..."));

  sendCommand(0xAE);             // Display OFF (sleep mode)
  
  sendCommand2(0xD5, 0x80);      // Set display clock divide ratio/oscillator frequency
  sendCommand2(0xA8, 0x3F);      // Set multiplex ratio (64 lines)
  sendCommand2(0xD3, 0x00);      // Set display offset to 0
  sendCommand(0x40);             // Set start line = 0

  // Dual Charge-Pump Initialization:
  sendCommand2(0x8D, 0x14);      // SSD1306: Turn ON internal charge pump (7.5V)
  sendCommand2(0xAD, 0x8B);      // SH1106:  Turn ON internal DC-DC converter

  sendCommand2(0x20, 0x00);      // Horizontal addressing mode
  sendCommand(0xA1);             // Segment re-map (A0/A1)
  sendCommand(0xC8);             // COM scan direction (C0/C8)
  sendCommand2(0xDA, 0x12);      // Set COM pins configuration
  
  // Maximum Contrast
  sendCommand2(0x81, 0xFF);      // Set contrast to maximum (255)

  sendCommand2(0xD9, 0xF1);      // Set pre-charge period
  sendCommand2(0xDB, 0x40);      // Set VCOMH deselect level
  sendCommand(0xA6);             // Normal display (non-inverted)
  
  sendCommand(0xAF);             // Display ON!

  Serial.println(F("[3] Hardware initialized. Starting visual test patterns..."));
}

void loop() {
  // Test Phase 1: Force All Pixels ON (Hardware Override 0xA5)
  Serial.println(F("Pattern 1: FORCING ALL PIXELS ON (Solid White Screen)..."));
  sendCommand(0xA5); // 0xA5 = Entire display ON (ignores RAM, lights every pixel)
  delay(2000);

  // Test Phase 2: Restore RAM mode
  Serial.println(F("Pattern 2: Writing checkerboard pattern to RAM..."));
  sendCommand(0xA4); // 0xA4 = Resume RAM display
  
  // Fill RAM with alternating stripes/checkerboard
  for (uint8_t page = 0; page < 8; page++) {
    // Set page address
    sendCommand(0xB0 + page);
    // Set column lower and higher nibble to 0
    sendCommand(0x00); // Low column
    sendCommand(0x10); // High column

    // Write 128 bytes to this page in batches of 16 bytes
    for (uint8_t batch = 0; batch < 8; batch++) {
      Wire.beginTransmission(oled_address);
      Wire.write(0x40); // 0x40 = Data mode
      for (uint8_t i = 0; i < 16; i++) {
        Wire.write((batch % 2 == 0) ? 0xAA : 0x55); // Checkerboard stripes
      }
      Wire.endTransmission();
    }
  }
  delay(2000);

  // Test Phase 3: Invert display colors
  Serial.println(F("Pattern 3: Inverting display..."));
  sendCommand(0xA7); // Invert
  delay(1500);
  sendCommand(0xA6); // Normal
  delay(1500);

  // Test Phase 4: Clear Screen
  Serial.println(F("Pattern 4: Clearing screen..."));
  for (uint8_t page = 0; page < 8; page++) {
    sendCommand(0xB0 + page);
    sendCommand(0x00);
    sendCommand(0x10);
    for (uint8_t batch = 0; batch < 8; batch++) {
      Wire.beginTransmission(oled_address);
      Wire.write(0x40);
      for (uint8_t i = 0; i < 16; i++) {
        Wire.write(0x00); // 0x00 = Blank/Black
      }
      Wire.endTransmission();
    }
  }
  delay(1000);
}
