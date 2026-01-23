# Adafruit IO Setup Guide

**Temporary workaround for ESP8266 TLS limitations with Cloudflare**

The ESP8266 cannot negotiate modern TLS with Cloudflare (returns HTTP -1 errors). As a temporary solution, we're using Adafruit IO's MQTT service, which is ESP8266-friendly and uses plain TCP on port 1883.

## Why Adafruit IO?

✅ **ESP8266 compatible**: Plain TCP MQTT (no TLS issues)
✅ **Free tier**: 30 data points/minute, sufficient for testing
✅ **Built-in dashboard**: Visualization without custom code
✅ **Easy setup**: 5 minutes to get credentials

## Setup Steps

### 1. Create Adafruit IO Account

1. Go to [https://io.adafruit.com/](https://io.adafruit.com/)
2. Click **Get Started for Free**
3. Create account (free tier is sufficient)

### 2. Get Your Credentials

**After logging in, look for your account credentials:**

**Option A (Newer Interface):**
1. In the top right corner, click your **username/profile icon**
2. Select **"Manage Account"** or **"My Key"**
3. You'll see your:
   - **Adafruit IO Username**
   - **Active Key** (your password/secret key)

**Option B (Alternative):**
1. Click on the **key icon** or **settings icon** (usually in top right corner next to username)
2. Select **"Active Key"** or **"View AIO Key"**
3. Copy your username and key from the modal that appears

**If you still can't find it:**
- Go directly to: https://io.adafruit.com/settings
- Scroll to **"Adafruit IO API Keys"** section
- You'll see your username and key listed there

### 3. Create a Feed

1. Go to **Feeds** in the top menu
2. Click **+ New Feed**
3. Enter:
   - **Name**: `airq`
   - **Description**: Air quality sensor data
4. Click **Create**

### 4. Update firmware/include/config.h

```cpp
static const char* AIO_USERNAME = "YOUR_USERNAME_HERE";  // From step 2
static const char* AIO_KEY      = "YOUR_AIO_KEY_HERE";   // From step 2
static const char* AIO_FEED     = "airq";                // From step 3
```

### 5. Build and Upload Firmware

```bash
cd firmware
pio run -t upload
pio device monitor
```

You should see:
```
[MQTT] Connecting to Adafruit IO... Connected!
[AIO] Publishing... ✓
```

### 6. View Data on Adafruit IO

1. Go to **Feeds** → **airq**
2. You'll see JSON data arriving every 2 seconds
3. Click **Create Dashboard** to visualize (or use the AirQ web dashboard)

## Free Tier Limits

- **30 data points per minute** (we're sending every 2 seconds = 30/min, exactly at limit)
- **30 days data retention**
- **10 feeds total**

If you hit rate limits, increase `SAMPLE_MS` to 3000 or 4000 in config.h.

## Migration Path to Cloudflare (Future)

When you upgrade to **ESP32** hardware:

1. Uncomment Cloudflare `INGEST_URL` in config.h
2. Switch back to HTTPS POST method
3. Keep Adafruit IO as backup/fallback
4. The ESP32's better TLS stack will handle Cloudflare perfectly

See README TODO section for ESP32 migration notes.

## Troubleshooting

### "MQTT not connected" errors

- Check AIO_USERNAME and AIO_KEY are correct
- Verify WiFi is connected
- Free tier allows only 1 active connection; close any open dashboards/feeds

### Rate limit errors

- Increase SAMPLE_MS to 3000 or higher
- Free tier: 30 data points/minute

### Feed not showing data

- Verify feed name matches AIO_FEED in config.h
- Check serial monitor for "[AIO] Publishing... ✓"
- Data can take 10-20 seconds to appear on dashboard

## Resources

- [Adafruit IO Documentation](https://io.adafruit.com/api/docs/)
- [MQTT API Reference](https://io.adafruit.com/api/docs/mqtt.html)
- [Dashboard Creation Guide](https://learn.adafruit.com/adafruit-io-basics-dashboards)
