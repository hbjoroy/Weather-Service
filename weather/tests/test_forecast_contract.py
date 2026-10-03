"""Validate production-serialized fixtures against the published OpenAPI schema."""
import json
from pathlib import Path
import subprocess
import sys

from jsonschema import Draft4Validator
import yaml


binary, contract = sys.argv[1:]
fixtures = json.loads(subprocess.check_output([str(Path(binary).resolve())], text=True))
spec = yaml.safe_load(Path(contract).read_text(encoding="utf-8"))
schemas = spec["components"]["schemas"]
hour_fields = schemas["ForecastHour"]["properties"]
assert hour_fields["uv"]["type"] == "number"
assert hour_fields["pressure_mb"]["type"] == "number"
assert "current" in schemas["ForecastResponse"]["properties"]

schema = {"$ref": "#/components/schemas/ForecastResponse", "components": spec["components"]}
Draft4Validator.check_schema(schema)
validator = Draft4Validator(schema)
for name, fixture in fixtures.items():
    validator.validate(fixture)
    assert set(fixture) <= {"location", "current", "forecast"}, name

hourly = fixtures["hourly"]
assert hourly["current"]["last_updated_epoch"] == 1791055800
assert hourly["current"]["last_updated"] == "2026-10-03 22:30"
assert hourly["location"]["tz_id"] == "Europe/Athens"
assert "current" not in fixtures["missing_epoch"]
assert "current" not in fixtures["missing_time"]
print("Forecast serialization and OpenAPI contract: 4 fixtures passed")
