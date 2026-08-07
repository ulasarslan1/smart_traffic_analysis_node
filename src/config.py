from __future__ import annotations

import json
from pathlib import Path
from typing import Any


PROJECT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_CONFIG_FILE = PROJECT_ROOT / "configs" / "default.json"


class RadarConfig:
    """Radar configuration loaded from a JSON file and kept in memory."""

    def __init__(self, config_file: str | Path = DEFAULT_CONFIG_FILE) -> None:
        self.config_file = Path(config_file).resolve()
        self.raw_data: dict[str, Any] = {}

        self.bandwidth_hz: float
        self.chirp_duration_s: float
        self.sampling_frequency_hz: float
        self.target_range_m: float
        self.snr_db: float
        self.speed_of_light_m_per_s: float

        self.load_config()
        self.validate()

    def load_config(self) -> None:
        """Load the JSON file into this object."""
        if not self.config_file.exists():
            raise FileNotFoundError(
                f"Configuration file not found: {self.config_file}"
            )

        if not self.config_file.is_file():
            raise ValueError(
                f"Configuration path is not a file: {self.config_file}"
            )

        with self.config_file.open("r", encoding="utf-8") as file:
            raw_data: dict[str, Any] = json.load(file)

        try:
            radar = raw_data["radar"]

            self.bandwidth_hz = float(radar["bandwidth_hz"])
            self.chirp_duration_s = float(radar["chirp_duration_s"])
            self.sampling_frequency_hz = float(radar["sampling_frequency_hz"])
            self.target_range_m = float(radar["target_range_m"])
            self.snr_db = float(radar["snr_db"])
            self.speed_of_light_m_per_s = float(radar["speed_of_light_m_per_s"])
        except KeyError as error:
            raise ValueError(
                f"Missing radar parameter: {error.args[0]}"
            ) from error
        except (TypeError, ValueError) as error:
            raise ValueError(
                "Radar parameters must contain numeric values."
            ) from error

        # Keep the parsed JSON in memory as well.
        self.raw_data = raw_data

    def reload(self) -> None:
        """Reload the same JSON file into the existing in-memory object."""
        self.load_config()
        self.validate()

    def validate(self) -> None:
        """Validate radar configuration parameters."""
        if self.bandwidth_hz <= 0:
            raise ValueError("bandwidth_hz must be greater than zero.")

        if self.chirp_duration_s <= 0:
            raise ValueError("chirp_duration_s must be greater than zero.")

        if self.sampling_frequency_hz <= 0:
            raise ValueError("sampling_frequency_hz must be greater than zero.")

        if self.target_range_m < 0:
            raise ValueError("target_range_m cannot be negative.")

        if self.speed_of_light_m_per_s <= 0:
            raise ValueError("speed_of_light_m_per_s must be greater than zero.")

    @property
    def chirp_slope_hz_per_s(self) -> float:
        """Calculate the FMCW chirp slope."""
        return self.bandwidth_hz / self.chirp_duration_s

    @property
    def number_of_samples(self) -> int:
        """Calculate the number of samples in one chirp."""
        return round(self.chirp_duration_s * self.sampling_frequency_hz)


# The default JSON is read exactly once when src.config is imported.
# All modules can reuse this same object instead of reopening the JSON file.
CONFIG = RadarConfig(DEFAULT_CONFIG_FILE)


def get_config() -> RadarConfig:
    """Return the shared in-memory default configuration."""
    return CONFIG


def reload_config() -> RadarConfig:
    """Reload default.json into the same shared configuration object."""
    CONFIG.reload()
    return CONFIG
