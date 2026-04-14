/*
 * Altoxymeter v2.2 — M5Stack CoreS3 + MAX30100 + BME688
 *
 * Port A (red):   BME688   SDA=2, SCL=1 → Wire1 (peripheral 1)
 * Port B (black):  MAX30100 SDA=9, SCL=8 → Wire2 (peripheral 0)
 * CSV to microSD card. RTC synced via NTP.
 */

#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <SD.h>
#include <SPI.h>
#include <M5Unified.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include "MAX30100_PulseOximeter.h"

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
const char* WIFI_SSID     = "47Harriet";
const char* WIFI_PASS     = "eatmyshorts";
const float SEA_LEVEL_HPA = 1013.25;
const unsigned long LOG_INTERVAL_MS = 5000;
const char* NY_TZ = "EST5EDT,M3.2.0/2,M11.1.0/2";
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

#define SDA_PORT_A  2
#define SCL_PORT_A  1
#define SDA_PORT_B  9
#define SCL_PORT_B  8
#define CSV_PATH "/readings.csv"

extern TwoWire Wire1;
TwoWire Wire2(0);

Adafruit_BME680 bme(&Wire1);
PulseOximeter pox;

float gPressure = 0;
float gAlt      = 0;

bool  poxOk  = false;
bool  bmeOk  = false;
bool  sdOk   = false;
unsigned long lastLog     = 0;
unsigned long logCount    = 0;
unsigned long lastDisplay = 0;
unsigned long lastBME     = 0;
unsigned long lastDebug   = 0;

void onBeatDetected() {
    Serial.println("[BEAT]");
}

// ══════════════════════════════════════════════════════════════
void setup()
{
    auto cfg = M5.config();
    cfg.output_power = true;
    M5.begin(cfg);

    Serial.begin(115200);
    unsigned long sw = millis();
    while (!Serial && (millis() - sw < 2000)) delay(10);
    delay(200);

    Serial.println("\n==========================================");
    Serial.println("  Altoxymeter v2.2 (CoreS3)");
    Serial.println("==========================================\n");

    // Force bus power
    {
        uint8_t buf[2] = {0, 0};
        M5.In_I2C.readRegister(0x58, 0x02, buf, 2, 400000);
        buf[0] |= 0x02;
        buf[1] |= 0x80;
        M5.In_I2C.writeRegister(0x58, 0x02, buf, 2, 400000);
    }
    delay(300);

    // Display — direct, no canvas
    M5.Lcd.setRotation(1);
    M5.Lcd.setBrightness(80);
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.println("Altoxymeter v2.2");
    M5.Lcd.setTextSize(1);

    // ── microSD ──
    M5.Lcd.setCursor(10, 40);
    M5.Lcd.print("SD: ");
    sdOk = SD.begin(GPIO_NUM_4, SPI, 25000000);
    M5.Lcd.println(sdOk ? "OK" : "FAIL");
    Serial.printf("[SD] %s\n", sdOk ? "OK" : "FAIL");
    if (sdOk && !SD.exists(CSV_PATH)) {
        File f = SD.open(CSV_PATH, FILE_WRITE);
        if (f) { f.println("date,time,spo2_pct,altitude_ft"); f.close(); }
    }

    // ── Wire1 → Port A (BME688) ──
    Wire1.begin(SDA_PORT_A, SCL_PORT_A, 100000UL);
    delay(100);

    // ── Wire2 → Port B (MAX30100) ──
    Wire2.begin(SDA_PORT_B, SCL_PORT_B, 400000UL);
    delay(100);

    // ── MAX30100 ──
    M5.Lcd.setCursor(10, 55);
    M5.Lcd.print("MAX30100: ");
    Serial.print("[MAX30100] ");
    if (pox.begin(Wire2)) {
        pox.setOnBeatDetectedCallback(onBeatDetected);
        poxOk = true;
        M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Lcd.println("OK");
        Serial.println("OK");
    } else {
        poxOk = false;
        M5.Lcd.setTextColor(TFT_RED, TFT_BLACK);
        M5.Lcd.println("FAIL");
        Serial.println("FAIL");
    }
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

    // ── BME688 ──
    M5.Lcd.setCursor(10, 70);
    M5.Lcd.print("BME688: ");
    Serial.print("[BME688] ");
    bmeOk = bme.begin(0x77);
    if (!bmeOk) bmeOk = bme.begin(0x76);
    if (bmeOk) {
        bme.setTemperatureOversampling(BME680_OS_8X);
        bme.setHumidityOversampling(BME680_OS_2X);
        bme.setPressureOversampling(BME680_OS_4X);
        bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
        bme.setGasHeater(320, 150);
        M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Lcd.println("OK");
        Serial.println("OK");
    } else {
        M5.Lcd.setTextColor(TFT_RED, TFT_BLACK);
        M5.Lcd.println("FAIL");
        Serial.println("FAIL");
    }
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

    // ── WiFi + NTP ──
    M5.Lcd.setCursor(10, 85);
    M5.Lcd.print("NTP: ");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int att = 0;
    while (WiFi.status() != WL_CONNECTED && att < 30) { delay(500); att++; }
    if (WiFi.status() == WL_CONNECTED) {
        configTzTime(NY_TZ, "pool.ntp.org", "time.nist.gov");
        struct tm ti;
        int tries = 0;
        while (!getLocalTime(&ti, 500) && tries < 20) tries++;
        if (getLocalTime(&ti, 0)) {
            m5::rtc_datetime_t rtcDt;
            rtcDt.date.year  = ti.tm_year + 1900;
            rtcDt.date.month = ti.tm_mon + 1;
            rtcDt.date.date  = ti.tm_mday;
            rtcDt.time.hours   = ti.tm_hour;
            rtcDt.time.minutes = ti.tm_min;
            rtcDt.time.seconds = ti.tm_sec;
            M5.Rtc.setDateTime(rtcDt);
            M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
            M5.Lcd.printf("%04d-%02d-%02d %02d:%02d:%02d",
                          rtcDt.date.year, rtcDt.date.month, rtcDt.date.date,
                          rtcDt.time.hours, rtcDt.time.minutes, rtcDt.time.seconds);
            Serial.printf("[NTP] OK\n");
        }
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    } else {
        M5.Lcd.setTextColor(TFT_RED, TFT_BLACK);
        M5.Lcd.println("FAIL");
        Serial.println("[WiFi] FAIL");
    }

    logCount = sdOk ? countCSVRows() : 0;

    Serial.println("\n========== STATUS ==========");
    Serial.printf("MAX30100: %s\n", poxOk ? "OK" : "FAIL");
    Serial.printf("BME688:   %s\n", bmeOk ? "OK" : "FAIL");
    Serial.printf("SD:       %s\n", sdOk ? "OK" : "FAIL");
    Serial.printf("CSV rows: %lu\n", logCount);
    Serial.println("============================\n");
    Serial.println("Waiting 3s then starting main loop...");

    delay(3000);
    M5.Lcd.fillScreen(TFT_BLACK);
}

// ══════════════════════════════════════════════════════════════
unsigned long countCSVRows() {
    File f = SD.open(CSV_PATH, FILE_READ);
    if (!f) return 0;
    unsigned long n = 0;
    while (f.available()) { if (f.read() == '\n') n++; }
    f.close();
    return (n > 0) ? n - 1 : 0;
}

bool getRTCTime(char* d, size_t dl, char* t, size_t tl) {
    auto dt = M5.Rtc.getDateTime();
    if (dt.date.year < 2025) return false;
    snprintf(d, dl, "%04d-%02d-%02d", dt.date.year, dt.date.month, dt.date.date);
    snprintf(t, tl, "%02d:%02d:%02d", dt.time.hours, dt.time.minutes, dt.time.seconds);
    return true;
}

void logToCSV() {
    if (!sdOk) return;
    char d[16], t[16];
    if (!getRTCTime(d, sizeof(d), t, sizeof(t))) return;

    uint8_t spo2 = poxOk ? pox.getSpO2() : 0;
    float altFt = gAlt * 3.28084;

    File f = SD.open(CSV_PATH, FILE_APPEND);
    if (!f) return;
    if (spo2 > 0)
        f.printf("%s,%s,%d,%.1f\n", d, t, spo2, altFt);
    else
        f.printf("%s,%s,,%.1f\n", d, t, altFt);
    f.close();
    logCount++;
}

void readBME688() {
    if (!bmeOk) return;
    if (!bme.performReading()) return;
    gPressure = bme.pressure / 100.0;
    gAlt = 44330.0 * (1.0 - pow(gPressure / SEA_LEVEL_HPA, 0.1903));
}

// ══════════════════════════════════════════════════════════════
void updateDisplay() {
    uint8_t spo2 = poxOk ? pox.getSpO2() : 0;
    float hr = poxOk ? pox.getHeartRate() : 0;
    float altFt = gAlt * 3.28084;

    M5.Lcd.fillScreen(TFT_BLACK);

    // SpO2 — big
    M5.Lcd.setTextColor(0x67FF, TFT_BLACK);
    M5.Lcd.setCursor(20, 10);
    M5.Lcd.setTextSize(5);
    if (spo2 > 0)
        M5.Lcd.printf("%d%%", spo2);
    else
        M5.Lcd.print("--%");
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(200, 20);
    M5.Lcd.setTextColor(0x8410, TFT_BLACK);
    M5.Lcd.print("SpO2");

    // HR
    M5.Lcd.setCursor(200, 50);
    M5.Lcd.setTextSize(2);
    if (hr > 0) {
        M5.Lcd.setTextColor(TFT_RED, TFT_BLACK);
        M5.Lcd.printf("%d bpm", (int)hr);
    } else if (poxOk) {
        M5.Lcd.setTextColor(0xFD20, TFT_BLACK);
        M5.Lcd.setTextSize(1);
        M5.Lcd.print("Place finger");
    }

    // Altitude — big
    M5.Lcd.setTextColor(0x47E0, TFT_BLACK);
    M5.Lcd.setCursor(20, 100);
    M5.Lcd.setTextSize(5);
    if (bmeOk && gPressure > 0)
        M5.Lcd.printf("%.0f", altFt);
    else
        M5.Lcd.print("--");
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(200, 110);
    M5.Lcd.setTextColor(0x8410, TFT_BLACK);
    M5.Lcd.print("ft");

    // Bottom bar
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(0x8410, TFT_BLACK);
    char d[16], t[16];
    if (getRTCTime(d, sizeof(d), t, sizeof(t))) {
        M5.Lcd.setCursor(5, 200);
        M5.Lcd.printf("%s %s NY", d, t);
    }
    int batPct = M5.Power.getBatteryLevel();
    M5.Lcd.setCursor(5, 215);
    M5.Lcd.printf("CSV:%lu  BAT:%d%%  MAX:%s BME:%s SD:%s",
                   logCount, batPct,
                   poxOk ? "OK" : "--",
                   bmeOk ? "OK" : "--",
                   sdOk ? "OK" : "--");
}

// ══════════════════════════════════════════════════════════════
void loop() {
    M5.update();

    // MAX30100 must be updated as fast as possible
    if (poxOk) {
        pox.update();
    }

    // BME688 every 3 seconds
    if (millis() - lastBME > 3000) {
        lastBME = millis();
        readBME688();
    }

    // Display every 500ms (less frequent to give pox.update() more time)
    if (millis() - lastDisplay > 500) {
        lastDisplay = millis();
        updateDisplay();
    }

    // Debug to serial every 2 seconds
    if (millis() - lastDebug > 2000) {
        lastDebug = millis();
        uint8_t spo2 = poxOk ? pox.getSpO2() : 0;
        float hr = poxOk ? pox.getHeartRate() : 0;
        Serial.printf("[DATA] SpO2=%d HR=%.0f Alt=%.1fft Pres=%.1fhPa\n",
                      spo2, hr, gAlt * 3.28084, gPressure);
    }

    // CSV logging
    if (millis() - lastLog >= LOG_INTERVAL_MS) {
        lastLog = millis();
        logToCSV();
    }
}
