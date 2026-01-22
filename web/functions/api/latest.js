// Get recent readings from last hour
export async function onRequestGet(context) {
  try {
    if (!context.env.DB) {
      return new Response(JSON.stringify({ error: "Database not configured" }), {
        status: 500,
        headers: { "Content-Type": "application/json" }
      });
    }

    // Get readings from last hour
    const { results } = await context.env.DB.prepare(
      `SELECT device_id, ts_ms, temperature, humidity, tvoc_ppb, eco2_ppm, aq_index, warming_up, created_at
       FROM readings
       WHERE created_at >= datetime('now', '-1 hour')
       ORDER BY created_at ASC`
    ).all();

    // Transform to match firmware JSON format
    const readings = results.map(r => ({
      ts_ms: r.ts_ms,
      device_id: r.device_id,
      t_c: r.temperature,
      rh: r.humidity,
      tvoc_ppb: r.tvoc_ppb,
      eco2_ppm: r.eco2_ppm,
      aq_index: r.aq_index,
      warming_up: r.warming_up === 1
    }));

    return new Response(JSON.stringify(readings), {
      headers: { 
        "Content-Type": "application/json",
        "Access-Control-Allow-Origin": "*"
      }
    });

  } catch (error) {
    console.error("Latest API error:", error);
    return new Response(JSON.stringify({ error: "Failed to fetch readings" }), {
      status: 500,
      headers: { "Content-Type": "application/json" }
    });
  }
}
