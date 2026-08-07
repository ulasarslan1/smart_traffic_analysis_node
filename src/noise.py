from __future__ import annotations

import numpy as np
from numpy.typing import NDArray


def add_complex_awgn(
    signal: NDArray[np.complex128],
    snr_db: float,
    rng: np.random.Generator | None = None,
) -> NDArray[np.complex128]:
    """Add complex white Gaussian noise to a signal."""
    if signal.size == 0:
        raise ValueError("Signal cannot be empty.")

    if rng is None:
        rng = np.random.default_rng()

    signal_power = float(np.mean(np.abs(signal) ** 2))
    snr_linear = 10.0 ** (snr_db / 10.0)
    noise_power = signal_power / snr_linear
    noise_std = np.sqrt(noise_power / 2.0)

    noise = noise_std * (
        rng.normal(size=signal.shape)
        + 1j * rng.normal(size=signal.shape)
    )

    return signal + noise


class AddNoise:
    """Backwards-compatible class interface for noise helpers."""

    add_complex_awgn = staticmethod(add_complex_awgn)
