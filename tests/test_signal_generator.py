from __future__ import annotations

import numpy as np
import pytest

from src.config import RadarConfig
from src.signal_generator import (
    SignalGenerator,
    calculate_beat_frequency,
    generate_beat_signal,
)


class TestCalculateBeatFrequency:
    def test_matches_manual_formula(self, default_radar_config: RadarConfig):
        cfg = default_radar_config
        expected = (
            2.0
            * cfg.chirp_slope_hz_per_s
            * cfg.target_range_m
            / cfg.speed_of_light_m_per_s
        )
        assert calculate_beat_frequency(cfg) == pytest.approx(expected)

    def test_zero_range_gives_zero_beat_frequency(self, write_config_file):
        path = write_config_file(overrides={"target_range_m": 0.0})
        cfg = RadarConfig(path)
        assert calculate_beat_frequency(cfg) == 0.0

    def test_beat_frequency_scales_linearly_with_range(self, write_config_file):
        path_near = write_config_file(
            overrides={"target_range_m": 10.0}, filename="near.json"
        )
        path_far = write_config_file(
            overrides={"target_range_m": 20.0}, filename="far.json"
        )
        cfg_near = RadarConfig(path_near)
        cfg_far = RadarConfig(path_far)

        fb_near = calculate_beat_frequency(cfg_near)
        fb_far = calculate_beat_frequency(cfg_far)

        assert fb_far == pytest.approx(2.0 * fb_near)

    def test_uses_shared_config_when_none_given(self, monkeypatch, default_radar_config):
        import src.signal_generator as signal_generator_module

        monkeypatch.setattr(
            signal_generator_module, "get_config", lambda: default_radar_config
        )
        assert calculate_beat_frequency(None) == calculate_beat_frequency(
            default_radar_config
        )


class TestGenerateBeatSignal:
    def test_returns_expected_shapes_and_dtypes(self, default_radar_config: RadarConfig):
        time_s, signal = generate_beat_signal(default_radar_config)
        assert time_s.shape == (default_radar_config.number_of_samples,)
        assert signal.shape == (default_radar_config.number_of_samples,)
        assert time_s.dtype == np.float64
        assert signal.dtype == np.complex128

    def test_time_axis_starts_at_zero_and_has_correct_step(
        self, default_radar_config: RadarConfig
    ):
        time_s, _ = generate_beat_signal(default_radar_config)
        assert time_s[0] == 0.0
        expected_step = 1.0 / default_radar_config.sampling_frequency_hz
        actual_steps = np.diff(time_s)
        np.testing.assert_allclose(actual_steps, expected_step)

    def test_signal_is_unit_magnitude_complex_exponential(
        self, default_radar_config: RadarConfig
    ):
        _, signal = generate_beat_signal(default_radar_config)
        magnitudes = np.abs(signal)
        np.testing.assert_allclose(magnitudes, 1.0, atol=1e-10)

    def test_signal_instantaneous_phase_matches_beat_frequency(
        self, default_radar_config: RadarConfig
    ):
        time_s, signal = generate_beat_signal(default_radar_config)
        beat_frequency_hz = calculate_beat_frequency(default_radar_config)
        expected = np.exp(1j * 2.0 * np.pi * beat_frequency_hz * time_s)
        np.testing.assert_allclose(signal, expected, atol=1e-10)

    def test_raises_when_beat_frequency_exceeds_nyquist(self, write_config_file):
        # With the default bandwidth/duration/sampling, ranges above ~37.5 m
        # push the beat frequency past the Nyquist frequency.
        path = write_config_file(overrides={"target_range_m": 1000.0})
        cfg = RadarConfig(path)
        with pytest.raises(ValueError, match="Nyquist"):
            generate_beat_signal(cfg)

    def test_uses_shared_config_when_none_given(self, monkeypatch, default_radar_config):
        import src.signal_generator as signal_generator_module

        monkeypatch.setattr(
            signal_generator_module, "get_config", lambda: default_radar_config
        )
        time_s, signal = generate_beat_signal(None)
        assert time_s.shape == (default_radar_config.number_of_samples,)
        assert signal.shape == (default_radar_config.number_of_samples,)


class TestSignalGeneratorBackwardsCompatibleClass:
    def test_calculate_beat_frequency_matches_function(
        self, default_radar_config: RadarConfig
    ):
        assert SignalGenerator.calculate_beat_frequency(
            default_radar_config
        ) == calculate_beat_frequency(default_radar_config)

    def test_generate_beat_signal_matches_function(
        self, default_radar_config: RadarConfig
    ):
        time_a, signal_a = SignalGenerator.generate_beat_signal(default_radar_config)
        time_b, signal_b = generate_beat_signal(default_radar_config)
        np.testing.assert_array_equal(time_a, time_b)
        np.testing.assert_array_equal(signal_a, signal_b)
