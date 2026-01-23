# AirQ – Air Quality Monitor

A compact, ESP8266-based indoor air-quality monitoring system designed to measure environmental conditions, infer air quality, and expose data through local (LED, serial) and cloud-ready interfaces.

## Overview

AirQ is not a consumer gadget—it's an **engineering platform** for controlled air-quality sensing and signal interpretation.

### Key Features

✅ **Hardware-constrained**: ESP8266 (80 MHz, ~80 KB RAM)
✅ **Offline-first**: Useful without backend or dashboard
✅ **Progressively extensible**: Cloud ingestion and visualization ready
✅ **Deterministic sampling**: 1 Hz cadence, stable sensor fusion
✅ **Hysteresis-stabilized LED feedback**: Prevents flickering near color thresholds

## Hardware Architecture

### Core MCU
- **WeMos D1 Mini** (ESP8266)
  - 80 MHz
  - ~80 KB usable RAM
  - Integrated Wi-Fi
  - USB-serial for power, flashing, and debugging

### Sensors

#### Sensirion SGP30 (VOC/eCO₂)
- Measures VOC gas resistance
- Outputs:
  - **TVOC** (ppb) – Total Volatile Organic Compounds
  - **eCO₂** (ppm) – Inferred equivalent CO₂
- Proprietary internal IAQ algorithm
- Requires ~1 Hz sampling cadence
- Benefits from humidity compensation

#### Sensirion SHT31 (Temperature & Humidity)
- **Temperature** (°C)
- **Relative Humidity** (%)
- Used as primary output and SGP30 compensation input

### Output / UX

#### Single NeoPixel RGB LED
- **Real-time air-quality indicator**
  - Green: Clean air (AQ Index 0–20)
  - Yellow: Moderate (20–60)
  - Red: Poor (60–100)
- **Warm-up state**: Pulsing blue (first 60 seconds)
- **Hysteresis protection**: LED stays stable near color transitions

## Firmware Architecture (ESP8266)

### Design Principles

- **Deterministic timing**: 1 Hz sampling loop
- **Defensive initialization**: Sensor presence flags prevent crashes
- **Minimal heap usage**: No dynamic JSON libraries
- **Explicit separation**: Sensing → Conditioning → Presentation → Transport

### Signal Flow

#### Boot
1. Initialize serial, I²C, LED
2. Probe sensors (SHT31, SGP30)
3. Start SGP30 IAQ algorithm
4. Connect to Wi-Fi (best-effort, non-blocking)

#### Warm-up Phase (60 seconds)
- Fixed warm-up window (`WARMUP_MS` in config.h)
- LED shows pulsing blue (ignores AQ)
- Data is still collected and logged

#### Sampling Loop (~1 Hz)
1. Read temperature and humidity (if SHT31 present)
2. Compute absolute humidity
3. Apply humidity compensation to SGP30
4. Trigger SGP30 IAQ step
5. Read TVOC, eCO₂, compute AQ Index
6. Apply hysteresis to stabilize LED transitions
7. Update LED color
8. Emit JSON via serial
9. POST JSON to cloud (if connected)

### AQ Index Calculation

**Heuristic mapping from TVOC (ppb) → AQ Index (0–100):**

| TVOC Range | AQ Index Range | Mapping |
|------------|----------------|---------|
| 0–200 ppb  | 0–60           | Linear  |
| 200–800 ppb| 60–90          | Linear  |
| >800 ppb   | 100            | Clamp   |

**Hysteresis**: Configurable via `AQ_HYSTERESIS_BAND` (default 5 ppb) prevents flickering.

## Firmware Outputs

Per-sample JSON emitted over serial and posted to cloud:

```json
{
  "ts_ms": 60301,           // Time since boot (ms)
  "device_id": "airq-d1mini-01",
  "t_c": 29.21,             // Temperature (°C)
  "rh": 41.56,              // Relative Humidity (%)
  "tvoc_ppb": 48,           // Total VOC (ppb)
  "eco2_ppm": 514,          // Inferred CO₂ (ppm)
  "aq_index": 2,            // Air Quality Index (0–100)
  "warming_up": false       // Warm-up phase flag
}
```

## Web / Dashboard Architecture

### Live Dashboard
- **Real-time MQTT connection via WebSocket**
- Connects to HiveMQ broker, subscribes to device topic
- Auto-updates charts every 5 seconds
- Visualizes last 120 samples (~4 minutes at 2s interval)
- Fallback: Manual JSON paste for offline data

### Plots
1. **Temperature + Humidity** (dual Y-axis)
2. **TVOC + eCO₂** (dual Y-axis)
3. **Air Quality Index** with threshold bands (green/yellow/red)

## Cloud Architecture

### Current Status
- **HiveMQ MQTT Broker**: Real-time pub/sub messaging
- **Cloudflare Pages**: Static dashboard hosting
- **GitHub Actions**: Automated deployment on push

## Configuration

### Firmware (config.h)

```cpp
// Wi-Fi credentials
static const char* WIFI_SSID = "YOUR_SSID";
static const char* WIFI_PASSWORD = "YOUR_PASSWORD";

// HiveMQ MQTT Broker
static const char* MQTT_BROKER = "your-cluster.hivemq.cloud";
static const uint16_t MQTT_PORT = 8883;
static const char* MQTT_USERNAME = "your-username";
static const char* MQTT_PASSWORD = "your-password";
static const char* MQTT_TOPIC = "airq/your-device-id";

// Sampling
static const uint32_t SAMPLE_MS = 2000;

// LED
static const uint8_t LED_BRIGHTNESS = 40;

// Warm-up
static const uint32_t WARMUP_MS = 60000;

// AQ Index thresholds & hysteresis
static const uint8_t AQ_THRESHOLD_LOW = 20;
static const uint8_t AQ_THRESHOLD_HIGH = 60;
static const uint8_t AQ_HYSTERESIS_BAND = 5;
```

See `firmware/include/config.example.h` for a template.

## Building & Flashing

### Prerequisites
- [PlatformIO](https://platformio.org/) (IDE or CLI)
- [Arduino IDE](https://www.arduino.cc/) (alternative)

### Build & Upload
```bash
cd firmware
pio run -t upload -e d1_mini
```

Or use the PlatformIO IDE sidebar.

### Monitor Serial Output
```bash
pio device monitor --baud 115200
```

## Deployment

See [CLOUDFLARE_SETUP.md](CLOUDFLARE_SETUP.md) for:
- Cloudflare Pages setup
- GitHub Actions automation
- Custom domain configuration
- D1 database integration (future)

## Project Structure

```
firmware/
  ├── platformio.ini         # PlatformIO config
  ├── include/
  │   ├── config.h           # Secrets & per-device settings (NOT committed)
  │   └── config.example.h   # Template for config.h
  └── src/
      └── main.cpp           # ESP8266 firmware

web/
  ├── package.json           # Node.js dependencies
  ├── wrangler.toml          # Cloudflare Workers config
  ├── functions/
  │   └── api/
  │       ├── ingest.js      # POST endpoint for sensor data
  │       ├── latest.js      # GET latest readings (stub)
  │       └── range.js       # GET time-range data (stub)
  ├── public/
  │   └── index.html         # Static dashboard
  └── schema.sql             # D1 database schema (future)

.github/
  └── workflows/
      └── deploy.yml         # GitHub Actions: deploy to Cloudflare
```

## Known Limitations

⚠️ **AQ Index is conservative**: Based on TVOC only; may underestimate poor air quality
⚠️ **No real CO₂ measurement**: eCO₂ is inferred; consider NDIR sensor for accuracy
⚠️ **TLS certificate validation disabled**: Uses `setInsecure()` for MVP

## TODO

- Evaluate discrete (banded) vs continuous (gradient) LED color signaling
- Add EEPROM-based WiFi credential storage
- Support NDIR CO₂ sensor module
- Add data persistence (D1 database integration)
- Implement certificate validation for production TLS

## References

- [Sensirion SGP30 Datasheet](https://sensirion.com/products/catalog/SGP30/)
- [Sensirion SHT31 Datasheet](https://sensirion.com/products/catalog/SHT31/)
- [ESP8266 Documentation](https://arduino-esp8266.readthedocs.io/)
- [Adafruit Libraries](https://github.com/adafruit)
- [HiveMQ Cloud](https://www.hivemq.com/mqtt-cloud-broker/)

## License

(Add your license here)

## Contact

Project author: Javier Alejandro Garcia  
GitHub: [Garcia-Javier-Alejandro/AirQ](https://github.com/Garcia-Javier-Alejandro/AirQ)
