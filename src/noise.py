from __future__ import annotations

import numpy as np
from numpy.typing import NDArray


def add_complex_awgn(
    signal: NDArray[np.complex128],
    snr_db: float,
    rng: np.random.Generator | None = None,
) -> NDArray[np.complex128]:
    """Add complex white Gaussian noise to a signal at the requested SNR."""

    # Normalize input type.
    signal_array = np.asarray(signal, dtype=np.complex128)

    # Validate signal.
    if signal_array.size == 0:
        raise ValueError("Signal cannot be empty.")

    if not np.all(np.isfinite(signal_array)):
        raise ValueError("Signal must contain only finite values.")

    # Validate SNR.
    if not np.isfinite(snr_db):
        raise ValueError("SNR must be finite.")

    if rng is None:
        rng = np.random.default_rng()

    # Average signal power:
    # P_signal = mean(|x[n]|^2)
    signal_power = float(
        np.mean(np.abs(signal_array) ** 2)
    )

    if not np.isfinite(signal_power) or signal_power <= 0.0:
        raise ValueError(
            "Signal power must be greater than zero "
            "to define an SNR-based noise power."
        )

    # Convert SNR from dB to linear scale.
    snr_linear = 10.0 ** (snr_db / 10.0)

    if not np.isfinite(snr_linear) or snr_linear <= 0.0:
        raise ValueError(
            "SNR produces an invalid linear-scale value."
        )

    # P_noise = P_signal / SNR
    noise_power = signal_power / snr_linear

    # Complex AWGN contains independent real and imaginary parts.
    # Therefore each component gets half of the total noise power.
    noise_std = np.sqrt(noise_power / 2.0)

    noise = noise_std * (
        rng.normal(size=signal_array.shape)
        + 1j * rng.normal(size=signal_array.shape)
    )

    return signal_array + noise


class AddNoise:
    """Backwards-compatible class interface for noise helpers."""

    add_complex_awgn = staticmethod(add_complex_awgn)