/*
 * ===============================================================================
 * Utility: I2C Bus Scanner
 * Use this to verify your OLED display is wired correctly and find its address!
 * Usually SSD1306 displays respond at 0x3C or 0x3D.
 * ===============================================================================
 */

#include <Wire.h>

void setup() {
  Wire.begin();
  Serial.begin(115200);
  while (!Serial); // Wait for serial monitor (for native USB boards like Leonardo/Pico)
  Serial.println(F("\n=================================="));
  Serial.println(F("       I2C Bus Address Scanner     "));
  Serial.println(F("=================================="));
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println(F("Scanning I2C bus..."));

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print(F(" -> Device found at 7-bit address 0x"));
      if (address < 16) Serial.print(F("0"));
      Serial.print(address, HEX);

      if (address == 0x3C || address == 0x3D) {
        Serial.print(F(" (Matches standard SSD1306 OLED)"));
      }
      Serial.println();
      nDevices++;
    } else if (error == 4) {
      Serial.print(F(" -> Unknown error at address 0x"));
      if (address < 16) Serial.print(F("0"));
      Serial.println(address, HEX);
    }
  }

  if (nDevices == 0) {
    Serial.println(F("[!] No I2C devices found. Check SDA/SCL connections and pull-ups."));
  } else {
    Serial.print(F("[OK] Scan done. Found "));
    Serial.print(nDevices);
    Serial.println(F(" device(s)."));
  }

  Serial.println(F("Waiting 5 seconds before next scan...\n"));
  delay(5000);
}
