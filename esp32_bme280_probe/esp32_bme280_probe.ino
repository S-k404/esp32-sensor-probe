/*
 * ==============================================================================
 *  ESP32 + BME280 + 1.14" (135x240 RGB IPS) PRECISION TEMPERATURE PROBE
 * ==============================================================================
 *  Hardware Target: ESP32 (DevKit / NodeMCU-32S / WROOM-32 OR TTGO T-Display)
 *  Sensor:          Bosch BME280 (Temperature, Humidity, Barometric Pressure)
 *  Display:         1.14" 135x240 Full-Color IPS LCD (ST7789 Controller)
 * ==============================================================================
 *  Features:
 *  - 4 Dynamic Interactive Display Modes (Dashboard, Trend Graph, Diagnostics, Big Digits)
 *  - High-precision Temperature (°C / °F), Humidity (%), Barometric Pressure (hPa/inHg)
 *  - Calculated Dew Point, Heat Index ("Feels Like"), Altitude (m/ft), Vapor Metrics
 *  - Live Min / Max Thermal Peak Memory & Reset Feature
 *  - 48-point Rolling Real-Time Temperature Sparkline Graph
 *  - Dynamic Thermal Alert Colors (Ice Blue < 18°C, Emerald 18-26°C, Amber 26-32°C, Coral > 32°C)
 *  - Zero-Flicker Differential Rendering Engine (overwrites values, no fillScreen flashes)
 *  - Auto-Detection for BME280 on both I2C addresses (0x76 and 0x77)
 *  - Auto-Recovery with On-Screen Diagnostics if sensor cable is disconnected
 *  - Single-Button Navigation (cycles screens on short press, toggles °C/°F on long press)
 * ==============================================================================
 */

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// ==============================================================================
// 1. HARDWARE CONFIGURATION & BOARD PROFILES
// ==============================================================================
#define PROFILE_GENERIC_ESP32   1   // Standard ESP32 DevKit + Separate 1.14" ST7789 SPI Module
#define PROFILE_TTGO_T_DISPLAY  2   // LilyGO TTGO T-Display (ESP32 with built-in 1.14" ST7789)

// >>> SELECT YOUR BOARD HERE <<<
#define BOARD_PROFILE PROFILE_GENERIC_ESP32

#if (BOARD_PROFILE == PROFILE_TTGO_T_DISPLAY)
  // LilyGO TTGO T-Display Internal Pin Routing
  #define TFT_MOSI         19
  #define TFT_SCLK         18
  #define TFT_CS            5
  #define TFT_DC           16
  #define TFT_RST          23
  #define TFT_BL            4   // Backlight Pin (Active HIGH)
  #define BTN_PRIMARY       0   // Left Button (GPIO 0, Active LOW)
  #define BTN_SECONDARY    35   // Right Button (GPIO 35, Active LOW)
  #define I2C_SDA          21   // BME280 SDA
  #define I2C_SCL          22   // BME280 SCL
#else
  // Standard ESP32 DevKit + Standalone 1.14" ST7789 SPI Module
  // (Uses ESP32 Hardware VSPI pins for blazing fast refresh rates)
  #define TFT_MOSI         23   // VSPI MOSI (SDA/DIN on display)
  #define TFT_SCLK         18   // VSPI SCK  (SCL/CLK on display)
  #define TFT_CS            5   // Chip Select (CS)
  #define TFT_DC           16   // Data/Command (DC)
  #define TFT_RST           4   // Reset (RST/RES)
  #define TFT_BL           15   // Backlight control (set to -1 if connected directly to 3.3V)
  #define BTN_PRIMARY       0   // Built-in BOOT Button on every ESP32 (Active LOW)
  #define BTN_SECONDARY    35   // Optional External Button or GPIO 35 (Active LOW)
  #define I2C_SDA          21   // BME280 SDA
  #define I2C_SCL          22   // BME280 SCL
#endif

// Display Geometry & Color Inversion
#define SCREEN_W          240
#define SCREEN_H          135
#define DISPLAY_ROTATION    1   // 1 = Landscape (Ribbon Left), 3 = Landscape (Ribbon Right)
#define INVERT_COLORS    true   // ST7789 IPS panels almost always require invertDisplay(true)

// Sensor & Measurement Settings
#define SEALEVELPRESSURE_HPA (1013.25)
#define TEMP_CAL_OFFSET_C    (0.0f)   // Temperature calibration adjustment (e.g., -0.5 or +0.2)
#define SENSOR_POLL_MS       (1000)   // Sensor poll & display refresh period (ms)
#define GRAPH_HISTORY_LEN      (48)   // Number of history points for sparkline graph

// ==============================================================================
// 2. COLOR PALETTE (RGB565 Tailored High-Contrast Palette)
// ==============================================================================
#define COL_BG          0x0821   // Deep Slate Navy (#0a0d18)
#define COL_CARD_BG     0x10C3   // Dark Obsidian Card (#101924)
#define COL_CARD_BORDER 0x2965   // Muted Slate Border
#define COL_TEXT_WHITE  0xFFFF   // Crisp White
#define COL_TEXT_DIM    0x8410   // Cool Silver/Grey
#define COL_ACCENT      0x05FF   // Electric Cyan
#define COL_TEMP_COLD   0x3D7F   // Ice Blue (< 18.0°C)
#define COL_TEMP_GOOD   0x27E8   // Emerald / Spring Green (18.0°C - 26.0°C)
#define COL_TEMP_WARM   0xFDA0   // Warm Amber (26.0°C - 32.0°C)
#define COL_TEMP_HOT    0xF986   // Vivid Coral Red (> 32.0°C)
#define COL_HUMID_BLUE  0x35BF   // Sky Blue
#define COL_PRESS_LILAC 0xCE7F   // Soft Lilac Purple
#define COL_ALERT_RED   0xF800   // Warning Red
#define COL_GRID_LINE   0x18E3   // Graph grid dots

// ==============================================================================
// 3. GLOBAL OBJECTS & STATE VARIABLES
// ==============================================================================
// Standard Hardware SPI using ESP32 GPIO Matrix pin reassignment
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

Adafruit_BME280 bme;
uint8_t bmeAddress = 0x00;
bool sensorHealthy = false;

// Display Modes
enum DisplayMode {
  MODE_DASHBOARD = 0,    // Primary probe instruments & cards
  MODE_TREND_GRAPH,      // Real-time thermal sparkline & slope
  MODE_DIAGNOSTICS,      // Meteorology calculations & hardware metrics
  MODE_BIG_DIGITS,       // Ultra-high visibility field probe mode
  MODE_COUNT
};

DisplayMode currentMode = MODE_DASHBOARD;
bool modeNeedsFullRedraw = true;
bool useFahrenheit = false;

// Sensor Readings & Metrics
float currentTempC = 0.0f;
float currentTempF = 0.0f;
float currentHum   = 0.0f;
float currentPress = 0.0f;
float currentInHg  = 0.0f;
float currentAlt   = 0.0f;
float currentDewPt = 0.0f;
float currentHeatIdx = 0.0f;

// Min / Max Records
float minTempC = 999.0f;
float maxTempC = -999.0f;
float minHum   = 100.0f;
float maxHum   = 0.0f;

// History Graph Buffer
float tempHistory[GRAPH_HISTORY_LEN];
int historyCount = 0;
int historyIndex = 0;

// Timing & Buttons
unsigned long lastSensorReadTime = 0;
unsigned long totalSamples = 0;
bool heartbeatState = false;

unsigned long btnPressStartTime = 0;
bool btnWasPressed = false;
unsigned long btn2PressStartTime = 0;
bool btn2WasPressed = false;

// ==============================================================================
// 4. SCIENTIFIC & METEOROLOGICAL CALCULATIONS
// ==============================================================================
// Magnus-Tetens approximation for Dew Point calculation
float calculateDewPoint(float tempC, float hum) {
  if (hum <= 0.0f) return tempC;
  const float a = 17.27f;
  const float b = 237.7f;
  float alpha = ((a * tempC) / (b + tempC)) + log(hum / 100.0f);
  return (b * alpha) / (a - alpha);
}

// NOAA Heat Index ("Feels Like") calculation
float calculateHeatIndex(float tempC, float hum) {
  float tempF = (tempC * 1.8f) + 32.0f;
  if (tempF < 80.0f) return tempC; // Heat index is only meaningful above 80°F (26.7°C)

  float c1 = -42.379f, c2 = 2.04901523f, c3 = 10.14333127f;
  float c4 = -0.22475541f, c5 = -0.00683783f, c6 = -0.05481717f;
  float c7 = 0.00122874f, c8 = 0.00085282f, c9 = -0.00000199f;

  float hiF = c1 + (c2 * tempF) + (c3 * hum) + (c4 * tempF * hum)
            + (c5 * tempF * tempF) + (c6 * hum * hum)
            + (c7 * tempF * tempF * hum) + (c8 * tempF * hum * hum)
            + (c9 * tempF * tempF * hum * hum);

  return (hiF - 32.0f) / 1.8f;
}

// Color coding based on temperature
uint16_t getTemperatureColor(float tempC) {
  if (tempC < 18.0f) return COL_TEMP_COLD;
  if (tempC <= 26.0f) return COL_TEMP_GOOD;
  if (tempC <= 32.0f) return COL_TEMP_WARM;
  return COL_TEMP_HOT;
}

const char* getComfortLabel(float tempC, float hum) {
  if (tempC < 16.0f) return "CHILLY";
  if (tempC > 32.0f) return "HOT";
  if (hum > 70.0f) return "HUMID";
  if (hum < 30.0f) return "DRY AIR";
  if (tempC >= 19.0f && tempC <= 25.0f && hum >= 35.0f && hum <= 65.0f) return "OPTIMAL";
  return "STABLE";
}

// ==============================================================================
// 5. GRAPHICS HELPERS & DRAWING ROUTINES
// ==============================================================================
void drawDegreeSymbol(int x, int y, uint16_t color) {
  tft.drawCircle(x, y, 2, color);
}

void drawTopStatusBar(const char* title) {
  // Pill header
  tft.fillRoundRect(2, 2, 80, 16, 3, COL_CARD_BG);
  tft.drawRoundRect(2, 2, 80, 16, 3, COL_ACCENT);
  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT_WHITE, COL_CARD_BG);
  tft.setCursor(8, 6);
  tft.print(title);

  // Min / Max summary in center
  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.setCursor(90, 6);
  float dispMin = useFahrenheit ? (minTempC * 1.8f + 32.0f) : minTempC;
  float dispMax = useFahrenheit ? (maxTempC * 1.8f + 32.0f) : maxTempC;
  tft.print("L:");
  tft.print(dispMin, 1);
  tft.print(" H:");
  tft.print(dispMax, 1);

  // Unit Badge
  tft.fillRoundRect(192, 2, 28, 16, 3, COL_CARD_BG);
  tft.drawRoundRect(192, 2, 28, 16, 3, COL_CARD_BORDER);
  tft.setTextColor(COL_ACCENT, COL_CARD_BG);
  tft.setCursor(196, 6);
  tft.print(useFahrenheit ? "[ F]" : "[ C]");

  // Heartbeat dot
  uint16_t heartCol = heartbeatState ? COL_TEMP_GOOD : COL_CARD_BORDER;
  tft.fillCircle(230, 9, 3, heartCol);

  // Horizontal separator line
  tft.drawFastHLine(0, 20, SCREEN_W, COL_CARD_BORDER);
}

// ==============================================================================
// 6. SCREEN MODES IMPLEMENTATION
// ==============================================================================

// ------------------------------------------------------------------------------
// MODE 0: PROBE DASHBOARD
// ------------------------------------------------------------------------------
void renderDashboardStatic() {
  tft.fillScreen(COL_BG);
  drawTopStatusBar("PROBE 1.14\"");

  // Card 1: Left Main Temperature Card (x: 2, y: 23, w: 122, h: 90)
  tft.fillRoundRect(2, 23, 122, 90, 4, COL_CARD_BG);
  tft.drawRoundRect(2, 23, 122, 90, 4, COL_CARD_BORDER);
  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT_DIM, COL_CARD_BG);
  tft.setCursor(8, 28);
  tft.print("TEMPERATURE");

  // Card 2: Top-Right Humidity Card (x: 128, y: 23, w: 110, h: 43)
  tft.fillRoundRect(128, 23, 110, 43, 4, COL_CARD_BG);
  tft.drawRoundRect(128, 23, 110, 43, 4, COL_CARD_BORDER);
  tft.setTextSize(1);
  tft.setTextColor(COL_HUMID_BLUE, COL_CARD_BG);
  tft.setCursor(134, 28);
  tft.print("HUMIDITY");

  // Card 3: Bottom-Right Pressure Card (x: 128, y: 70, w: 110, h: 43)
  tft.fillRoundRect(128, 70, 110, 43, 4, COL_CARD_BG);
  tft.drawRoundRect(128, 70, 110, 43, 4, COL_CARD_BORDER);
  tft.setTextSize(1);
  tft.setTextColor(COL_PRESS_LILAC, COL_CARD_BG);
  tft.setCursor(134, 75);
  tft.print("BAROMETER");

  // Bottom Footer Bar (x: 0, y: 116, w: 240, h: 19)
  tft.drawFastHLine(0, 116, SCREEN_W, COL_CARD_BORDER);
}

void renderDashboardDynamic() {
  drawTopStatusBar("PROBE 1.14\"");

  uint16_t tempCol = getTemperatureColor(currentTempC);

  // Update Left Main Temperature Card
  tft.drawRoundRect(2, 23, 122, 90, 4, tempCol); // Dynamic color frame

  float dispTemp = useFahrenheit ? currentTempF : currentTempC;

  // Clear numeric area only to prevent any flicker
  tft.fillRect(6, 40, 114, 38, COL_CARD_BG);
  tft.setTextColor(tempCol, COL_CARD_BG);
  tft.setTextSize(4);
  tft.setCursor(8, 42);
  if (dispTemp >= 100.0f || dispTemp <= -10.0f) {
    tft.setTextSize(3);
    tft.setCursor(8, 46);
  }
  tft.print(dispTemp, 1);

  // Unit Symbol
  tft.setTextSize(2);
  tft.setTextColor(COL_TEXT_DIM, COL_CARD_BG);
  tft.setCursor(96, 42);
  tft.print(useFahrenheit ? "F" : "C");
  drawDegreeSymbol(91, 44, COL_TEXT_DIM);

  // Comfort Pill inside Temperature Card
  const char* comfortStr = getComfortLabel(currentTempC, currentHum);
  tft.fillRoundRect(8, 86, 110, 18, 3, COL_BG);
  tft.drawRoundRect(8, 86, 110, 18, 3, tempCol);
  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT_WHITE, COL_BG);
  int textLen = strlen(comfortStr);
  int startX = 63 - (textLen * 3);
  tft.setCursor(startX, 91);
  tft.print(comfortStr);

  // Update Humidity Card
  tft.fillRect(134, 40, 98, 16, COL_CARD_BG);
  tft.setTextColor(COL_TEXT_WHITE, COL_CARD_BG);
  tft.setTextSize(2);
  tft.setCursor(134, 40);
  tft.print(currentHum, 1);
  tft.setTextSize(1);
  tft.setTextColor(COL_HUMID_BLUE, COL_CARD_BG);
  tft.print(" %");

  // Humidity mini progress bar (w: 96, h: 4)
  int barW = map(constrain((int)currentHum, 0, 100), 0, 100, 0, 96);
  tft.fillRoundRect(134, 58, 96, 4, 2, COL_BG);
  tft.fillRoundRect(134, 58, barW, 4, 2, COL_HUMID_BLUE);

  // Update Pressure Card
  tft.fillRect(134, 87, 98, 16, COL_CARD_BG);
  tft.setTextColor(COL_TEXT_WHITE, COL_CARD_BG);
  tft.setTextSize(2);
  tft.setCursor(134, 87);
  tft.print((int)currentPress);
  tft.setTextSize(1);
  tft.setTextColor(COL_PRESS_LILAC, COL_CARD_BG);
  tft.setCursor(188, 93);
  tft.print("hPa");

  // Bottom Footer: Dew Point & Altitude
  tft.fillRect(2, 118, 236, 15, COL_BG);
  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.setCursor(4, 122);
  tft.print("Dew: ");
  tft.setTextColor(COL_TEXT_WHITE, COL_BG);
  float dispDew = useFahrenheit ? (currentDewPt * 1.8f + 32.0f) : currentDewPt;
  tft.print(dispDew, 1);
  tft.print(useFahrenheit ? "F" : "C");

  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.setCursor(88, 122);
  tft.print("Alt: ");
  tft.setTextColor(COL_TEXT_WHITE, COL_BG);
  tft.print((int)currentAlt);
  tft.print("m");

  tft.setTextColor(COL_ACCENT, COL_BG);
  tft.setCursor(162, 122);
  tft.print("[BOOT:Next]");
}

// ------------------------------------------------------------------------------
// MODE 1: REAL-TIME THERMAL TREND GRAPH
// ------------------------------------------------------------------------------
void renderTrendGraphStatic() {
  tft.fillScreen(COL_BG);
  drawTopStatusBar("THERMAL TREND");

  // Graph Canvas Frame (x: 32, y: 25, w: 202, h: 88)
  tft.fillRoundRect(30, 24, 206, 92, 3, COL_CARD_BG);
  tft.drawRoundRect(30, 24, 206, 92, 3, COL_CARD_BORDER);

  // Grid Lines
  for (int y = 44; y < 110; y += 22) {
    for (int x = 34; x < 232; x += 6) {
      tft.drawPixel(x, y, COL_GRID_LINE);
    }
  }

  // Footer info
  tft.drawFastHLine(0, 118, SCREEN_W, COL_CARD_BORDER);
  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.setCursor(4, 122);
  tft.print("48 Samples Rolling | Rate: ");
}

void renderTrendGraphDynamic() {
  drawTopStatusBar("THERMAL TREND");

  // Find min & max within recent history window for dynamic auto-scaling
  float localMin = currentTempC;
  float localMax = currentTempC;

  int points = min(historyCount, GRAPH_HISTORY_LEN);
  if (points > 0) {
    localMin = tempHistory[0];
    localMax = tempHistory[0];
    for (int i = 1; i < points; i++) {
      if (tempHistory[i] < localMin) localMin = tempHistory[i];
      if (tempHistory[i] > localMax) localMax = tempHistory[i];
    }
  }

  // Ensure reasonable minimum graph range (at least 2.0°C range to avoid flat line noise)
  if ((localMax - localMin) < 2.0f) {
    float mid = (localMax + localMin) / 2.0f;
    localMin = mid - 1.0f;
    localMax = mid + 1.0f;
  }

  // Redraw Y-Axis Scale Values
  tft.fillRect(0, 24, 28, 92, COL_BG);
  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT_DIM, COL_BG);

  float dispMax = useFahrenheit ? (localMax * 1.8f + 32.0f) : localMax;
  float dispMin = useFahrenheit ? (localMin * 1.8f + 32.0f) : localMin;

  tft.setCursor(2, 26);
  tft.print((int)round(dispMax));
  tft.setCursor(2, 65);
  tft.print((int)round((dispMax + dispMin) / 2.0f));
  tft.setCursor(2, 106);
  tft.print((int)round(dispMin));

  // Clear Graph Area
  tft.fillRect(32, 26, 202, 88, COL_CARD_BG);

  // Redraw dotted grid lines
  for (int y = 47; y < 110; y += 22) {
    for (int x = 36; x < 230; x += 6) {
      tft.drawPixel(x, y, COL_GRID_LINE);
    }
  }

  // Plot data points
  if (points >= 2) {
    int graphW = 196;
    int graphH = 80;
    int graphX0 = 35;
    int graphY0 = 108; // Baseline

    float stepX = (float)graphW / (float)(GRAPH_HISTORY_LEN - 1);

    uint16_t plotColor = getTemperatureColor(currentTempC);

    int prevX = 0, prevY = 0;
    for (int i = 0; i < points; i++) {
      // Circular buffer ordered retrieval
      int bufIdx = (historyIndex - points + i + GRAPH_HISTORY_LEN) % GRAPH_HISTORY_LEN;
      float val = tempHistory[bufIdx];

      int px = graphX0 + (int)(i * stepX);
      float norm = (val - localMin) / (localMax - localMin);
      norm = constrain(norm, 0.0f, 1.0f);
      int py = graphY0 - (int)(norm * graphH);

      if (i > 0) {
        tft.drawLine(prevX, prevY, px, py, plotColor);
        tft.drawLine(prevX, prevY + 1, px, py + 1, plotColor); // 2px thickness
      }

      // Latest point gets a glowing dot
      if (i == points - 1) {
        tft.fillCircle(px, py, 3, COL_TEXT_WHITE);
      }

      prevX = px;
      prevY = py;
    }
  }

  // Calculate rate of change (°C/min) over recent samples
  float rate = 0.0f;
  if (points >= 10) {
    int oldestIdx = (historyIndex - points + GRAPH_HISTORY_LEN) % GRAPH_HISTORY_LEN;
    float deltaTemp = currentTempC - tempHistory[oldestIdx];
    float deltaMinutes = (points * (SENSOR_POLL_MS / 1000.0f)) / 60.0f;
    if (deltaMinutes > 0.0f) rate = deltaTemp / deltaMinutes;
  }

  // Update Footer Rate
  tft.fillRect(150, 120, 88, 14, COL_BG);
  tft.setTextSize(1);
  if (rate > 0.05f) {
    tft.setTextColor(COL_TEMP_WARM, COL_BG);
    tft.print("+");
  } else if (rate < -0.05f) {
    tft.setTextColor(COL_TEMP_COLD, COL_BG);
  } else {
    tft.setTextColor(COL_TEMP_GOOD, COL_BG);
  }
  tft.setCursor(160, 122);
  tft.print(rate, 2);
  tft.print(useFahrenheit ? " F/m" : " C/m");
}

// ------------------------------------------------------------------------------
// MODE 2: DIAGNOSTICS & METEOROLOGY STATS
// ------------------------------------------------------------------------------
void renderDiagnosticsStatic() {
  tft.fillScreen(COL_BG);
  drawTopStatusBar("METRICS & STATS");

  // Grid of 6 diagnostic sub-cards
  int colW = 114;
  int rowH = 28;

  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 2; c++) {
      int x = 4 + (c * 118);
      int y = 24 + (r * 31);
      tft.fillRoundRect(x, y, colW, rowH, 3, COL_CARD_BG);
      tft.drawRoundRect(x, y, colW, rowH, 3, COL_CARD_BORDER);
    }
  }

  // Static Labels
  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT_DIM, COL_CARD_BG);

  tft.setCursor(8, 28);  tft.print("DEW POINT");
  tft.setCursor(126, 28); tft.print("FEELS LIKE");

  tft.setCursor(8, 59);  tft.print("PRESSURE (inHg)");
  tft.setCursor(126, 59); tft.print("ALTITUDE (EST)");

  tft.setCursor(8, 90);  tft.print("I2C ADDRESS");
  tft.setCursor(126, 90); tft.print("PROBE UPTIME");

  tft.drawFastHLine(0, 118, SCREEN_W, COL_CARD_BORDER);
}

void renderDiagnosticsDynamic() {
  drawTopStatusBar("METRICS & STATS");

  tft.setTextSize(1);

  // 1. Dew Point
  tft.fillRect(8, 38, 100, 11, COL_CARD_BG);
  tft.setTextColor(COL_ACCENT, COL_CARD_BG);
  tft.setCursor(8, 38);
  float dispDew = useFahrenheit ? (currentDewPt * 1.8f + 32.0f) : currentDewPt;
  tft.print(dispDew, 2);
  tft.print(useFahrenheit ? " deg F" : " deg C");

  // 2. Feels Like (Heat Index)
  tft.fillRect(126, 38, 100, 11, COL_CARD_BG);
  tft.setTextColor(getTemperatureColor(currentHeatIdx), COL_CARD_BG);
  tft.setCursor(126, 38);
  float dispHi = useFahrenheit ? (currentHeatIdx * 1.8f + 32.0f) : currentHeatIdx;
  tft.print(dispHi, 2);
  tft.print(useFahrenheit ? " deg F" : " deg C");

  // 3. Pressure inHg
  tft.fillRect(8, 69, 100, 11, COL_CARD_BG);
  tft.setTextColor(COL_TEXT_WHITE, COL_CARD_BG);
  tft.setCursor(8, 69);
  tft.print(currentInHg, 2);
  tft.print(" inHg");

  // 4. Altitude (meters + feet)
  tft.fillRect(126, 69, 100, 11, COL_CARD_BG);
  tft.setTextColor(COL_TEXT_WHITE, COL_CARD_BG);
  tft.setCursor(126, 69);
  tft.print((int)currentAlt);
  tft.print("m (");
  tft.print((int)(currentAlt * 3.28084f));
  tft.print("ft)");

  // 5. I2C Bus Status
  tft.fillRect(8, 100, 100, 11, COL_CARD_BG);
  tft.setTextColor(COL_TEMP_GOOD, COL_CARD_BG);
  tft.setCursor(8, 100);
  tft.print("0x");
  tft.print(bmeAddress, HEX);
  tft.print(" (OK 400k)");

  // 6. Uptime (HH:MM:SS)
  unsigned long sec = millis() / 1000;
  int hh = sec / 3600;
  int mm = (sec % 3600) / 60;
  int ss = sec % 60;
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", hh, mm, ss);
  tft.fillRect(126, 100, 100, 11, COL_CARD_BG);
  tft.setTextColor(COL_TEXT_WHITE, COL_CARD_BG);
  tft.setCursor(126, 100);
  tft.print(timeBuf);

  // Bottom Footer
  tft.fillRect(4, 121, 232, 13, COL_BG);
  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.setCursor(4, 122);
  tft.print("Total Samples: ");
  tft.setTextColor(COL_TEXT_WHITE, COL_BG);
  tft.print(totalSamples);
  tft.setCursor(140, 122);
  tft.setTextColor(COL_ACCENT, COL_BG);
  tft.print("[Hold BOOT: C/F]");
}

// ------------------------------------------------------------------------------
// MODE 3: HIGH CONTRAST BIG DIGITS (FIELD PROBE)
// ------------------------------------------------------------------------------
void renderBigDigitsStatic() {
  tft.fillScreen(COL_BG);
  drawTopStatusBar("PROBE READOUT");
  tft.drawFastHLine(0, 100, SCREEN_W, COL_CARD_BORDER);
}

void renderBigDigitsDynamic() {
  drawTopStatusBar("PROBE READOUT");

  uint16_t tempCol = getTemperatureColor(currentTempC);
  float dispTemp = useFahrenheit ? currentTempF : currentTempC;

  // Giant Temperature Readout (Font Size 5)
  tft.fillRect(8, 30, 224, 48, COL_BG);
  tft.setTextColor(tempCol, COL_BG);
  tft.setTextSize(5);
  tft.setCursor(12, 34);
  tft.print(dispTemp, 1);

  // Big Unit
  tft.setTextSize(3);
  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.print(useFahrenheit ? "F" : "C");

  // Dynamic Horizontal Color Bar
  tft.fillRoundRect(8, 86, 224, 6, 3, tempCol);

  // Bottom Dual Metrics: Humidity & Pressure
  tft.fillRect(4, 106, 232, 24, COL_BG);
  tft.setTextSize(2);
  tft.setTextColor(COL_HUMID_BLUE, COL_BG);
  tft.setCursor(8, 110);
  tft.print(currentHum, 0);
  tft.print("% RH");

  tft.setTextColor(COL_PRESS_LILAC, COL_BG);
  tft.setCursor(130, 110);
  tft.print((int)currentPress);
  tft.setTextSize(1);
  tft.print(" hPa");
}

// ------------------------------------------------------------------------------
// SENSOR ERROR / DISCONNECTED MODAL SCREEN
// ------------------------------------------------------------------------------
void renderSensorErrorScreen() {
  tft.fillScreen(COL_BG);

  // Alert Box
  tft.fillRoundRect(12, 12, 216, 111, 6, COL_CARD_BG);
  tft.drawRoundRect(12, 12, 216, 111, 6, COL_ALERT_RED);
  tft.drawRoundRect(14, 14, 212, 107, 4, COL_ALERT_RED);

  tft.setTextSize(2);
  tft.setTextColor(COL_ALERT_RED, COL_CARD_BG);
  tft.setCursor(34, 24);
  tft.print("! PROBE ERROR !");

  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT_WHITE, COL_CARD_BG);
  tft.setCursor(24, 52);
  tft.print("BME280 sensor not detected.");

  tft.setTextColor(COL_TEXT_DIM, COL_CARD_BG);
  tft.setCursor(24, 70);
  tft.print("Check Wiring:");
  tft.setCursor(24, 82);
  tft.print("  SDA -> GPIO 21  |  SCL -> GPIO 22");
  tft.setCursor(24, 94);
  tft.print("  VCC -> 3.3V     |  GND -> GND");

  tft.setTextColor(COL_ACCENT, COL_CARD_BG);
  tft.setCursor(55, 108);
  tft.print("Auto-retrying bus...");
}

// ==============================================================================
// 7. INPUT HANDLING (BUTTON CYCLING & UNIT TOGGLING)
// ==============================================================================
void handleButtonInputs() {
  // Primary Button (GPIO 0: BOOT button or TTGO Left button)
  bool btnState = (digitalRead(BTN_PRIMARY) == LOW);

  if (btnState && !btnWasPressed) {
    btnPressStartTime = millis();
    btnWasPressed = true;
  } else if (!btnState && btnWasPressed) {
    unsigned long duration = millis() - btnPressStartTime;
    btnWasPressed = false;

    if (duration >= 1000) {
      // Long press: Toggle °C <-> °F
      useFahrenheit = !useFahrenheit;
      Serial.println(useFahrenheit ? "[INPUT] Unit switched to FAHRENHEIT" : "[INPUT] Unit switched to CELSIUS");
      modeNeedsFullRedraw = true;
    } else if (duration >= 50) {
      // Short press: Cycle screen modes
      currentMode = (DisplayMode)((currentMode + 1) % MODE_COUNT);
      modeNeedsFullRedraw = true;
      Serial.print("[INPUT] Display Mode: ");
      Serial.println((int)currentMode);
    }
  }

  // Secondary Button (GPIO 35: TTGO Right button or optional external button)
  #if defined(BTN_SECONDARY)
  bool btn2State = (digitalRead(BTN_SECONDARY) == LOW);
  if (btn2State && !btn2WasPressed) {
    btn2PressStartTime = millis();
    btn2WasPressed = true;
  } else if (!btn2State && btn2WasPressed) {
    unsigned long duration = millis() - btn2PressStartTime;
    btn2WasPressed = false;
    if (duration >= 50) {
      // Reset Min / Max values
      minTempC = currentTempC;
      maxTempC = currentTempC;
      minHum   = currentHum;
      maxHum   = currentHum;
      Serial.println("[INPUT] Min/Max records reset to current reading.");
      modeNeedsFullRedraw = true;
    }
  }
  #endif
}

// ==============================================================================
// 8. SENSOR INITIALIZATION & SAFE READING
// ==============================================================================
bool connectBME280() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000); // 400kHz Fast I2C Mode

  // Try 0x76 (standard for GY-BME280 modules)
  if (bme.begin(0x76, &Wire)) {
    bmeAddress = 0x76;
    Serial.println("[BME280] Successfully connected at address 0x76");
    return true;
  }

  // Try 0x77 (standard for Adafruit/SparkFun modules)
  if (bme.begin(0x77, &Wire)) {
    bmeAddress = 0x77;
    Serial.println("[BME280] Successfully connected at address 0x77");
    return true;
  }

  Serial.println("[BME280 ERROR] Could not find BME280 sensor at 0x76 or 0x77!");
  return false;
}

void configureBME280Settings() {
  // Indoor temperature probe recommended sampling profile:
  // Forced mode or Normal mode, 16x oversampling for rock-solid stability
  bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                  Adafruit_BME280::SAMPLING_X16,  // Temperature oversampling
                  Adafruit_BME280::SAMPLING_X16,  // Pressure oversampling
                  Adafruit_BME280::SAMPLING_X16,  // Humidity oversampling
                  Adafruit_BME280::FILTER_X16,    // IIR Filter coefficient
                  Adafruit_BME280::STANDBY_MS_500);
}

bool readSensorData() {
  float t = bme.readTemperature();
  float h = bme.readHumidity();
  float p = bme.readPressure();

  if (isnan(t) || isnan(h) || isnan(p) || p <= 0.0f) {
    return false;
  }

  currentTempC = t + TEMP_CAL_OFFSET_C;
  currentTempF = (currentTempC * 1.8f) + 32.0f;
  currentHum   = h;
  currentPress = p / 100.0F; // hPa
  currentInHg  = currentPress * 0.02953F;
  currentAlt   = bme.readAltitude(SEALEVELPRESSURE_HPA);
  currentDewPt = calculateDewPoint(currentTempC, currentHum);
  currentHeatIdx = calculateHeatIndex(currentTempC, currentHum);

  // Update Min / Max memory
  if (currentTempC < minTempC) minTempC = currentTempC;
  if (currentTempC > maxTempC) maxTempC = currentTempC;
  if (currentHum < minHum) minHum = currentHum;
  if (currentHum > maxHum) maxHum = currentHum;

  // Record into rolling history buffer
  tempHistory[historyIndex] = currentTempC;
  historyIndex = (historyIndex + 1) % GRAPH_HISTORY_LEN;
  if (historyCount < GRAPH_HISTORY_LEN) historyCount++;

  totalSamples++;
  return true;
}

// ==============================================================================
// 9. SETUP & MAIN LOOP
// ==============================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println(F("\n========================================================"));
  Serial.println(F(" ESP32 + BME280 + 1.14\" IPS (135x240) TEMP PROBE SENSOR "));
  Serial.println(F("========================================================"));

  // Button setup
  pinMode(BTN_PRIMARY, INPUT_PULLUP);
  #if defined(BTN_SECONDARY)
  pinMode(BTN_SECONDARY, INPUT_PULLUP);
  #endif

  // Backlight setup
  #if (TFT_BL >= 0)
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH); // Turn on display backlight
  #endif

  // Initialize ESP32 Hardware SPI bus with board-specific pins
  #if (defined(TFT_SCLK) && defined(TFT_MOSI) && defined(TFT_CS))
    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  #endif

  tft.init(SCREEN_H, SCREEN_W); // 135, 240
  tft.setRotation(DISPLAY_ROTATION);
  tft.invertDisplay(INVERT_COLORS);
  tft.fillScreen(COL_BG);

  // Welcome Splash Screen
  tft.fillRoundRect(20, 20, 200, 95, 6, COL_CARD_BG);
  tft.drawRoundRect(20, 20, 200, 95, 6, COL_ACCENT);

  tft.setTextSize(2);
  tft.setTextColor(COL_TEXT_WHITE, COL_CARD_BG);
  tft.setCursor(45, 34);
  tft.print("TEMP PROBE");

  tft.setTextSize(1);
  tft.setTextColor(COL_ACCENT, COL_CARD_BG);
  tft.setCursor(52, 60);
  tft.print("ESP32 + BME280 + IPS");

  tft.setTextColor(COL_TEXT_DIM, COL_CARD_BG);
  tft.setCursor(40, 80);
  tft.print("Initializing Sensor...");

  // Initialize BME280
  sensorHealthy = connectBME280();
  if (sensorHealthy) {
    configureBME280Settings();
    readSensorData();
    tft.setTextColor(COL_TEMP_GOOD, COL_CARD_BG);
    tft.setCursor(68, 96);
    tft.print("Sensor Ready!");
  } else {
    tft.setTextColor(COL_ALERT_RED, COL_CARD_BG);
    tft.setCursor(55, 96);
    tft.print("Sensor Not Found!");
  }

  delay(1200);
  modeNeedsFullRedraw = true;
}

void loop() {
  handleButtonInputs();

  unsigned long currentMillis = millis();

  // Periodic sensor read & UI update
  if (currentMillis - lastSensorReadTime >= SENSOR_POLL_MS) {
    lastSensorReadTime = currentMillis;
    heartbeatState = !heartbeatState;

    if (!sensorHealthy) {
      sensorHealthy = connectBME280();
      if (sensorHealthy) {
        configureBME280Settings();
        modeNeedsFullRedraw = true;
      } else {
        renderSensorErrorScreen();
        return;
      }
    }

    // Read sensor
    if (readSensorData()) {
      // Print high-precision telemetry stream over Serial
      Serial.printf("[PROBE] #%lu | Temp: %.2f C (%.2f F) | Hum: %.2f %% | Press: %.2f hPa | Dew: %.2f C\n",
                    totalSamples, currentTempC, currentTempF, currentHum, currentPress, currentDewPt);

      // Render Display
      if (modeNeedsFullRedraw) {
        switch (currentMode) {
          case MODE_DASHBOARD:    renderDashboardStatic(); break;
          case MODE_TREND_GRAPH:  renderTrendGraphStatic(); break;
          case MODE_DIAGNOSTICS:  renderDiagnosticsStatic(); break;
          case MODE_BIG_DIGITS:   renderBigDigitsStatic(); break;
          default: break;
        }
        modeNeedsFullRedraw = false;
      }

      switch (currentMode) {
        case MODE_DASHBOARD:    renderDashboardDynamic(); break;
        case MODE_TREND_GRAPH:  renderTrendGraphDynamic(); break;
        case MODE_DIAGNOSTICS:  renderDiagnosticsDynamic(); break;
        case MODE_BIG_DIGITS:   renderBigDigitsDynamic(); break;
        default: break;
      }
    } else {
      // Reading failed mid-operation
      sensorHealthy = false;
      renderSensorErrorScreen();
    }
  }
}
