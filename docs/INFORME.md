# Informe técnico – Tarea Académica N.º 3
**Smart Environment IoT: monitoreo de temperatura con ESP32, MQTT, InfluxDB y Grafana**

**Curso:** _completar_ · **Grupo:** _completar_ · **Fecha:** _completar_

**Integrantes y roles**

| Integrante | Rol / tareas |
|---|---|
| _Nombre 1_ | Firmware ESP32 / Wokwi |
| _Nombre 2_ | Broker EMQX y Docker |
| _Nombre 3_ | Ingestor Python e InfluxDB |
| _Nombre 4_ | Dashboard Grafana |
| _Nombre 5_ | Informe y evidencias |

(Ajustar a los roles reales; deben coincidir con los commits del repositorio.)

## 1. Objetivo
Implementar una arquitectura IoT funcional: adquisición de datos con un nodo simulado, transmisión por MQTT a un broker local, almacenamiento en base de datos y visualización en tiempo real en Grafana.

## 2. Arquitectura
```
ESP32 (Wokwi) → broker.emqx.io → mqtt-bridge → EMQX local → mqtt-ingestor → InfluxDB → Grafana
```
Todo el backend corre en Docker Compose (`compose.yaml`) con imágenes de versión fija: EMQX 5.8.6, InfluxDB 2.7.11, Grafana 11.4.0 y dos servicios Python propios (ingestor y bridge).

## 3. Implementación por partes
### 3.1 Nodo IoT (Parte 1)
- ESP32 DevKit C simulado en Wokwi con DHT22 (GPIO 15), LDR (GPIO 34) y PIR (GPIO 27); proyecto PlatformIO en `firmware/esp32`.
- Lectura y publicación **cada 10 s** (`PUBLISH_INTERVAL_MS=10000`).
- Modo demo (`DEMO_MODE=1`) con un escenario sintético de 5 min (estable → calentamiento → crítico → enfriamiento) y modo sensores Wokwi (`DEMO_MODE=0`).
- Estado calculado en el dispositivo: NORMAL, WARNING (≥ 30 °C) y CRITICAL (≥ 35 °C).
- Evidencia: _captura del monitor serial_ (`docs/evidencias/`).

### 3.2 Broker EMQX (Parte 2)
- Contenedor `emqx/emqx:5.8.6`, puertos 1883 (MQTT) y 18083 (dashboard) ligados a `127.0.0.1`, volúmenes persistentes y healthcheck.
- Evidencia: _captura del dashboard de EMQX con el cliente conectado_.

### 3.3 Publicación MQTT (Parte 3)
- Tópicos `smart-environment/esp32-001/telemetry` y `.../status` (retenido, con Last Will `offline`).
- Payload JSON: `device_id, temperature, humidity, light_raw, motion, status, source, sequence, uptime_ms`.
- Evidencia: _captura de mensajes recibidos en EMQX / `docker compose logs mqtt-ingestor`_.

### 3.4 Almacenamiento (Parte 4)
- `mqtt-ingestor` se suscribe a `smart-environment/+/telemetry`, valida el payload con Pydantic (tipos estrictos y rangos) y escribe en InfluxDB de forma automática.
- Modelo: measurement `environment`; tags `device_id, source, status` (baja cardinalidad); fields `temperature, humidity, light_raw, motion, status_code, sequence, uptime_ms`.
- Pruebas automatizadas de la validación en `services/mqtt-ingestor/tests`.
- Evidencia: _captura de Data Explorer de InfluxDB_.

### 3.5 Visualización (Parte 4)
- Dashboard de Grafana con 15 paneles (temperatura actual/máx/mín/promedio, humedad, iluminación, movimiento, estado, conexión, última medición, series de tiempo), variable de dispositivo y refresco de 5 s. Provisionado por archivos (datasource + dashboard JSON versionado).
- Personalizado con el nombre del grupo en el título.
- Evidencia: _captura del dashboard_.

## 4. Decisiones técnicas
1. **Puente por broker público.** Wokwi gratuito no puede alcanzar `localhost`; el ESP32 publica en `broker.emqx.io` con un prefijo aleatorio y `mqtt-bridge` lo reenvía al EMQX local, desde donde continúa el flujo pedido. Limitación: los datos pasan por un broker público sin autenticación (solo datos de demostración). Con la licencia de Private IoT Gateway se usaría `host.wokwi.internal`.
2. **InfluxDB** frente a DynamoDB: ejecución local, sin costo y buena integración con Grafana.
3. **Hora de recepción** como timestamp: el ESP32 no tiene hora absoluta.
4. **Validación estricta** en el ingestor para no almacenar datos corruptos.
5. **Docker Compose con healthchecks y versiones fijas** para reproducibilidad; puertos solo en localhost y credenciales en `.env` (fuera de Git).

## 5. Seguridad y limitaciones
MQTT sin autenticación ni TLS en la demo; en producción usar usuarios/ACL de EMQX y TLS. El tópico público depende de un prefijo secreto, no de autenticación.

## 6. Pruebas
`docker compose config -q`, tests del ingestor (`pytest`), regeneración del dashboard y verificación de extremo a extremo con `mosquitto_pub` (ver README).

## 7. Trabajo colaborativo
_Describir reparto de tareas, reuniones y aportes de cada integrante; enlazar commits/PR del repositorio._

## 8. Conclusiones
_Aprendizajes, dificultades (p. ej. conexión de Wokwi con el broker local) y mejoras futuras._

## 9. Anexo: evidencias
Ver `docs/evidencias/README.md`.
