# Smart Environment IoT

Monitoreo ambiental con **ESP32 (Wokwi)** + DHT22, LDR y PIR. Los datos viajan por MQTT hasta EMQX, un servicio Python los guarda en InfluxDB y Grafana los visualiza. Todo corre en local.

```
ESP32 (Wokwi/VS Code) --WiFi/MQTT--> EMQX --> mqtt-ingestor (Python) --> InfluxDB --> Grafana
```

## Estructura

```
firmware/esp32/      PlatformIO + Wokwi (src/: config, sensors, demo_scenario, payload, network, main)
  original/          sketch.ino y libraries.txt originales (copia de respaldo)
docker/grafana/      provisioning (datasource, dashboards) y dashboard JSON versionado
services/mqtt-ingestor/  suscriptor MQTT -> InfluxDB (Dockerfile, main.py, tests/)
services/mqtt-bridge/    puente broker publico -> EMQX local
docs/INFORME.md      borrador del informe tecnico
scripts/build-dashboard.mjs  regenera el dashboard (node scripts/build-dashboard.mjs)
docs/evidencias/     evidencias para la TA3
compose.yaml  .env.example
```

## Requisitos

- Docker Desktop (Compose v2), VS Code con extensiones **PlatformIO IDE** y **Wokwi Simulator**.
- Licencia de Wokwi con **Private IoT Gateway** (plan de pago; el ESP32 simulado no puede alcanzar tu PC sin él).
- Node 18+ (solo para regenerar el dashboard).

## Puesta en marcha

1. `copy .env.example .env` y cambia contraseñas y `INFLUXDB_TOKEN`.
2. `docker compose up -d --build` y comprueba con `docker compose ps` (todos `healthy`).
3. Compila el firmware: en VS Code, PlatformIO → *Build* (o `pio run -d firmware/esp32`).
4. Abre la carpeta `firmware/esp32`, pulsa **F1 → "Enable Private Wokwi IoT Gateway"** y luego **F1 → "Wokwi: Start Simulator"**.
5. El monitor serial debe mostrar `[WIFI] Conectado`, `[MQTT] Conectado` y `MQTT: publicado`.

## Sin licencia del Private IoT Gateway: puente por broker publico

Wokwi gratis no puede alcanzar `localhost`. Solucion usada: el ESP32 publica en `broker.emqx.io:1883`
(tópico `<prefijo>/smart-environment/esp32-001/telemetry`) y el servicio `mqtt-bridge` (compose) lo copia al EMQX local,
de donde sigue el flujo normal (ingestor -> InfluxDB -> Grafana).

- El prefijo aleatorio esta en `platformio.ini` (`MQTT_TOPIC_BASE`) y en `.env` (`BRIDGE_TOPIC_PREFIX`); deben coincidir.
- **Limitacion:** los datos pasan por un broker publico sin autenticacion (solo datos de demo) y requiere internet.
  Con licencia de Wokwi se puede volver a `MQTT_HOST=host.wokwi.internal` y quitar el prefijo.

## Servicios y puertos (solo `127.0.0.1`)

| Servicio | URL | Credenciales |
|---|---|---|
| EMQX Dashboard | http://localhost:18083 | `EMQX_DASHBOARD_*` del `.env` |
| MQTT | localhost:1883 | anónimo (demo local) |
| InfluxDB | http://localhost:8086 | `INFLUXDB_ADMIN_*` |
| Grafana | http://localhost:3000 | `GRAFANA_ADMIN_*` → dashboard **GRUPO 1 \| SMART ENVIRONMENT - IoT MONITORING** (cambia `GROUP_NAME` en `scripts/build-dashboard.mjs`) |

## Configuración del firmware

Se cambia en `firmware/esp32/platformio.ini` (`build_flags`): `DEMO_MODE` (1 sintético / 0 sensores Wokwi), `PUBLISH_INTERVAL_MS` (**10000** = lectura cada 10 s, exigido por la consigna), `MQTT_HOST` (`host.wokwi.internal`), `MQTT_PORT`, `MQTT_USER`/`MQTT_PASSWORD`.

Tópicos: `smart-environment/esp32-001/telemetry` y `.../status` (retenido, con Last Will `offline`).

Payload: `device_id, temperature (°C), humidity (%), light_raw (ADC 0-4095, no lux), motion, status, source (synthetic|sensor), sequence, uptime_ms`.

Modelo InfluxDB: measurement `environment`; tags `device_id, source, status`; fields `temperature, humidity, light_raw, motion, status_code, sequence, uptime_ms`; timestamp = hora de recepción.

## Pruebas

```bash
docker compose config -q                      # valida compose
docker compose logs -f mqtt-ingestor          # ver "OK ESP32-001 seq=..."
docker compose exec emqx emqx ctl topics list # tópicos activos
```
Prueba sin ESP32 (solo MQTT → Influx), con `mosquitto_pub` si lo tienes:
`mosquitto_pub -h localhost -t smart-environment/esp32-001/telemetry -m '{"device_id":"ESP32-001","temperature":26.1,"humidity":55.0,"light_raw":1200,"motion":false,"status":"NORMAL","source":"synthetic","sequence":1,"uptime_ms":5000}'`

Tests del ingestor: `pip install -r services/mqtt-ingestor/requirements.txt pytest && pytest services/mqtt-ingestor/tests`

## Errores frecuentes

- **`[MQTT] Fallo, rc=-2`**: gateway de Wokwi no activo o EMQX apagado.
- **Docker "unable to start"**: reinicia Docker Desktop / WSL (`wsl --shutdown`).
- **Grafana sin datos**: espera 5-10 s tras iniciar el simulador; revisa el selector *Dispositivo*.
- **Cambié el nombre del bucket**: el dashboard usa `smart_environment`; regenera con `scripts/build-dashboard.mjs`.

## Seguridad

Credenciales solo en `.env` (ignorado por Git). Puertos ligados a localhost. MQTT sin autenticación ni TLS por simplicidad de la demo; en producción usar usuarios/ACL de EMQX y TLS.
