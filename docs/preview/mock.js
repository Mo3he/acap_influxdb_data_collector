// Fictional connection and collection settings; refresh the copied UI and sample settings response from the app UI when it changes.
"use strict";

let sampleSettings = {
  influxdb: {
    url: "http://influxdb.example.invalid:8086",
    org: "example-organisation",
    bucket: "camera-sample",
    token: "",
    enabled: true,
  },
  dataCollection: {
    pollIntervalSeconds: 30,
    selectedDataTypes: {
      cpu: true,
      memory: true,
      network: true,
      temperature: false,
      uptime: true,
      storage: false,
      thermometry: false,
      spot_temperature: false,
      air_quality: false,
      people_counter: false,
    },
  },
  measurement: { name: "device_metrics" },
};

function response(body, status = 200) {
  return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });
}

window.fetch = async (input, init = {}) => {
  const url = new URL(typeof input === "string" ? input : input.url, location.href);
  if (url.origin !== location.origin)
    return new Response("Offline preview blocks external requests.", { status: 403 });
  if (url.pathname.endsWith("/settings") && (!init.method || init.method === "GET")) return response(sampleSettings);
  if (url.pathname.endsWith("/settings") && init.method === "POST") {
    const submitted = JSON.parse(init.body || "{}");
    sampleSettings = {
      ...submitted,
      influxdb: { ...submitted.influxdb, token: "" },
      measurement: { name: "device_metrics" },
    };
    return response({ ok: true });
  }
  if (url.pathname.endsWith("/test"))
    return new Response("Offline preview: no InfluxDB connection was attempted.", { status: 403 });
  return new Response("Offline preview: this endpoint is not available.", { status: 404 });
};
