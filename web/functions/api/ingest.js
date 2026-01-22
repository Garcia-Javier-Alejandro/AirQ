// AirQ Ingest API - Cloudflare Pages Function
// Receives sensor data from ESP8266 firmware

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

    // Log to Cloudflare (visible in deployment logs)
    console.log(`[${data.device_id}] TVOC=${data.tvoc_ppb}ppb AQ=${data.aq_index} temp=${data.t_c}°C`);

    // TODO: Add to D1 database when ready
    
    return new Response(JSON.stringify({ 
      success: true,
      device_id: data.device_id,
      timestamp: new Date().toISOString()
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

// Health check endpoint
export async function onRequestGet() {
  return new Response(JSON.stringify({ 
    status: "ok",
    endpoint: "/api/ingest",
    methods: ["POST"],
    expected_fields: ["device_id", "ts_ms", "t_c", "rh", "tvoc_ppb", "eco2_ppm", "aq_index", "warming_up"]
  }), {
    headers: { "Content-Type": "application/json" }
  });
}


