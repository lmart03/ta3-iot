// Genera docker/grafana/dashboards/smart-environment.json
// Uso: node scripts/build-dashboard.mjs
import { writeFileSync } from "node:fs";

const DS = { type: "influxdb", uid: "influx-flux" };
const base = (field, range = "v.timeRangeStart, stop: v.timeRangeStop") =>
  `from(bucket: "smart_environment")
  |> range(start: ${range})
  |> filter(fn: (r) => r._measurement == "environment" and r.device_id == "\${device}" and r._field == "${field}")
  |> group(columns: ["_field"])
  |> sort(columns: ["_time"])`;

const MQTT_DS = { type: "grafana-mqtt-datasource", uid: "mqtt-live" };
const LIVE_TOPIC = "smart-environment/esp32-001/telemetry";
const liveTarget = [{ datasource: MQTT_DS, refId: "A", topic: LIVE_TOPIC }];

const GREEN = "#2ecc71", YELLOW = "#f1c40f", RED = "#e74c3c", BLUE = "#3498db";
const tempSteps = [
  { color: GREEN, value: null },
  { color: YELLOW, value: 30 },
  { color: RED, value: 35 },
];

let id = 0;
const target = (flux) => [{ datasource: DS, refId: "A", query: flux }];
const stat = (title, x, y, w, flux, o = {}) => ({
  id: ++id, type: "stat", title, datasource: DS, gridPos: { x, y, w, h: 4 },
  targets: target(flux),
  fieldConfig: { defaults: { unit: o.unit, decimals: o.decimals, mappings: o.mappings ?? [],
    thresholds: { mode: "absolute", steps: o.steps ?? [{ color: BLUE, value: null }] },
    noValue: o.noValue ?? "Sin datos" }, overrides: [] },
  options: { colorMode: o.colorMode ?? "background", graphMode: o.graph ?? "area", textMode: "value",
    reduceOptions: { calcs: [o.calc ?? "lastNotNull"], fields: "", values: false } },
});
const series = (title, x, y, w, field, unit, o = {}) => ({
  id: ++id, type: "timeseries", title, datasource: DS, gridPos: { x, y, w, h: 8 },
  targets: target(base(field) + `\n  |> aggregateWindow(every: v.windowPeriod, fn: mean, createEmpty: false)`),
  fieldConfig: { defaults: { unit, custom: { lineWidth: 2, fillOpacity: 15, showPoints: "never",
      thresholdsStyle: { mode: o.steps ? "line+area" : "off" } },
    color: { mode: o.steps ? "thresholds" : "fixed", fixedColor: BLUE },
    thresholds: { mode: "absolute", steps: o.steps ?? [{ color: BLUE, value: null }] } }, overrides: [] },
  options: { legend: { displayMode: "table", placement: "bottom", calcs: ["min", "mean", "max", "lastNotNull"] },
    tooltip: { mode: "multi", sort: "none" } },
});

// Tarjetas en vivo (WebSocket): el valor se actualiza en cuanto llega el mensaje MQTT.
const liveStat = (title, x, field, o = {}) => ({
  id: ++id, type: "stat", title: `${title} (EN VIVO)`, datasource: MQTT_DS, gridPos: { x, y: 0, w: 4, h: 4 },
  targets: liveTarget,
  fieldConfig: { defaults: { unit: o.unit, decimals: o.decimals, mappings: o.mappings ?? [],
    thresholds: { mode: "absolute", steps: o.steps ?? [{ color: BLUE, value: null }] }, noValue: "Sin datos" }, overrides: [] },
  options: { colorMode: "background", graphMode: o.graph ?? "none", textMode: "value",
    reduceOptions: { calcs: ["lastNotNull"], fields: `/^${field}$/`, values: false } },
});
const motionInt = `\n  |> map(fn: (r) => ({r with _value: if r._value then 1 else 0}))`;

const panels = [
  liveStat("Temperatura", 0, "temperature", { unit: "celsius", decimals: 1, steps: tempSteps, graph: "area" }),
  liveStat("Humedad", 4, "humidity", { unit: "humidity", decimals: 1, graph: "area" }),
  liveStat("Iluminación (ADC)", 8, "light_raw", { unit: "none", decimals: 0, steps: [{ color: "#8e44ad", value: null }], graph: "area" }),
  liveStat("Movimiento", 12, "motion", {
    mappings: [{ type: "value", options: { "false": { text: "SIN MOVIMIENTO", color: GREEN }, "true": { text: "DETECTADO", color: YELLOW } } }],
    steps: [{ color: GREEN, value: null }] }),
  liveStat("Estado", 16, "status", {
    mappings: [{ type: "value", options: { NORMAL: { text: "NORMAL", color: GREEN }, WARNING: { text: "WARNING", color: YELLOW }, CRITICAL: { text: "CRITICAL", color: RED } } }],
    steps: [{ color: GREEN, value: null }] }),
  stat("Mediciones recibidas", 20, 0, 4, base("sequence") + "\n  |> count()",
    { unit: "none", graph: "none", steps: [{ color: BLUE, value: null }] }),

  stat("Temperatura máxima", 0, 4, 4, base("temperature") + "\n  |> max()",
    { unit: "celsius", decimals: 1, graph: "none", steps: tempSteps }),
  stat("Temperatura mínima", 4, 4, 4, base("temperature") + "\n  |> min()",
    { unit: "celsius", decimals: 1, graph: "none", steps: tempSteps }),
  stat("Temperatura promedio", 8, 4, 4, base("temperature") + "\n  |> mean()",
    { unit: "celsius", decimals: 1, graph: "none", steps: tempSteps }),
  // Los 90 s son una tolerancia de monitoreo para la simulacion (Wokwi a velocidad
  // reducida); no es una prueba directa del estado de la conexion TCP/MQTT.
  stat("Conexión del dispositivo", 12, 4, 6,
    base("sequence", "-90s") + "\n  |> count()",
    { graph: "none", noValue: "OFFLINE",
      mappings: [{ type: "range", options: { from: 1, to: 1e9, result: { text: "ONLINE", color: GREEN } } }],
      steps: [{ color: RED, value: null }, { color: GREEN, value: 1 }] }),
  stat("Última medición recibida", 18, 4, 6,
    base("sequence", "-7d") +
    `\n  |> last()\n  |> map(fn: (r) => ({r with _value: float(v: int(v: r._time)) / 1000000.0}))`,
    { unit: "dateTimeFromNow", graph: "none", steps: [{ color: BLUE, value: null }] }),

  series("Temperatura", 0, 8, 12, "temperature", "celsius", { steps: tempSteps }),
  series("Humedad", 12, 8, 12, "humidity", "humidity"),
  series("Iluminación (ADC)", 0, 16, 12, "light_raw", "none"),
  {
    id: ++id, type: "state-timeline", title: "Historial de detecciones de movimiento", datasource: DS,
    gridPos: { x: 12, y: 16, w: 12, h: 8 },
    targets: target(base("motion") + motionInt),
    fieldConfig: { defaults: { custom: { fillOpacity: 80 },
      mappings: [{ type: "value", options: { "0": { text: "NO", color: GREEN }, "1": { text: "MOVIMIENTO", color: YELLOW } } }],
      thresholds: { mode: "absolute", steps: [{ color: GREEN, value: null }] } }, overrides: [] },
    options: { legend: { showLegend: true, placement: "bottom", displayMode: "list" }, mergeValues: true, showValue: "auto" },
  },
];

// Fila de monitoreo: consulta periodica a InfluxDB (cada 10 s) con historial persistente.
// El plugin MQTT no almacena series temporales, por eso estos graficos usan influx-flux.
const monitoringPanel = (title, x, w, field, unit, color) => ({
  id: ++id, type: "timeseries", title: `${title} (ACTUALIZACION 10 s)`, datasource: DS, gridPos: { x, y: 4, w, h: 7 },
  targets: target(base(field, "-10m") + `\n  |> aggregateWindow(every: 10s, fn: mean, createEmpty: false)`),
  fieldConfig: { defaults: { unit, custom: { lineWidth: 2, fillOpacity: 15, showPoints: "always", pointSize: 3 },
    color: { mode: "fixed", fixedColor: color } }, overrides: [] },
  options: { legend: { showLegend: false }, tooltip: { mode: "single" } },
});
const monitoringPanels = [
  monitoringPanel("Temperatura", 0, 8, "temperature", "celsius", RED),
  monitoringPanel("Humedad", 8, 8, "humidity", "humidity", BLUE),
  monitoringPanel("Iluminación", 16, 8, "light_raw", "none", "#8e44ad"),
];
for (const p of panels) if (p.datasource !== MQTT_DS && p.gridPos.y > 0) p.gridPos.y += 7;
panels.push(...monitoringPanels);

// Nombre del grupo (aparece en el titulo del dashboard). CAMBIAR por el nombre real del equipo.
const GROUP_NAME = process.env.GROUP_NAME || "GRUPO 1";

const dashboard = {
  uid: "smart-environment",
  title: `${GROUP_NAME} | SMART ENVIRONMENT - IoT MONITORING`,
  tags: ["iot", "esp32", "mqtt"],
  timezone: "browser",
  schemaVersion: 39,
  version: 1,
  refresh: "10s",
  time: { from: "now-30m", to: "now" },
  timepicker: { refresh_intervals: ["1s", "2s", "5s", "10s", "30s", "1m"] },
  templating: { list: [{
    name: "device", label: "Dispositivo", type: "query", datasource: DS,
    query: `import "influxdata/influxdb/schema"\nschema.tagValues(bucket: "smart_environment", tag: "device_id")`,
    refresh: 2, sort: 1, includeAll: false, multi: false,
    current: { text: "ESP32-001", value: "ESP32-001" },
  }] },
  annotations: { list: [] },
  panels,
};

writeFileSync(
  new URL("../docker/grafana/dashboards/smart-environment.json", import.meta.url),
  JSON.stringify(dashboard, null, 2) + "\n",
);
console.log(`Dashboard generado con ${panels.length} paneles`);
