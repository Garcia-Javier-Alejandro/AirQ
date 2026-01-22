// AirQ Ingest API - Cloudflare Pages Function
// Receives sensor data from ESP8266 firmware and stores in D1

export async function onRequestPost(context) {
  try {
    const data = await context.request.json();
    
    // Basic validation
    if (!data.device_id || typeof data.tvoc_ppb === 'undefined') {
      return new Response(JSON.stringify({ error: "Missing required fields" }), {
        status: 400,
        headers: { "Content-Type": "application/json" }
      });
    }

    // Save to D1 database
    if (context.env.DB) {
      await context.env.DB.prepare(
        `INSERT INTO readings (device_id, ts_ms, temperature, humidity, tvoc_ppb, eco2_ppm, aq_index, warming_up)
         VALUES (?, ?, ?, ?, ?, ?, ?, ?)`
      ).bind(
        data.device_id,
        data.ts_ms,
        data.t_c || null,
        data.rh || null,
        data.tvoc_ppb,
        data.eco2_ppm || null,
        data.aq_index || null,
        data.warming_up ? 1 : 0
      ).run();
    }

    console.log(`[${data.device_id}] TVOC=${data.tvoc_ppb}ppb AQ=${data.aq_index}`);

    return new Response(JSON.stringify({ 
      success: true,
      device_id: data.device_id
    }), {
      status: 200,
      headers: { "Content-Type": "application/json" }
    });

  } catch (error) {
    console.error("Ingest error:", error);
    return new Response(JSON.stringify({ error: "Invalid request" }), {
      status: 400,
      headers: { "Content-Type": "application/json" }
    });
  }
}

// Health check
export async function onRequestGet() {
  return new Response(JSON.stringify({ 
    status: "ok",
    endpoint: "/api/ingest",
    methods: ["POST"]
  }), {
    headers: { "Content-Type": "application/json" }
  });
}


