from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import pytest

from src.config import RadarConfig


DEFAULT_RADAR_PARAMS: dict[str, Any] = {
    "bandwidth_hz": 500_000_000.0,
    "chirp_duration_s": 0.00005,
    "sampling_frequency_hz": 5_000_000.0,
    "target_range_m": 15.0,
    "snr_db": 20.0,
    "speed_of_light_m_per_s": 299_792_458.0,
}


def _write_radar_json(path: Path, radar_params: dict[str, Any] | None) -> Path:
    """Write a config JSON file. ``radar_params=None`` omits the 'radar' key."""
    if radar_params is None:
        payload: dict[str, Any] = {}
    else:
        payload = {"radar": radar_params}

    path.write_text(json.dumps(payload), encoding="utf-8")
    return path


@pytest.fixture
def write_config_file(tmp_path: Path):
    """Factory fixture: returns a function that writes a config JSON file
    (with optional parameter overrides / omissions) and returns its path.
    """

    def _factory(
        overrides: dict[str, Any] | None = None,
        omit_keys: list[str] | None = None,
        filename: str = "config.json",
        radar_params: dict[str, Any] | None | object = "__default__",
    ) -> Path:
        if radar_params == "__default__":
            params = dict(DEFAULT_RADAR_PARAMS)
            if overrides:
                params.update(overrides)
            if omit_keys:
                for key in omit_keys:
                    params.pop(key, None)
        else:
            params = radar_params  # type: ignore[assignment]

        return _write_radar_json(tmp_path / filename, params)

    return _factory


@pytest.fixture
def default_radar_config(write_config_file) -> RadarConfig:
    """A RadarConfig built from a temp file with the well-known default values."""
    path = write_config_file()
    return RadarConfig(path)
