from __future__ import annotations

import numpy as np
import pytest

from src.config import RadarConfig
from src.processing import Process, compute_range_fft, estimate_target_range
from src.signal_generator import generate_beat_signal


class TestComputeRangeFft:
    def test_raises_when_signal_too_short(self):
        signal = np.array([1.0 + 0j], dtype=np.complex128)
        with pytest.raises(ValueError, match="at least two samples"):
            compute_range_fft(
                signal,
                sampling_frequency_hz=1000.0,
                chirp_slope_hz_per_s=1.0,
                speed_of_light_m_per_s=3e8,
            )

    def test_raises_when_sampling_frequency_not_positive(self):
        signal = np.ones(10, dtype=np.complex128)
        with pytest.raises(ValueError, match="Sampling frequency"):
            compute_range_fft(
                signal,
                sampling_frequency_hz=0.0,
                chirp_slope_hz_per_s=1.0,
                speed_of_light_m_per_s=3e8,
            )

    def test_raises_when_chirp_slope_not_positive(self):
        signal = np.ones(10, dtype=np.complex128)
        with pytest.raises(ValueError, match="Chirp slope"):
            compute_range_fft(
                signal,
                sampling_frequency_hz=1000.0,
                chirp_slope_hz_per_s=0.0,
                speed_of_light_m_per_s=3e8,
            )

    def test_raises_when_explicit_speed_of_light_not_positive(self):
        signal = np.ones(10, dtype=np.complex128)
        with pytest.raises(ValueError, match="Speed of light"):
            compute_range_fft(
                signal,
                sampling_frequency_hz=1000.0,
                chirp_slope_hz_per_s=1.0,
                speed_of_light_m_per_s=-1.0,
            )

    def test_defaults_speed_of_light_from_shared_config(self, monkeypatch, default_radar_config):
        import src.processing as processing_module

        monkeypatch.setattr(processing_module, "get_config", lambda: default_radar_config)

        signal = np.ones(10, dtype=np.complex128)
        range_axis_default, _ = compute_range_fft(
            signal, sampling_frequency_hz=1000.0, chirp_slope_hz_per_s=1.0
        )
        range_axis_explicit, _ = compute_range_fft(
            signal,
            sampling_frequency_hz=1000.0,
            chirp_slope_hz_per_s=1.0,
            speed_of_light_m_per_s=default_radar_config.speed_of_light_m_per_s,
        )
        np.testing.assert_array_equal(range_axis_default, range_axis_explicit)

    def test_output_lengths_match_positive_frequency_bins(
        self, default_radar_config: RadarConfig
    ):
        _, signal = generate_beat_signal(default_radar_config)
        range_axis_m, magnitude = compute_range_fft(
            signal=signal,
            sampling_frequency_hz=default_radar_config.sampling_frequency_hz,
            chirp_slope_hz_per_s=default_radar_config.chirp_slope_hz_per_s,
            speed_of_light_m_per_s=default_radar_config.speed_of_light_m_per_s,
        )
        frequency_axis_hz = np.fft.fftfreq(
            signal.size, d=1.0 / default_radar_config.sampling_frequency_hz
        )
        expected_bins = int(np.sum(frequency_axis_hz >= 0))
        assert range_axis_m.shape == (expected_bins,)
        assert magnitude.shape == (expected_bins,)

    def test_range_axis_starts_at_zero_and_is_nondecreasing(
        self, default_radar_config: RadarConfig
    ):
        _, signal = generate_beat_signal(default_radar_config)
        range_axis_m, _ = compute_range_fft(
            signal=signal,
            sampling_frequency_hz=default_radar_config.sampling_frequency_hz,
            chirp_slope_hz_per_s=default_radar_config.chirp_slope_hz_per_s,
            speed_of_light_m_per_s=default_radar_config.speed_of_light_m_per_s,
        )
        assert range_axis_m[0] == pytest.approx(0.0)
        assert np.all(np.diff(range_axis_m) > 0)

    def test_magnitude_is_nonnegative(self, default_radar_config: RadarConfig):
        _, signal = generate_beat_signal(default_radar_config)
        _, magnitude = compute_range_fft(
            signal=signal,
            sampling_frequency_hz=default_radar_config.sampling_frequency_hz,
            chirp_slope_hz_per_s=default_radar_config.chirp_slope_hz_per_s,
            speed_of_light_m_per_s=default_radar_config.speed_of_light_m_per_s,
        )
        assert np.all(magnitude >= 0.0)

    def test_peak_bin_recovers_target_range_for_clean_signal(
        self, default_radar_config: RadarConfig
    ):
        cfg = default_radar_config
        _, signal = generate_beat_signal(cfg)

        range_axis_m, magnitude = compute_range_fft(
            signal=signal,
            sampling_frequency_hz=cfg.sampling_frequency_hz,
            chirp_slope_hz_per_s=cfg.chirp_slope_hz_per_s,
            speed_of_light_m_per_s=cfg.speed_of_light_m_per_s,
        )

        estimated_range_m = estimate_target_range(range_axis_m, magnitude)
        range_resolution_m = cfg.speed_of_light_m_per_s / (2.0 * cfg.bandwidth_hz)

        assert estimated_range_m == pytest.approx(
            cfg.target_range_m, abs=range_resolution_m
        )


class TestEstimateTargetRange:
    def test_raises_on_shape_mismatch(self):
        range_axis_m = np.array([0.0, 1.0, 2.0])
        magnitude = np.array([0.0, 1.0])
        with pytest.raises(ValueError, match="shapes must match"):
            estimate_target_range(range_axis_m, magnitude)

    def test_raises_when_fewer_than_two_bins(self):
        range_axis_m = np.array([0.0])
        magnitude = np.array([1.0])
        with pytest.raises(ValueError, match="at least two bins"):
            estimate_target_range(range_axis_m, magnitude)

    def test_ignores_dc_bin_when_finding_peak(self):
        range_axis_m = np.array([0.0, 1.0, 2.0, 3.0])
        # DC bin (index 0) has the largest magnitude, but must be ignored.
        magnitude = np.array([100.0, 1.0, 5.0, 2.0])
        estimated = estimate_target_range(range_axis_m, magnitude)
        assert estimated == 2.0

    def test_picks_correct_peak_among_non_dc_bins(self):
        range_axis_m = np.array([0.0, 10.0, 20.0, 30.0, 40.0])
        magnitude = np.array([0.0, 0.5, 0.1, 9.9, 3.0])
        estimated = estimate_target_range(range_axis_m, magnitude)
        assert estimated == 30.0

    def test_returns_python_float(self):
        range_axis_m = np.array([0.0, 1.0, 2.0])
        magnitude = np.array([0.0, 1.0, 0.5])
        estimated = estimate_target_range(range_axis_m, magnitude)
        assert isinstance(estimated, float)


class TestProcessBackwardsCompatibleClass:
    def test_compute_range_fft_matches_function(self, default_radar_config: RadarConfig):
        _, signal = generate_beat_signal(default_radar_config)
        kwargs = dict(
            signal=signal,
            sampling_frequency_hz=default_radar_config.sampling_frequency_hz,
            chirp_slope_hz_per_s=default_radar_config.chirp_slope_hz_per_s,
            speed_of_light_m_per_s=default_radar_config.speed_of_light_m_per_s,
        )
        range_a, mag_a = Process.compute_range_fft(**kwargs)
        range_b, mag_b = compute_range_fft(**kwargs)
        np.testing.assert_array_equal(range_a, range_b)
        np.testing.assert_array_equal(mag_a, mag_b)

    def test_estimate_target_range_matches_function(self):
        range_axis_m = np.array([0.0, 1.0, 2.0])
        magnitude = np.array([0.0, 1.0, 0.5])
        assert Process.estimate_target_range(
            range_axis_m, magnitude
        ) == estimate_target_range(range_axis_m, magnitude)
