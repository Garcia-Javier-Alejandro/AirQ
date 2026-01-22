// TODO (post-review):
// - Add hysteresis to LED behavior to prevent flicker near AQ thresholds ✓ DONE
// - Expose AQ thresholds and hysteresis margins via config.h ✓ DONE
// - Evaluate discrete (banded) vs continuous (gradient) LED color signaling


#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>

#include <Adafruit_SGP30.h>
#include <Adafruit_SHT31.h>
#include <Adafruit_NeoPixel.h>

#include "config.h" // local secrets + per-device settings (NOT committed; See config.example.h for an example)

static bool shtOk = false;
static bool sgpOk = false;
static uint8_t lastAqIndex = 0;  // Track previous AQ index for hysteresis


// Hardware stack
Adafruit_NeoPixel leds(N_LEDS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
Adafruit_SGP30 sgp;
Adafruit_SHT31 sht31 = Adafruit_SHT31();

static uint32_t bootMs = 0;
static uint32_t lastSample = 0;

// Apply brightness cap and update first pixel (assumes at least 1 LED)
static void setLed(uint32_t c) {
  leds.setBrightness(LED_BRIGHTNESS);
  leds.setPixelColor(0, c);
  leds.show();
}

// From power-on until WARMUP_MS expires the LED ignores air quality and shows a slow pulsing blue
static uint32_t pulsingBlue(uint32_t nowMs) {
  float phase = (nowMs % 1000) / 1000.0f;
  float tri = (phase < 0.5f) ? (phase * 2.0f) : (2.0f - phase * 2.0f);
  uint8_t b = (uint8_t)(20 + 120 * tri);
  return leds.Color(0, 0, b);
}

// 0..100 index from TVOC ppb (tune later!)
static uint8_t tvocToIndex(uint16_t tvoc) {
  if (tvoc <= 200) {
    // 0..200 ppb → 0..60
    return (uint8_t)(tvoc * 60 / 200);
  }
  if (tvoc <= 800) {
    // 200..800 ppb → 60..90
    return (uint8_t)(60 + (tvoc - 200) * 30 / 600);
  }
  // >800 ppb → clamp
  return 100;
}

// Apply hysteresis to prevent LED flickering near threshold transitions
// Returns the stabilized AQ index: sticks with lastAqIndex unless
// the new value is far enough away (beyond the hysteresis band)
static uint8_t applyHysteresis(uint8_t newIndex) {
  // If we're in a stable zone far from thresholds, accept the change
  if (newIndex <= (AQ_THRESHOLD_LOW - AQ_HYSTERESIS_BAND) ||
      (newIndex > (AQ_THRESHOLD_LOW - AQ_HYSTERESIS_BAND) && 
       newIndex < (AQ_THRESHOLD_LOW + AQ_HYSTERESIS_BAND) &&
       lastAqIndex >= AQ_THRESHOLD_LOW) ||
      (newIndex > (AQ_THRESHOLD_HIGH - AQ_HYSTERESIS_BAND) && 
       newIndex < (AQ_THRESHOLD_HIGH + AQ_HYSTERESIS_BAND) &&
       lastAqIndex >= AQ_THRESHOLD_HIGH) ||
      newIndex >= (AQ_THRESHOLD_HIGH + AQ_HYSTERESIS_BAND)) {
    // Simplified: accept if clearly away from thresholds
    if (newIndex < (AQ_THRESHOLD_LOW - AQ_HYSTERESIS_BAND) ||
        newIndex > (AQ_THRESHOLD_HIGH + AQ_HYSTERESIS_BAND)) {
      return newIndex;
    }
    // In hysteresis zone: check relative to last state
    if (lastAqIndex < AQ_THRESHOLD_LOW && newIndex < (AQ_THRESHOLD_LOW + AQ_HYSTERESIS_BAND)) {
      return lastAqIndex;  // Stay in green
    }
    if (lastAqIndex >= AQ_THRESHOLD_LOW && lastAqIndex < AQ_THRESHOLD_HIGH &&
        newIndex >= (AQ_THRESHOLD_LOW - AQ_HYSTERESIS_BAND) && 
        newIndex < (AQ_THRESHOLD_HIGH + AQ_HYSTERESIS_BAND)) {
      return lastAqIndex;  // Stay in yellow
    }
    if (lastAqIndex >= AQ_THRESHOLD_HIGH && newIndex > (AQ_THRESHOLD_HIGH - AQ_HYSTERESIS_BAND)) {
      return lastAqIndex;  // Stay in red
    }
  }
  return newIndex;
}


//Index →
//0    LOW       HIGH          100
//|----|----------|------------|
// G      G→Y        Y→R
static uint32_t colorForIndex(uint8_t idx) {
  if (idx <= AQ_THRESHOLD_LOW) return leds.Color(0, 255, 0);
  if (idx <= AQ_THRESHOLD_HIGH) {
    float u = (idx - AQ_THRESHOLD_LOW) / (float)(AQ_THRESHOLD_HIGH - AQ_THRESHOLD_LOW);
    return leds.Color((uint8_t)(255 * u), 255, 0);
  }
  float u = (idx - AQ_THRESHOLD_HIGH) / (float)(100 - AQ_THRESHOLD_HIGH);
  return leds.Color(255, (uint8_t)(255 * (1.0f - u)), 0);
}


// absolute humidity (mg/m^3) for SGP30 compensation
static uint32_t absoluteHumidity_mgm3(float tempC, float rh) {
  // saturation vapor pressure (hPa)
  float svp = 6.112f * expf((17.62f * tempC) / (243.12f + tempC));
  // actual vapor pressure (hPa)
  float avp = svp * (rh / 100.0f);
  // absolute humidity (g/m^3)
  float ah = 2.1674f * avp * 100.0f / (273.15f + tempC);
  return (uint32_t)(ah * 1000.0f);
}

static void wifiConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 15000) {
    delay(250);
  }
}

// HTTPS POST to Cloudlfare ingest endpoint
static bool postIngest(const String& json) {
  if (WiFi.status() != WL_CONNECTED) return false;

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure(); // MVP; later: CA bundle

  HTTPClient https;
  if (!https.begin(*client, INGEST_URL)) return false;

  https.addHeader("Content-Type", "application/json");
  https.addHeader("Authorization", String("Bearer ") + INGEST_TOKEN);

  int code = https.POST((uint8_t*)json.c_str(), json.length());
  https.end();
  return (code >= 200 && code < 300);
}

void setup() {
  bootMs = millis();
  Serial.begin(115200);
  delay(50);

  Wire.begin(); // D1 mini default I2C pins

  leds.begin();
  leds.clear();
  leds.show();

  shtOk = sht31.begin(0x45); 
  //When there is a SHT sensor, use I2C, talk to device at address 0x45 (BEWARE, NOT THE USUAL 0x44!), return if it is acknowledged.
  sgpOk = sgp.begin();
  // When there is a SGP sensor, use it at its only possible I2C address, return if it is acknowledged.

  if (!shtOk) Serial.println("{\"error\":\"SHT3x not found\"}");
  if (!sgpOk) Serial.println("{\"error\":\"SGP30 not found\"}");

  if (sgpOk) sgp.IAQinit(); //Start internal air-quality algorithm and baseline tracking.

  wifiConnect();
}

void loop() {
  uint32_t now = millis();
  bool warmingUp = (now - bootMs) < WARMUP_MS;

  // Sample cadence (SGP30 IAQ wants ~1 Hz; keep SAMPLE_MS around 1000 in config.h)
  if (now - lastSample >= SAMPLE_MS) {
    lastSample = now;

    //Defensive initialization
    float tC = NAN;
    float rh = NAN;

    // If SHT sensor is present and initialized, attempt to read temperature/humidity.  
    if (shtOk) {
      tC = sht31.readTemperature();
      rh = sht31.readHumidity();
    }
    // If the SHT sensor readings are actually valid numerical values, do the math for absolute humidity into SGP compensation
    if (!isnan(tC) && !isnan(rh)) {
      uint32_t ah = absoluteHumidity_mgm3(tC, rh);
      sgp.setHumidity(ah); // From Adafruit SGP30 library
    }

uint16_t tvoc = 0; // Total Volatile Organic Compounds
uint16_t eco2 = 0; // Equivalent CO2

if (sgpOk) {
  bool ok = sgp.IAQmeasure();  // Trigger single SGP measurement step
  if (ok) {
    tvoc = sgp.TVOC; // Total Volatile Organic Compounds
    eco2 = sgp.eCO2; // Equivalent CO2
  }
}



uint16_t idx = tvocToIndex(tvoc);

    uint32_t ledColor;  //led color!
    if (warmingUp) {
      ledColor = pulsingBlue(now);
    } else {
      // Apply hysteresis to stabilize LED color transitions
      idx = applyHysteresis(idx);
      lastAqIndex = idx;
      ledColor = colorForIndex(idx);
    }

setLed(ledColor);

// Build a JSON payload manually (kept small to reduce heap usage on ESP8266)
String json = "{";
json += "\"ts_ms\":" + String(now) + ",";                 // Time since boot (ms)
json += "\"device_id\":\"" + String(DEVICE_ID) + "\",";   // Device ID
json += "\"t_c\":" + String(tC, 2) + ",";                 // Temp (°C)
json += "\"rh\":" + String(rh, 2) + ",";                  // Humidity (%)
json += "\"tvoc_ppb\":" + String(tvoc) + ",";             // TVOC (ppb)
json += "\"eco2_ppm\":" + String(eco2) + ",";             // eCO2 (ppm)
json += "\"aq_index\":" + String(idx) + ",";              // AQ index (0–100)
json += "\"warming_up\":" + String(warmingUp ? "true" : "false"); // Warmup flag
json += "}";  // End JSON

Serial.println(json);      // Serial log
(void)postIngest(json);    // Cloud ingest (best-effort)

  }
}
