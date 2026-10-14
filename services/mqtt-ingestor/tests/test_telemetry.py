import os
import sys
from pathlib import Path

import pytest

os.environ.setdefault("INFLUXDB_URL", "http://localhost:8086")
os.environ.setdefault("INFLUXDB_TOKEN", "t")
os.environ.setdefault("INFLUXDB_ORG", "o")
os.environ.setdefault("INFLUXDB_BUCKET", "b")
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from pydantic import ValidationError  # noqa: E402

import main  # noqa: E402

VALID = (
    '{"device_id":"ESP32-001","temperature":26.1,"humidity":55.0,"light_raw":1200,'
    '"motion":false,"status":"NORMAL","source":"synthetic","sequence":1,"uptime_ms":5000}'
)


def test_valid_payload_to_line_protocol():
    line = main.to_point(main.Telemetry.model_validate_json(VALID)).to_line_protocol()
    assert line.startswith("environment,device_id=ESP32-001,source=synthetic,status=NORMAL ")
    assert "temperature=26.1" in line and "light_raw=1200i" in line and "status_code=0i" in line


@pytest.mark.parametrize("old,new", [
    ('false', '"false"'),        # motion como string
    ('1200', '1200.5'),          # light_raw decimal
    ('26.1', '99'),              # temperatura fuera de rango
    ('"NORMAL"', '"OTRO"'),      # estado desconocido
    ('"ESP32-001"', '"a b"'),    # device_id invalido
])
def test_invalid_payloads_rejected(old, new):
    with pytest.raises(ValidationError):
        main.Telemetry.model_validate_json(VALID.replace(old, new))
