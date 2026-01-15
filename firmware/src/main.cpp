#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>

#include <Adafruit_SGP30.h>
#include <Adafruit_SHT31.h>
#include <Adafruit_NeoPixel.h>

// You will copy config.example.h -> config.h and edit it
#include "config.h"

Adafruit_NeoPixel leds(N_LEDS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
Adafruit_SGP30 sgp;
Adafruit_SHT31 sht31 = Adafruit_SHT31();

static uint32_t bootMs = 0;
static uint32_t lastSample = 0;

static void setLed(uint32_t c) {
  leds.setBrightness(LED_BRIGHTNESS);
  leds.setPixelColor(0, c);
  leds.show();
}

static uint32_t pulsingBlue(uint32_t nowMs) {
  float phase = (nowMs % 1000) / 1000.0f;
  float tri = (phase < 0.5f) ? (phase * 2.0f) : (2.0f - phase * 2.0f);
  uint8_t b = (uint8_t)(20 + 120 * tri);
  return leds.Color(0, 0, b);
}

// 0..200 index from TVOC ppb (heuristic; tune later)
static uint16_t tvocToIndex(uint16_t tvoc) {
  if (tvoc <= 500) return (uint16_t)(tvoc / 50);               // 0..10
  if (tvoc <= 2000) return (uint16_t)(10 + (tvoc - 500) / 10); // 10..160
  return 200;
}

static uint32_t colorForIndex(uint16_t idx) {
  idx = (idx > 200) ? 200 : idx;
  if (idx <= 10) return leds.Color(0, 255, 0);

  float t = (idx - 10) / 190.0f;
  if (t < 0.5f) {
    float u = t / 0.5f; // green->yellow
    return leds.Color((uint8_t)(255 * u), 255, 0);
  } else {
    float u = (t - 0.5f) / 0.5f; // yellow->red
    return leds.Color(255, (uint8_t)(255 * (1.0f - u)), 0);
  }
}

// absolute humidity (mg/m^3) for SGP30 compensation
static uint32_t absoluteHumidity_mgm3(float tempC, float rh) {
  // saturation vapor pressure (hPa)
  float svp = 6.112f * expf((17.62f * tempC) / (243.12f + tempC));
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

static bool postIngest(const String& json) {
  if (WiFi.status() != WL_CONNECTED) return false;

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure(); // MVP; later: CA bundle or pinning

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

  bool shtOk = sht31.begin(0x44);
  bool sgpOk = sgp.begin();

  if (!shtOk) Serial.println("{\"error\":\"SHT3x not found\"}");
  if (!sgpOk) Serial.println("{\"error\":\"SGP30 not found\"}");

  if (sgpOk) sgp.IAQinit();

  wifiConnect();
}

void loop() {
  uint32_t now = millis();
  bool warmingUp = (now - bootMs) < WARMUP_MS;

  if (now - lastSample >= SAMPLE_MS) {
    lastSample = now;

    float tC = sht31.readTemperature();
    float rh = sht31.readHumidity();

    if (!isnan(tC) && !isnan(rh)) {
      uint32_t ah = absoluteHumidity_mgm3(tC, rh);
      sgp.setHumidity(ah);
    }

    bool ok = sgp.IAQmeasure();
    uint16_t tvoc = ok ? sgp.TVOC : 0;
    uint16_t eco2 = ok ? sgp.eCO2 : 0;

    uint16_t idx = tvocToIndex(tvoc);

    setLed(warmingUp ? pulsingBlue(now) : colorForIndex(idx));

    String json = "{";
    json += "\"ts_ms\":" + String(now) + ",";
    json += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
    json += "\"t_c\":" + String(tC, 2) + ",";
    json += "\"rh\":" + String(rh, 2) + ",";
    json += "\"tvoc_ppb\":" + String(tvoc) + ",";
    json += "\"eco2_ppm\":" + String(eco2) + ",";
    json += "\"aq_index\":" + String(idx) + ",";
    json += "\"warming_up\":" + String(warmingUp ? "true" : "false");
    json += "}";

    Serial.println(json);

    // non-blocking philosophy: best-effort post; if it fails, next sample tries again
    (void)postIngest(json);
  }
}
