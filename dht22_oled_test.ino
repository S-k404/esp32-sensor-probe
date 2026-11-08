/*
 * ===============================================================================
 * Arduino Uno DHT22 + I2C OLED Monitor (High-Clarity Display Version)
 * File: weather_station.ino
 * ===============================================================================
 * Enhancements:
 * - Wide Character Kerning (Prevents characters from touching or merging)
 * - Anti-Ghosting Page Clearing on every refresh
 * - Centered, Large 2x Scaled Digits with Proper Spacing
 * - Zero External Libraries (<Wire.h> only)
 * ===============================================================================
 */

#include <Wire.h>
#include <avr/pgmspace.h>

const int DHT_PIN = 2;
byte oled_address = 0x3C;
unsigned long lastReadTime = 0;
unsigned long sampleCount = 0;
bool heartbeat = false;

const uint8_t font5x7[][5] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x5F, 0x00, 0x00,
  0x00, 0x07, 0x00, 0x07, 0x00,
  0x14, 0x7F, 0x14, 0x7F, 0x14,
  0x24, 0x2A, 0x7F, 0x2A, 0x12,
  0x23, 0x13, 0x08, 0x64, 0x62,
  0x36, 0x49, 0x55, 0x22, 0x50,
  0x00, 0x05, 0x03, 0x00, 0x00,
  0x00, 0x1C, 0x22, 0x41, 0x00,
  0x00, 0x41, 0x22, 0x1C, 0x00,
  0x08, 0x2A, 0x1C, 0x2A, 0x08,
  0x08, 0x08, 0x3E, 0x08, 0x08,
  0x00, 0x50, 0x30, 0x00, 0x00,
  0x08, 0x08, 0x08, 0x08, 0x08,
  0x00, 0x60, 0x60, 0x00, 0x00,
  0x20, 0x10, 0x08, 0x04, 0x02,
  0x3E, 0x51, 0x49, 0x45, 0x3E,
  0x00, 0x42, 0x7F, 0x40, 0x00,
  0x42, 0x61, 0x51, 0x49, 0x46,
  0x21, 0x41, 0x45, 0x4B, 0x31,
  0x18, 0x14, 0x12, 0x7F, 0x10,
  0x27, 0x45, 0x45, 0x45, 0x39,
  0x3C, 0x4A, 0x49, 0x49, 0x30,
  0x01, 0x71, 0x09, 0x05, 0x03,
  0x36, 0x49, 0x49, 0x49, 0x36,
  0x06, 0x49, 0x49, 0x29, 0x1E,
  0x00, 0x36, 0x36, 0x00, 0x00,
  0x00, 0x56, 0x36, 0x00, 0x00,
  0x00, 0x08, 0x14, 0x22, 0x41,
  0x14, 0x14, 0x14, 0x14, 0x14,
  0x41, 0x22, 0x14, 0x08, 0x00,
  0x02, 0x01, 0x51, 0x09, 0x06,
  0x32, 0x49, 0x79, 0x41, 0x3E,
  0x7E, 0x11, 0x11, 0x11, 0x7E,
  0x7F, 0x49, 0x49, 0x49, 0x36,
  0x3E, 0x41, 0x41, 0x41, 0x22,
  0x7F, 0x41, 0x41, 0x22, 0x1C,
  0x7F, 0x49, 0x49, 0x49, 0x41,
  0x7F, 0x09, 0x09, 0x09, 0x01,
  0x3E, 0x41, 0x49, 0x49, 0x7A,
  0x7F, 0x08, 0x08, 0x08, 0x7F,
  0x00, 0x41, 0x7F, 0x41, 0x00,
  0x20, 0x40, 0x41, 0x3F, 0x01,
  0x7F, 0x08, 0x14, 0x22, 0x41,
  0x7F, 0x40, 0x40, 0x40, 0x40,
  0x7F, 0x02, 0x04, 0x02, 0x7F,
  0x7F, 0x04, 0x08, 0x10, 0x7F,
  0x3E, 0x41, 0x41, 0x41, 0x3E,
  0x7F, 0x09, 0x09, 0x09, 0x06,
  0x3E, 0x41, 0x51, 0x21, 0x5E,
  0x7F, 0x09, 0x19, 0x29, 0x46,
  0x46, 0x49, 0x49, 0x49, 0x31,
  0x01, 0x01, 0x7F, 0x01, 0x01,
  0x3F, 0x40, 0x40, 0x40, 0x3F,
  0x1F, 0x20, 0x40, 0x20, 0x1F,
  0x7F, 0x20, 0x18, 0x20, 0x7F,
  0x63, 0x14, 0x08, 0x14, 0x63,
  0x03, 0x04, 0x78, 0x04, 0x03,
  0x61, 0x51, 0x49, 0x45, 0x43,
  0x20, 0x54, 0x54, 0x54, 0x78,
  0x7F, 0x48, 0x44, 0x44, 0x38,
  0x38, 0x44, 0x44, 0x44, 0x20,
  0x38, 0x44, 0x44, 0x48, 0x7F,
  0x38, 0x54, 0x54, 0x54, 0x18,
  0x08, 0x7E, 0x09, 0x01, 0x02,
  0x08, 0x14, 0x54, 0x54, 0x3C,
  0x7F, 0x08, 0x04, 0x04, 0x78,
  0x00, 0x44, 0x7D, 0x40, 0x00,
  0x20, 0x40, 0x44, 0x3D, 0x00,
  0x7F, 0x10, 0x28, 0x44, 0x00,
  0x00, 0x41, 0x7F, 0x40, 0x00,
  0x7C, 0x04, 0x18, 0x04, 0x78,
  0x7C, 0x08, 0x04, 0x04, 0x78,
  0x38, 0x44, 0x44, 0x44, 0x38,
  0x7C, 0x14, 0x14, 0x14, 0x08,
  0x08, 0x14, 0x14, 0x18, 0x7C,
  0x7C, 0x08, 0x04, 0x04, 0x08,
  0x48, 0x54, 0x54, 0x54, 0x20,
  0x04, 0x3F, 0x44, 0x40, 0x20,
  0x3C, 0x40, 0x40, 0x20, 0x7C,
  0x1C, 0x20, 0x40, 0x20, 0x1C,
  0x3C, 0x40, 0x30, 0x40, 0x3C,
  0x44, 0x28, 0x10, 0x28, 0x44,
  0x0C, 0x50, 0x50, 0x50, 0x3C,
  0x44, 0x64, 0x54, 0x4C, 0x44,
  0x00, 0x08, 0x36, 0x41, 0x00,
  0x00, 0x00, 0x7F, 0x00, 0x00,
  0x00, 0x41, 0x36, 0x08, 0x00,
  0x08, 0x08, 0x2A, 0x1C, 0x08,
  0x00, 0x06, 0x09, 0x09, 0x06,
};

// 2x vertical scale helper
uint8_t scaleNibble(uint8_t n) {
  uint8_t out = 0;
  if (n & 0x01) out |= 0x03;
  if (n & 0x02) out |= 0x0C;
  if (n & 0x04) out |= 0x30;
  if (n & 0x08) out |= 0xC0;
  return out;
}

// -------------------------------------------------------------
// OLED Low-Level Drivers
// -------------------------------------------------------------
void oledCommand(uint8_t cmd) {
  Wire.beginTransmission(oled_address);
  Wire.write(0x00);
  Wire.write(cmd);
  Wire.endTransmission();
}

void oledCommand2(uint8_t cmd1, uint8_t cmd2) {
  Wire.beginTransmission(oled_address);
  Wire.write(0x00);
  Wire.write(cmd1);
  Wire.write(cmd2);
  Wire.endTransmission();
}

void oledSetCursor(uint8_t page, uint8_t col) {
  oledCommand(0xB0 + (page & 0x07));
  oledCommand(0x00 | (col & 0x0F));
  oledCommand(0x10 | ((col >> 4) & 0x0F));
}

// Clear single page (removes ghosting / merged text)
void oledClearLine(uint8_t page) {
  oledSetCursor(page, 0);
  for (uint8_t batch = 0; batch < 8; batch++) {
    Wire.beginTransmission(oled_address);
    Wire.write(0x40);
    for (uint8_t i = 0; i < 16; i++) Wire.write(0x00);
    Wire.endTransmission();
  }
}

void oledClear() {
  for (uint8_t page = 0; page < 8; page++) {
    oledClearLine(page);
  }
}

// Print standard character with 2-pixel separation gap (prevents merging)
void oledPrintChar(char c, bool invert = false) {
  uint8_t idx = 0;
  if (c >= 32 && c <= 126) {
    idx = c - 32;
  } else if ((uint8_t)c == 248 || (uint8_t)c == 247 || c == '^' || (uint8_t)c == 0xB0) {
    idx = 95; // Degree symbol
  }

  Wire.beginTransmission(oled_address);
  Wire.write(0x40);
  for (uint8_t col = 0; col < 5; col++) {
    uint8_t b = pgm_read_byte(&font5x7[idx][col]);
    Wire.write(invert ? ~b : b);
  }
  // 2-pixel blank gap between characters for maximum clarity
  Wire.write(invert ? 0xFF : 0x00);
  Wire.write(invert ? 0xFF : 0x00);
  Wire.endTransmission();
}

void oledPrint(const char* str, bool invert = false) {
  while (*str) {
    oledPrintChar(*str++, invert);
  }
}

// Print large 2x character with 4-pixel separation gap
void oledPrint2xChar(uint8_t topPage, uint8_t col, char c) {
  uint8_t idx = 0;
  if (c >= 32 && c <= 126) {
    idx = c - 32;
  } else if ((uint8_t)c == 248 || (uint8_t)c == 247 || c == '^' || (uint8_t)c == 0xB0) {
    idx = 95; // Degree symbol
  }

  // Top Half
  oledSetCursor(topPage, col);
  Wire.beginTransmission(oled_address);
  Wire.write(0x40);
  for (uint8_t i = 0; i < 5; i++) {
    uint8_t b = pgm_read_byte(&font5x7[idx][i]);
    uint8_t topB = scaleNibble(b & 0x0F);
    Wire.write(topB);
    Wire.write(topB);
  }
  // 4-pixel clear gap
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.endTransmission();

  // Bottom Half
  oledSetCursor(topPage + 1, col);
  Wire.beginTransmission(oled_address);
  Wire.write(0x40);
  for (uint8_t i = 0; i < 5; i++) {
    uint8_t b = pgm_read_byte(&font5x7[idx][i]);
    uint8_t botB = scaleNibble((b >> 4) & 0x0F);
    Wire.write(botB);
    Wire.write(botB);
  }
  // 4-pixel clear gap
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.endTransmission();
}

void oledPrint2x(uint8_t topPage, uint8_t startCol, const char* str) {
  uint8_t col = startCol;
  while (*str) {
    oledPrint2xChar(topPage, col, *str++);
    col += 14; // 10 pixels character + 4 pixels gap
  }
}

bool oledInit() {
  Wire.beginTransmission(0x3C);
  if (Wire.endTransmission() == 0) {
    oled_address = 0x3C;
  } else {
    Wire.beginTransmission(0x3D);
    if (Wire.endTransmission() == 0) {
      oled_address = 0x3D;
    } else {
      return false;
    }
  }

  oledCommand(0xAE);             // Display OFF
  oledCommand2(0xD5, 0x80);      // Clock divide
  oledCommand2(0xA8, 0x3F);      // Multiplex ratio (64)
  oledCommand2(0xD3, 0x00);      // Offset 0
  oledCommand(0x40);             // Start line 0

  // Turn ON both charge pumps (SSD1306 + SH1106)
  oledCommand2(0x8D, 0x14);      // SSD1306 Charge Pump ON
  oledCommand2(0xAD, 0x8B);      // SH1106 DC-DC Converter ON

  oledCommand2(0x20, 0x00);      // Horizontal address mode
  oledCommand(0xA1);             // Segment remap
  oledCommand(0xC8);             // COM direction
  oledCommand2(0xDA, 0x12);      // COM pins configuration
  oledCommand2(0x81, 0xFF);      // Max contrast
  oledCommand2(0xD9, 0xF1);      // Pre-charge
  oledCommand2(0xDB, 0x40);      // VCOMH
  oledCommand(0xA4);             // Output follows RAM
  oledCommand(0xA6);             // Normal display
  oledCommand(0xAF);             // Display ON!

  oledClear();
  return true;
}

// -------------------------------------------------------------
// DHT22 Driver (Exact timing that works for your hardware)
// -------------------------------------------------------------
bool readDHT22(float &temperature, float &humidity) {
  uint8_t data[5] = {0, 0, 0, 0, 0};

  // 1. Send start signal: pull LOW for 1.1 ms
  pinMode(DHT_PIN, OUTPUT);
  digitalWrite(DHT_PIN, LOW);
  delayMicroseconds(1100);

  // 2. Release line: pull HIGH for 30 microseconds
  digitalWrite(DHT_PIN, HIGH);
  delayMicroseconds(30);
  pinMode(DHT_PIN, INPUT_PULLUP);

  // 3. Wait for sensor response
  unsigned long timeout = micros();
  while (digitalRead(DHT_PIN) == HIGH) {
    if (micros() - timeout > 100) return false;
  }
  timeout = micros();
  while (digitalRead(DHT_PIN) == LOW) {
    if (micros() - timeout > 100) return false;
  }
  timeout = micros();
  while (digitalRead(DHT_PIN) == HIGH) {
    if (micros() - timeout > 100) return false;
  }

  // 4. Read 40 data bits
  for (int i = 0; i < 40; i++) {
    timeout = micros();
    while (digitalRead(DHT_PIN) == LOW) {
      if (micros() - timeout > 100) return false;
    }

    unsigned long pulseStart = micros();
    timeout = micros();
    while (digitalRead(DHT_PIN) == HIGH) {
      if (micros() - timeout > 100) return false;
    }
    unsigned long pulseLen = micros() - pulseStart;

    if (pulseLen > 40) {
      data[i / 8] |= (1 << (7 - (i % 8)));
    }
  }

  // 5. Verify checksum
  if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
    return false;
  }

  // 6. Decode values
  humidity = ((data[0] << 8) | data[1]) * 0.1;
  int16_t rawTemp = ((data[2] & 0x7F) << 8) | data[3];
  temperature = rawTemp * 0.1;
  if (data[2] & 0x80) {
    temperature = -temperature;
  }

  return true;
}

// -------------------------------------------------------------
// OLED Screen Drawing
// -------------------------------------------------------------
void drawHeader() {
  oledSetCursor(0, 0);
  // Centered header banner with inverted colors
  oledPrint(" WEATHER MONITOR  ", true);
  for (int i = 0; i < 2; i++) {
    Wire.beginTransmission(oled_address);
    Wire.write(0x40);
    Wire.write(0xFF);
    Wire.endTransmission();
  }
}

void showReadings(float tempC, float humidity) {
  // Clear pages 1 through 7 cleanly before redrawing
  // This completely eliminates any ghosting or merged characters!
  for (uint8_t p = 1; p < 8; p++) {
    oledClearLine(p);
  }

  drawHeader();

  // Format strings
  char tempBuf[8];
  char humidBuf[8];
  dtostrf(tempC, 4, 1, tempBuf);
  dtostrf(humidity, 4, 1, humidBuf);

  // --- Temperature Section ---
  oledSetCursor(1, 2);
  oledPrint("TEMP:");

  // Large centered temperature digits with wide kerning
  oledPrint2x(2, 16, tempBuf);
  oledPrint2xChar(2, 16 + strlen(tempBuf) * 14, '^'); // Degree symbol
  oledPrint2xChar(2, 16 + (strlen(tempBuf) + 1) * 14, 'C');

  // --- Humidity Section ---
  oledSetCursor(4, 2);
  oledPrint("HUMIDITY:");

  // Large centered humidity digits with wide kerning
  oledPrint2x(5, 24, humidBuf);
  oledPrint2xChar(5, 24 + strlen(humidBuf) * 14, '%');

  // --- Bottom Status Bar ---
  oledSetCursor(7, 2);
  oledPrint("STATUS: OK ");
  oledPrintChar(heartbeat ? '*' : 'o');
  oledPrint("  READ:");
  char cnt[6];
  itoa(sampleCount, cnt, 10);
  oledPrint(cnt);
}

void showError() {
  for (uint8_t p = 1; p < 8; p++) {
    oledClearLine(p);
  }
  drawHeader();
  oledSetCursor(2, 8);
  oledPrint("! SENSOR ERROR !");
  oledSetCursor(4, 8);
  oledPrint("Failed to read DHT22");
  oledSetCursor(5, 8);
  oledPrint("Check Pin 2 wire");
  oledSetCursor(7, 8);
  oledPrint("Retrying in 2s...");
}

// -------------------------------------------------------------
// Setup & Loop
// -------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(100000);
  delay(1000);

  Serial.println(F("===================================="));
  Serial.println(F("   Arduino Uno: DHT22 + OLED        "));
  Serial.println(F("===================================="));

  if (!oledInit()) {
    Serial.println(F("[ERROR] OLED not found!"));
    while (true);
  }
  Serial.println(F("[OK] OLED initialized."));

  drawHeader();
  oledSetCursor(3, 10);
  oledPrint("Starting DHT22...");
  delay(2000);
  oledClear();
}

void loop() {
  if (millis() - lastReadTime >= 2000) {
    lastReadTime = millis();
    sampleCount++;
    heartbeat = !heartbeat;

    float temperature = 0.0;
    float humidity = 0.0;

    if (readDHT22(temperature, humidity)) {
      float tempF = temperature * 1.8 + 32.0;

      // Print to Serial Monitor
      Serial.print(F("Sample #"));
      Serial.print(sampleCount);
      Serial.print(F(" | Temp: "));
      Serial.print(temperature, 1);
      Serial.print(F(" C ("));
      Serial.print(tempF, 1);
      Serial.print(F(" F) | Humidity: "));
      Serial.print(humidity, 1);
      Serial.println(F(" %"));

      // Display on OLED with high clarity & clean spacing
      showReadings(temperature, humidity);
    } else {
      Serial.println(F("[WARNING] Failed to read from DHT22!"));
      showError();
    }
  }
}
