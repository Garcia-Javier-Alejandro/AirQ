/**
 * Cloudflare Worker: AirQ Data Aggregation & Storage
 * 
 * Endpoints:
 * POST   /api/readings   - Receive 5s readings, aggregate to 30min, store in D1
 * GET    /api/history    - Retrieve historical 30-minute aggregates
 */

import { Router } from 'itty-router';

const router = Router();

// In-memory buffer for current 30-minute window
const readingBuffer = new Map();

// Helper: Get 30-minute bucket timestamp (rounds down to nearest 30 min)
function getBucketTimestamp(date = new Date()) {
  const minutes = Math.floor(date.getMinutes() / 30) * 30;
  const bucketed = new Date(date);
  bucketed.setMinutes(minutes, 0, 0);
  return bucketed.toISOString();
}

// POST /api/readings - Receive reading and aggregate
router.post('/api/readings', async (request, env) => {
  try {
    const reading = await request.json();
    
    if (!reading.ts_ms || reading.aq_index === undefined) {
      return new Response(
        JSON.stringify({ error: 'Missing ts_ms or aq_index' }),
        { status: 400 }
      );
    }

    const readingDate = new Date(reading.ts_ms);
    const bucketKey = getBucketTimestamp(readingDate);
    const deviceId = reading.device_id || 'airq-d1mini-01';

    // Get or initialize bucket
    if (!readingBuffer.has(bucketKey)) {
      readingBuffer.set(bucketKey, {
        device_id: deviceId,
        aq_index: [],
        tvoc_ppb: [],
        eco2_ppm: [],
        temp_c: [],
        rh: [],
        count: 0
      });
    }

    const bucket = readingBuffer.get(bucketKey);
    bucket.aq_index.push(reading.aq_index);
    bucket.tvoc_ppb.push(reading.tvoc_ppb || 0);
    bucket.eco2_ppm.push(reading.eco2_ppm || 400);
    bucket.temp_c.push(reading.t_c || 0);
    bucket.rh.push(reading.rh || 0);
    bucket.count++;

    // Check if previous bucket is complete (30 min passed) and save it
    const allBuckets = Array.from(readingBuffer.keys()).sort();
    for (const oldBucket of allBuckets) {
      const oldBucketDate = new Date(oldBucket);
      const timeDiff = readingDate - oldBucketDate;

      // If difference > 30 minutes and bucket is not current, save it
      if (timeDiff > 30 * 60 * 1000 && oldBucket !== bucketKey) {
        await saveBucketToD1(env, oldBucket, readingBuffer.get(oldBucket));
        readingBuffer.delete(oldBucket);
      }
    }

    return new Response(
      JSON.stringify({
        success: true,
        bucket: bucketKey,
        samples_in_bucket: bucket.count
      }),
      { status: 200 }
    );
  } catch (error) {
    return new Response(
      JSON.stringify({ error: error.message }),
      { status: 500 }
    );
  }
});

// GET /api/history?days=7 - Retrieve historical data
router.get('/api/history', async (request, env) => {
  try {
    const url = new URL(request.url);
    const days = parseInt(url.searchParams.get('days') || '7');
    const deviceId = url.searchParams.get('device_id') || 'airq-d1mini-01';

    const db = env.DB;
    const since = new Date(Date.now() - days * 24 * 60 * 60 * 1000).toISOString();

    const data = await db
      .prepare(
        `SELECT timestamp, aq_index_avg, tvoc_ppb_avg, eco2_ppm_avg, temp_c_avg, rh_avg, sample_count
         FROM airq_readings
         WHERE device_id = ? AND timestamp >= ?
         ORDER BY timestamp DESC
         LIMIT 672`
      )
      .bind(deviceId, since)
      .all();

    return new Response(
      JSON.stringify({
        success: true,
        device_id: deviceId,
        days,
        count: data.results ? data.results.length : 0,
        data: data.results || []
      }),
      { status: 200, headers: { 'Content-Type': 'application/json' } }
    );
  } catch (error) {
    return new Response(
      JSON.stringify({ error: error.message }),
      { status: 500 }
    );
  }
});

// Helper: Save completed bucket to D1
async function saveBucketToD1(env, timestamp, bucket) {
  try {
    const db = env.DB;

    // Calculate averages
    const avg = (arr) => arr.reduce((a, b) => a + b, 0) / arr.length;

    const values = {
      timestamp: timestamp,
      device_id: bucket.device_id,
      aq_index_avg: Math.round(avg(bucket.aq_index) * 10) / 10,
      tvoc_ppb_avg: Math.round(avg(bucket.tvoc_ppb) * 10) / 10,
      eco2_ppm_avg: Math.round(avg(bucket.eco2_ppm)),
      temp_c_avg: Math.round(avg(bucket.temp_c) * 100) / 100,
      rh_avg: Math.round(avg(bucket.rh) * 100) / 100,
      sample_count: bucket.count
    };

    // Insert or ignore if already exists
    await db
      .prepare(
        `INSERT OR IGNORE INTO airq_readings 
         (timestamp, device_id, aq_index_avg, tvoc_ppb_avg, eco2_ppm_avg, temp_c_avg, rh_avg, sample_count)
         VALUES (?, ?, ?, ?, ?, ?, ?, ?)`
      )
      .bind(
        values.timestamp,
        values.device_id,
        values.aq_index_avg,
        values.tvoc_ppb_avg,
        values.eco2_ppm_avg,
        values.temp_c_avg,
        values.rh_avg,
        values.sample_count
      )
      .run();

    console.log(`Saved 30-min aggregate for ${timestamp}: ${bucket.count} samples`);
  } catch (error) {
    console.error(`Failed to save bucket: ${error.message}`);
  }
}

// 404 fallback
router.all('*', () => new Response('Not Found', { status: 404 }));

export default {
  fetch: router.handle
};
