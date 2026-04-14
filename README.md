# Altoxymeter

SpO2, heart rate, and altitude monitor for M5Stack CoreS3.

![Version](https://img.shields.io/badge/version-v2.2-blue)
![Hardware](https://img.shields.io/badge/hardware-M5Stack%20CoreS3-green)
![Platform](https://img.shields.io/badge/platform-Arduino%2FESP32-orange)

## Features

- **SpO2 Monitoring** - Blood oxygen saturation via MAX30100 sensor
- **Heart Rate** - Real-time pulse detection
- **Altitude** - Calculated from barometric pressure (BME688)
- **Data Logging** - CSV export to microSD card
- **RTC Sync** - Automatic time synchronization via NTP

## Hardware Setup

### Required Components

| Component | Port | SDA | SCL |
|-----------|------|-----|-----|
| M5Stack CoreS3 | - | - | - |
| MAX30100 | Port B (black) | GPIO 9 | GPIO 8 |
| BME688 | Port A (red) | GPIO 2 | GPIO 1 |
| microSD card | Built-in | - | - |

### Wiring

```
Port A (red)  → BME688 sensor (SDA=2, SCL=1)
Port B (black) → MAX30100 sensor (SDA=9, SCL=8)
```

## Configuration

### 1. WiFi Credentials

Before uploading, edit `Altoxymeter.ino` and set your WiFi credentials:

```cpp
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASS     = "YOUR_WIFI_PASSWORD";
```

> **Note**: These credentials are used only for NTP time synchronization. The device does not transmit any data over WiFi during normal operation.

### 2. Timezone

Modify the timezone string if needed (default is US Eastern):

```cpp
const char* NY_TZ = "EST5EDT,M3.2.0/2,M11.1.0/2";
```

For other timezones, see the [ESP32 timezone database](https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h).

### 3. Logging Interval

Change how often data is saved to CSV:

```cpp
const unsigned long LOG_INTERVAL_MS = 5000;  // 5 seconds
```

## Installation

### Arduino IDE

1. Install required libraries:
   - **M5Unified** - Core S3 support
   - **M5GFX** - Display graphics
   - **Adafruit_BME680** - Pressure sensor
   - **MAX30100lib** - Pulse oximeter (or use included drivers)

2. Select board: **ESP32-S3 Dev Module** or **M5Stack CoreS3**

3. Configure partition scheme: **Minimal SPIFFS** or use `partitions.csv`

4. Upload the sketch

### PlatformIO

```bash
pio run --upload
```

## Output

### Display

The LCD shows:
- SpO2 percentage (large, cyan)
- Heart rate in BPM (red)
- Altitude in feet (large, green)
- Current date/time
- Battery level
- Sensor status

### Serial Monitor

Debug output at 115200 baud:
```
[DATA] SpO2=98 HR=72 Alt=125.4ft Pres=1013.2hPa
```

### CSV File

Data is saved to `/readings.csv` on the microSD card:

```csv
date,time,spo2_pct,altitude_ft
2026-04-14,10:30:00,98,125.3
2026-04-14,10:30:05,97,125.4
```

## Troubleshooting

### MAX30100 Not Detected
- Ensure the sensor is connected to **Port B** (black)
- Try re-seating the cable
- Check that Wire2 is initialized correctly

### BME688 Not Detected
- Ensure the sensor is connected to **Port A** (red)
- Try the alternative I2C address (0x76 instead of 0x77)

### SD Card Failed
- Format the SD card as FAT32
- Ensure the card is properly inserted

### RTC Not Syncing
- Verify WiFi credentials are correct
- Check that the NTP pool is accessible
- The device will still function without NTP sync (time will show year < 2025)

## License

MIT License - See LICENSE file for details.
