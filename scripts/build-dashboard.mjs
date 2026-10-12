// Genera docker/grafana/dashboards/smart-environment.json
// Uso: node scripts/build-dashboard.mjs
import { writeFileSync } from "node:fs";

const DS = { type: "influxdb", uid: "influx-flux" };
const base = (field, range = "v.timeRangeStart, stop: v.timeRangeStop") =>
  `from(bucket: "smart_environment")
  |> range(start: ${range})
  |> filter(fn: (r) => r._measurement == "environment" and r.device_id == "\${device}" and r._field == "${field}")`;

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
const motionInt = `\n  |> map(fn: (r) => ({r with _value: if r._value then 1 else 0}))`;

const panels = [
  stat("Temperatura actual", 0, 0, 4, base("temperature") + "\n  |> last()",
    { unit: "celsius", decimals: 1, steps: tempSteps }),
  stat("Humedad actual", 4, 0, 4, base("humidity") + "\n  |> last()",
    { unit: "humidity", decimals: 1, steps: [{ color: BLUE, value: null }] }),
  stat("Iluminación (ADC 0-4095)", 8, 0, 4, base("light_raw") + "\n  |> last()",
    { unit: "none", decimals: 0, steps: [{ color: "#8e44ad", value: null }] }),
  stat("Movimiento", 12, 0, 4, base("motion") + motionInt + "\n  |> last()",
    { colorMode: "background", graph: "none",
      mappings: [{ type: "value", options: { "0": { text: "SIN MOVIMIENTO", color: GREEN }, "1": { text: "DETECTADO", color: YELLOW } } }],
      steps: [{ color: GREEN, value: null }] }),
  stat("Estado general", 16, 0, 4, base("status_code") + "\n  |> last()",
    { graph: "none",
      mappings: [{ type: "value", options: { "0": { text: "NORMAL", color: GREEN }, "1": { text: "WARNING", color: YELLOW }, "2": { text: "CRITICAL", color: RED } } }],
      steps: [{ color: GREEN, value: null }] }),
  stat("Mediciones recibidas", 20, 0, 4, base("sequence") + "\n  |> count()",
    { unit: "none", graph: "none", steps: [{ color: BLUE, value: null }] }),

  stat("Temperatura máxima", 0, 4, 4, base("temperature") + "\n  |> max()",
    { unit: "celsius", decimals: 1, graph: "none", steps: tempSteps }),
  stat("Temperatura mínima", 4, 4, 4, base("temperature") + "\n  |> min()",
    { unit: "celsius", decimals: 1, graph: "none", steps: tempSteps }),
  stat("Temperatura promedio", 8, 4, 4, base("temperature") + "\n  |> mean()",
    { unit: "celsius", decimals: 1, graph: "none", steps: tempSteps }),
  stat("Conexión del dispositivo", 12, 4, 6,
    base("sequence", "-30s") + "\n  |> count()",
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

// Nombre del grupo (aparece en el titulo del dashboard). CAMBIAR por el nombre real del equipo.
const GROUP_NAME = process.env.GROUP_NAME || "GRUPO 1";

const dashboard = {
  uid: "smart-environment",
  title: `${GROUP_NAME} | SMART ENVIRONMENT - IoT MONITORING`,
  tags: ["iot", "esp32", "mqtt"],
  timezone: "browser",
  schemaVersion: 39,
  version: 1,
  refresh: "5s",
  time: { from: "now-15m", to: "now" },
  timepicker: { refresh_intervals: ["5s", "10s", "30s", "1m"] },
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
