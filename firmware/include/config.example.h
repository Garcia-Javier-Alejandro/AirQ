#pragma once

// Wi-Fi
static const char* WIFI_SSID     = "YOUR_WIFI_SSID";
static const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// HiveMQ MQTT Broker
static const char* MQTT_BROKER   = "your-cluster.hivemq.cloud";
static const uint16_t MQTT_PORT  = 8883;
static const char* MQTT_USERNAME = "your-username";
static const char* MQTT_PASSWORD = "your-password";
static const char* MQTT_TOPIC    = "airq/your-device-id";

// Identity
static const char* DEVICE_ID = "airq-d1mini-01";

// Sampling
static const uint32_t SAMPLE_MS = 2000;

// NeoPixel shield
static const uint8_t PIN_NEOPIXEL = D4;   // common default for D1 mini shields
static const uint16_t N_LEDS = 1;

// UX / warmup
static const uint32_t WARMUP_MS = 60000;

// LED brightness cap (0..255)
static const uint8_t LED_BRIGHTNESS = 40;

// AQ Index thresholds and hysteresis
// Thresholds define the color transitions: green (0-LOW), yellow (LOW-HIGH), red (HIGH-100)
// Hysteresis margin prevents flickering near boundaries
static const uint8_t AQ_THRESHOLD_LOW = 20;     // Green → Yellow transition
static const uint8_t AQ_THRESHOLD_HIGH = 60;    // Yellow → Red transition
static const uint8_t AQ_HYSTERESIS_BAND = 5;    // Margin to prevent flickering (ppb)
