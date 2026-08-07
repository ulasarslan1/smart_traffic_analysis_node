from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

from .config import RadarConfig, get_config


def calculate_beat_frequency(
    radar_config: RadarConfig | None = None,
) -> float:
    """Calculate the beat frequency based on radar parameters."""
    if radar_config is None:
        radar_config = get_config()

    return (
        2.0
        * radar_config.chirp_slope_hz_per_s
        * radar_config.target_range_m
        / radar_config.speed_of_light_m_per_s
    )


def generate_beat_signal(
    radar_config: RadarConfig | None = None,
) -> tuple[NDArray[np.float64], NDArray[np.complex128]]:
    """Generate an ideal complex beat signal."""
    if radar_config is None:
        radar_config = get_config()

    number_of_samples = radar_config.number_of_samples
    time_s = (
        np.arange(number_of_samples, dtype=np.float64)
        / radar_config.sampling_frequency_hz
    )

    beat_frequency_hz = calculate_beat_frequency(radar_config)
    nyquist_frequency_hz = radar_config.sampling_frequency_hz / 2.0

    if beat_frequency_hz >= nyquist_frequency_hz:
        raise ValueError("Beat frequency exceeds the Nyquist frequency.")

    beat_signal = np.exp(
        1j * 2.0 * np.pi * beat_frequency_hz * time_s
    ).astype(np.complex128)

    return time_s, beat_signal


class SignalGenerator:
    """Backwards-compatible class interface for signal helpers."""

    calculate_beat_frequency = staticmethod(calculate_beat_frequency)
    generate_beat_signal = staticmethod(generate_beat_signal)
