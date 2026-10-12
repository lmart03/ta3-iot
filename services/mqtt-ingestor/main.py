"""Suscriptor MQTT -> InfluxDB para Smart Environment IoT."""
import logging
import os
import pathlib
import signal
import sys
import threading
import time
from typing import Literal

import paho.mqtt.client as mqtt
from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS
from pydantic import BaseModel, Field, ValidationError, field_validator

logging.basicConfig(
    level=os.getenv("LOG_LEVEL", "INFO"),
    format="%(asctime)s %(levelname)s %(message)s",
)
log = logging.getLogger("ingestor")

HEALTH_FILE = pathlib.Path("/tmp/healthy")
STATUS_CODE = {"NORMAL": 0, "WARNING": 1, "CRITICAL": 2}


def env(name: str, default: str | None = None) -> str:
    value = os.getenv(name, default)
    if value is None:
        log.error("Falta la variable de entorno %s", name)
        sys.exit(1)
    return value


class Telemetry(BaseModel):
    device_id: str = Field(pattern=r"^[A-Za-z0-9_-]{1,32}$")
    temperature: float = Field(ge=-40, le=80)
    humidity: float = Field(ge=0, le=100)
    light_raw: int = Field(ge=0, le=4095)
    motion: bool
    status: Literal["NORMAL", "WARNING", "CRITICAL"]
    source: Literal["synthetic", "sensor"]
    sequence: int = Field(ge=0)
    uptime_ms: int = Field(ge=0)

    @field_validator("motion", "light_raw", "sequence", "uptime_ms", mode="before")
    @classmethod
    def strict_types(cls, v, info):
        # evita que "true"/1.5 se coaccionen silenciosamente
        if info.field_name == "motion" and not isinstance(v, bool):
            raise ValueError("motion debe ser booleano")
        if info.field_name != "motion" and (isinstance(v, bool) or not isinstance(v, int)):
            raise ValueError(f"{info.field_name} debe ser entero")
        return v


def to_point(t: Telemetry) -> Point:
    """Modelo: measurement 'environment'; tags de baja cardinalidad; medidas como fields.
    Se usa la hora de recepcion (uptime_ms no es una fecha absoluta)."""
    return (
        Point("environment")
        .tag("device_id", t.device_id)
        .tag("source", t.source)
        .tag("status", t.status)
        .field("temperature", t.temperature)
        .field("humidity", t.humidity)
        .field("light_raw", t.light_raw)
        .field("motion", t.motion)
        .field("status_code", STATUS_CODE[t.status])
        .field("sequence", t.sequence)
        .field("uptime_ms", t.uptime_ms)
        .time(time.time_ns(), WritePrecision.NS)
    )


class Ingestor:
    def __init__(self) -> None:
        self.stop = threading.Event()
        self.topic = env("MQTT_TOPIC", "smart-environment/+/telemetry")
        self.influx = InfluxDBClient(
            url=env("INFLUXDB_URL"), token=env("INFLUXDB_TOKEN"), org=env("INFLUXDB_ORG")
        )
        self.write_api = self.influx.write_api(write_options=SYNCHRONOUS)
        self.bucket = env("INFLUXDB_BUCKET")
        self.org = env("INFLUXDB_ORG")

        self.client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2, client_id="mqtt-ingestor"
        )
        user = os.getenv("MQTT_USER", "")
        if user:
            self.client.username_pw_set(user, os.getenv("MQTT_PASSWORD", ""))
        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect
        self.client.on_message = self.on_message
        self.client.reconnect_delay_set(min_delay=1, max_delay=30)

    def on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            log.error("MQTT rechazo la conexion: %s", reason_code)
            return
        log.info("Conectado a MQTT; suscrito a %s", self.topic)
        client.subscribe(self.topic, qos=0)

    def on_disconnect(self, client, userdata, flags, reason_code, properties):
        log.warning("Desconectado de MQTT (%s); reintentando", reason_code)

    def on_message(self, client, userdata, msg):
        try:
            data = Telemetry.model_validate_json(msg.payload)
        except ValidationError as exc:
            log.error("Payload invalido en %s: %s | %r", msg.topic, exc.errors(include_url=False), msg.payload[:200])
            return
        try:
            self.write_api.write(bucket=self.bucket, org=self.org, record=to_point(data))
            log.info("OK %s seq=%d T=%.2f H=%.2f %s [%s]", data.device_id, data.sequence,
                     data.temperature, data.humidity, data.status, data.source)
        except Exception as exc:  # error de red/InfluxDB: no detener el servicio
            log.error("Fallo al escribir en InfluxDB: %s", exc)

    def heartbeat(self) -> None:
        while not self.stop.wait(10):
            if self.client.is_connected():
                HEALTH_FILE.touch()

    def run(self) -> None:
        HEALTH_FILE.touch()
        threading.Thread(target=self.heartbeat, daemon=True).start()
        host, port = env("MQTT_HOST"), int(env("MQTT_PORT", "1883"))
        while not self.stop.is_set():
            try:
                self.client.connect(host, port, keepalive=30)
                break
            except OSError as exc:
                log.warning("Broker %s:%d no disponible (%s); reintento en 3 s", host, port, exc)
                self.stop.wait(3)
        self.client.loop_start()  # reconexion automatica
        self.stop.wait()
        log.info("Apagando...")
        self.client.loop_stop()
        self.client.disconnect()
        self.influx.close()


def main() -> None:
    ing = Ingestor()
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *_: ing.stop.set())
    ing.run()


if __name__ == "__main__":
    main()
