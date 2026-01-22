-- AirQ Database Schema for D1
-- Stores sensor readings from ESP8266 firmware

CREATE TABLE IF NOT EXISTS readings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    ts_ms INTEGER NOT NULL,
    temperature REAL,
    humidity REAL,
    tvoc_ppb INTEGER,
    eco2_ppm INTEGER,
    aq_index INTEGER,
    warming_up INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_readings_created_at ON readings(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_readings_device ON readings(device_id);

