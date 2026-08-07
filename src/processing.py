from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

from .config import get_config


def compute_range_fft(
    signal: NDArray[np.complex128],
    sampling_frequency_hz: float,
    chirp_slope_hz_per_s: float,
    speed_of_light_m_per_s: float | None = None,
) -> tuple[NDArray[np.float64], NDArray[np.float64]]:
    """Calculate the positive-frequency range spectrum."""
    if signal.size < 2:
        raise ValueError("Signal must contain at least two samples.")

    if sampling_frequency_hz <= 0:
        raise ValueError("Sampling frequency must be positive.")

    if chirp_slope_hz_per_s <= 0:
        raise ValueError("Chirp slope must be positive.")

    if speed_of_light_m_per_s is None:
        speed_of_light_m_per_s = get_config().speed_of_light_m_per_s

    if speed_of_light_m_per_s <= 0:
        raise ValueError("Speed of light must be positive.")

    signal_without_dc = signal - np.mean(signal)
    window = np.hanning(signal.size)
    windowed_signal = signal_without_dc * window

    fft_result = np.fft.fft(windowed_signal)
    frequency_axis_hz = np.fft.fftfreq(
        signal.size,
        d=1.0 / sampling_frequency_hz,
    )

    positive_frequency_mask = frequency_axis_hz >= 0
    positive_frequencies_hz = frequency_axis_hz[positive_frequency_mask]
    magnitude = np.abs(fft_result[positive_frequency_mask])

    range_axis_m = (
        speed_of_light_m_per_s
        * positive_frequencies_hz
        / (2.0 * chirp_slope_hz_per_s)
    )

    return range_axis_m, magnitude


def estimate_target_range(
    range_axis_m: NDArray[np.float64],
    magnitude: NDArray[np.float64],
) -> float:
    """Estimate target range from the strongest FFT bin."""
    if range_axis_m.shape != magnitude.shape:
        raise ValueError("Range axis and magnitude shapes must match.")

    if magnitude.size < 2:
        raise ValueError("Spectrum must contain at least two bins.")

    peak_index = int(np.argmax(magnitude[1:]) + 1)
    return float(range_axis_m[peak_index])


class Process:
    """Backwards-compatible class interface for processing helpers."""

    compute_range_fft = staticmethod(compute_range_fft)
    estimate_target_range = staticmethod(estimate_target_range)
