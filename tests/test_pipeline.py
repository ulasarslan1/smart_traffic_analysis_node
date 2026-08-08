from __future__ import annotations

import numpy as np
import pytest

from src.config import RadarConfig, get_config
from src.pipeline import PipelineResult, RadarPipeline


class TestRadarPipelineRun:
    def test_run_returns_pipeline_result_with_expected_shapes(
        self, default_radar_config: RadarConfig
    ):
        pipeline = RadarPipeline(radar_config=default_radar_config, random_seed=1)
        result = pipeline.run()

        assert isinstance(result, PipelineResult)
        n = default_radar_config.number_of_samples
        assert result.time_s.shape == (n,)
        assert result.clean_signal.shape == (n,)
        assert result.noisy_signal.shape == (n,)
        assert result.range_axis_m.shape == result.magnitude.shape
        assert isinstance(result.estimated_range_m, float)

    def test_clean_and_noisy_signal_differ(self, default_radar_config: RadarConfig):
        pipeline = RadarPipeline(radar_config=default_radar_config, random_seed=1)
        result = pipeline.run()
        assert not np.allclose(result.clean_signal, result.noisy_signal)

    def test_estimated_range_is_close_to_configured_target(
        self, default_radar_config: RadarConfig
    ):
        pipeline = RadarPipeline(radar_config=default_radar_config, random_seed=1)
        result = pipeline.run()

        range_resolution_m = default_radar_config.speed_of_light_m_per_s / (
            2.0 * default_radar_config.bandwidth_hz
        )
        # SNR is high (20 dB by default) so the estimate should land within
        # a few range bins of the true target range.
        assert result.estimated_range_m == pytest.approx(
            default_radar_config.target_range_m, abs=5 * range_resolution_m
        )

    def test_same_seed_gives_reproducible_results(self, default_radar_config: RadarConfig):
        pipeline_a = RadarPipeline(radar_config=default_radar_config, random_seed=99)
        pipeline_b = RadarPipeline(radar_config=default_radar_config, random_seed=99)

        result_a = pipeline_a.run()
        result_b = pipeline_b.run()

        np.testing.assert_array_equal(result_a.noisy_signal, result_b.noisy_signal)
        assert result_a.estimated_range_m == result_b.estimated_range_m

    def test_different_seeds_give_different_noisy_signal(
        self, default_radar_config: RadarConfig
    ):
        pipeline_a = RadarPipeline(radar_config=default_radar_config, random_seed=1)
        pipeline_b = RadarPipeline(radar_config=default_radar_config, random_seed=2)

        result_a = pipeline_a.run()
        result_b = pipeline_b.run()

        assert not np.array_equal(result_a.noisy_signal, result_b.noisy_signal)

    def test_two_runs_of_same_pipeline_instance_differ(
        self, default_radar_config: RadarConfig
    ):
        # The internal rng advances between calls, so repeated .run() calls
        # on the same pipeline should not reproduce identical noise.
        pipeline = RadarPipeline(radar_config=default_radar_config, random_seed=1)
        result_a = pipeline.run()
        result_b = pipeline.run()
        assert not np.array_equal(result_a.noisy_signal, result_b.noisy_signal)


class TestRadarPipelineConstruction:
    def test_defaults_to_shared_config_when_none_given(self):
        pipeline = RadarPipeline(radar_config=None, random_seed=0)
        assert pipeline.radar_config is get_config()

    def test_uses_provided_config_instead_of_shared_default(
        self, default_radar_config: RadarConfig
    ):
        pipeline = RadarPipeline(radar_config=default_radar_config, random_seed=0)
        assert pipeline.radar_config is default_radar_config

    def test_none_seed_still_produces_a_valid_result(
        self, default_radar_config: RadarConfig
    ):
        pipeline = RadarPipeline(radar_config=default_radar_config, random_seed=None)
        result = pipeline.run()
        assert np.all(np.isfinite(result.noisy_signal))


class TestPipelineResultIsImmutable:
    def test_fields_cannot_be_reassigned(self, default_radar_config: RadarConfig):
        pipeline = RadarPipeline(radar_config=default_radar_config, random_seed=1)
        result = pipeline.run()
        with pytest.raises(Exception):
            result.estimated_range_m = 0.0  # type: ignore[misc]
