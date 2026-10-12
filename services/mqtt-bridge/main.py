"""Puente MQTT: broker publico (donde publica el ESP32 de Wokwi) -> EMQX local.

Suscribe a <PREFIX>/# en el broker remoto, quita el prefijo y republica en EMQX local,
de modo que el resto del flujo (ingestor, InfluxDB, Grafana) no depende del broker publico.
"""
import logging
import os
import signal
import sys
import threading

import paho.mqtt.client as mqtt

logging.basicConfig(level=os.getenv("LOG_LEVEL", "INFO"), format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger("bridge")


def env(name: str, default: str | None = None) -> str:
    value = os.getenv(name, default)
    if value is None:
        log.error("Falta la variable de entorno %s", name)
        sys.exit(1)
    return value


REMOTE_HOST = env("REMOTE_MQTT_HOST", "broker.emqx.io")
REMOTE_PORT = int(env("REMOTE_MQTT_PORT", "1883"))
PREFIX = env("BRIDGE_TOPIC_PREFIX").strip("/")
LOCAL_HOST = env("MQTT_HOST", "emqx")
LOCAL_PORT = int(env("MQTT_PORT", "1883"))
stop = threading.Event()


def make_client(name: str) -> mqtt.Client:
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"{name}-{os.getpid()}")
    c.reconnect_delay_set(min_delay=1, max_delay=30)
    return c


local = make_client("bridge-local")
remote = make_client("bridge-remote")
if env("MQTT_USER", ""):
    local.username_pw_set(env("MQTT_USER"), env("MQTT_PASSWORD", ""))


def on_remote_connect(client, userdata, flags, rc, props):
    if rc.is_failure:
        log.error("Broker remoto rechazo la conexion: %s", rc)
        return
    log.info("Conectado a %s:%d; suscrito a %s/#", REMOTE_HOST, REMOTE_PORT, PREFIX)
    client.subscribe(f"{PREFIX}/#", qos=0)


def on_remote_message(client, userdata, msg):
    topic = msg.topic[len(PREFIX) + 1:]
    if not topic or not local.is_connected():
        log.warning("Descartado %s (EMQX local no disponible)", msg.topic)
        return
    local.publish(topic, msg.payload, qos=0, retain=msg.retain)
    log.info("%s -> %s (%d bytes)", msg.topic, topic, len(msg.payload))


local.on_connect = lambda c, u, f, rc, p: log.info("Conectado a EMQX local %s:%d", LOCAL_HOST, LOCAL_PORT)
local.on_disconnect = lambda c, u, f, rc, p: log.warning("EMQX local desconectado (%s)", rc)
remote.on_connect = on_remote_connect
remote.on_message = on_remote_message
remote.on_disconnect = lambda c, u, f, rc, p: log.warning("Broker remoto desconectado (%s)", rc)

for sig in (signal.SIGINT, signal.SIGTERM):
    signal.signal(sig, lambda *_: stop.set())

for client, host, port in ((local, LOCAL_HOST, LOCAL_PORT), (remote, REMOTE_HOST, REMOTE_PORT)):
    while not stop.is_set():
        try:
            client.connect(host, port, keepalive=30)
            break
        except OSError as exc:
            log.warning("%s:%d no disponible (%s); reintento en 3 s", host, port, exc)
            stop.wait(3)
    client.loop_start()

stop.wait()
log.info("Apagando...")
for c in (remote, local):
    c.loop_stop()
    c.disconnect()
