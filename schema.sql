-- AirQ historical data storage (30-minute aggregates)
CREATE TABLE IF NOT EXISTS airq_readings (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  timestamp DATETIME NOT NULL UNIQUE,
  device_id TEXT NOT NULL DEFAULT 'airq-d1mini-01',
  aq_index_avg REAL NOT NULL,
  tvoc_ppb_avg REAL NOT NULL,
  eco2_ppm_avg REAL NOT NULL,
  temp_c_avg REAL NOT NULL,
  rh_avg REAL NOT NULL,
  sample_count INTEGER NOT NULL DEFAULT 1,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_timestamp ON airq_readings(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_device_id ON airq_readings(device_id);
