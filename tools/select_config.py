"""Select a device ID and local credential header for the MQTT build."""

import os
import re
from json import dumps
from pathlib import Path

Import("env")

device_id = os.environ.get("SENSOR_ID", "")
if not re.fullmatch(r"[a-z][a-z0-9-]{0,22}[a-z0-9]", device_id):
    raise SystemExit(
        "Set SENSOR_ID to a unique 2-24 character lowercase ID that starts "
        "with a letter and ends with a letter or digit, for example "
        "SENSOR_ID=living-room."
    )

project_dir = Path(env.subst("$PROJECT_DIR")).resolve()
config_dir = Path(
    os.environ.get("SENSOR_CONFIG_DIR", str(project_dir / "include"))
).expanduser().resolve()
secrets_file = (config_dir / "sensor_secrets.h").resolve()

if not secrets_file.is_file():
    raise SystemExit(f"Missing local credential file: {secrets_file}")

secrets_text = secrets_file.read_text()
placeholders = (
    "YOUR_WIFI_SSID",
    "YOUR_WIFI_PASSWORD",
    "YOUR_MQTT_USERNAME",
    "YOUR_MQTT_PASSWORD",
)
if any(f'"{placeholder}"' in secrets_text for placeholder in placeholders):
    raise SystemExit(
        f"Credential file still contains template values: {secrets_file}"
    )

generated_dir = project_dir / ".pio" / "generated" / "mqtt"
generated_dir.mkdir(parents=True, exist_ok=True)
id_header = generated_dir / "device_id.h"
contents = (
    f'#pragma once\n#define SENSOR_ID "{device_id}"\n'
    f'#include {dumps(str(secrets_file))}\n'
)
if not id_header.exists() or id_header.read_text() != contents:
    id_header.write_text(contents)

env.Append(CPPPATH=[str(generated_dir)])
print(f"MQTT device ID: {device_id}; local config: {secrets_file}")
