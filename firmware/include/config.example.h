#pragma once

// Wi-Fi (replace in config.h)
static const char* WIFI_SSID     = "YOUR_WIFI_SSID";
static const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Cloud ingest endpoint (replace in config.h)
static const char* INGEST_URL   = "https://airq-5xv.pages.dev/api/ingest";
static const char* INGEST_TOKEN = "REPLACE_WITH_SECRET_TOKEN";

// Identity
static const char* DEVICE_ID = "airq-d1mini-01";

// Sampling
static const uint32_t SAMPLE_MS = 1000;

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
