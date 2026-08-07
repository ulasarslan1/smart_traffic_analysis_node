from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from numpy.typing import NDArray

from .config import RadarConfig, get_config
from .noise import add_complex_awgn
from .processing import compute_range_fft, estimate_target_range
from .signal_generator import generate_beat_signal


@dataclass(frozen=True)
class PipelineResult:
    time_s: NDArray[np.float64]
    clean_signal: NDArray[np.complex128]
    noisy_signal: NDArray[np.complex128]
    range_axis_m: NDArray[np.float64]
    magnitude: NDArray[np.float64]
    estimated_range_m: float


class RadarPipeline:
    def __init__(
        self,
        radar_config: RadarConfig | None = None,
        random_seed: int | None = 42,
    ) -> None:
        # Reuse the JSON-backed configuration already held in memory.
        self.radar_config = radar_config or get_config()
        self.rng = np.random.default_rng(random_seed)

    def run(self) -> PipelineResult:
        """Run the complete single-target radar pipeline."""
        time_s, clean_signal = generate_beat_signal(self.radar_config)

        noisy_signal = add_complex_awgn(
            signal=clean_signal,
            snr_db=self.radar_config.snr_db,
            rng=self.rng,
        )

        range_axis_m, magnitude = compute_range_fft(
            signal=noisy_signal,
            sampling_frequency_hz=self.radar_config.sampling_frequency_hz,
            chirp_slope_hz_per_s=self.radar_config.chirp_slope_hz_per_s,
            speed_of_light_m_per_s=self.radar_config.speed_of_light_m_per_s,
        )

        estimated_range_m = estimate_target_range(
            range_axis_m,
            magnitude,
        )

        return PipelineResult(
            time_s=time_s,
            clean_signal=clean_signal,
            noisy_signal=noisy_signal,
            range_axis_m=range_axis_m,
            magnitude=magnitude,
            estimated_range_m=estimated_range_m,
        )
