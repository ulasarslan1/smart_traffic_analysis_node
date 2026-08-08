from __future__ import annotations

import pytest

from src.config import (
    CONFIG,
    DEFAULT_CONFIG_FILE,
    RadarConfig,
    get_config,
    reload_config,
)


class TestLoadConfig:
    def test_loads_expected_values(self, default_radar_config: RadarConfig):
        cfg = default_radar_config
        assert cfg.bandwidth_hz == 500_000_000.0
        assert cfg.chirp_duration_s == 0.00005
        assert cfg.sampling_frequency_hz == 5_000_000.0
        assert cfg.target_range_m == 15.0
        assert cfg.snr_db == 20.0
        assert cfg.speed_of_light_m_per_s == 299_792_458.0

    def test_values_are_cast_to_float(self, write_config_file):
        # Integers in the JSON should still end up as Python floats.
        path = write_config_file(overrides={"target_range_m": 15, "snr_db": 20})
        cfg = RadarConfig(path)
        assert isinstance(cfg.target_range_m, float)
        assert isinstance(cfg.snr_db, float)

    def test_raw_data_is_stored(self, default_radar_config: RadarConfig):
        assert "radar" in default_radar_config.raw_data
        assert default_radar_config.raw_data["radar"]["target_range_m"] == 15.0

    def test_missing_file_raises_file_not_found(self, tmp_path):
        missing_path = tmp_path / "does_not_exist.json"
        with pytest.raises(FileNotFoundError):
            RadarConfig(missing_path)

    def test_directory_instead_of_file_raises_value_error(self, tmp_path):
        directory = tmp_path / "a_directory"
        directory.mkdir()
        with pytest.raises(ValueError, match="not a file"):
            RadarConfig(directory)

    def test_missing_radar_section_raises_value_error(self, write_config_file):
        path = write_config_file(radar_params=None)
        with pytest.raises(ValueError, match="Missing radar parameter"):
            RadarConfig(path)

    @pytest.mark.parametrize(
        "missing_key",
        [
            "bandwidth_hz",
            "chirp_duration_s",
            "sampling_frequency_hz",
            "target_range_m",
            "snr_db",
            "speed_of_light_m_per_s",
        ],
    )
    def test_missing_individual_key_raises_value_error(
        self, write_config_file, missing_key
    ):
        path = write_config_file(omit_keys=[missing_key])
        with pytest.raises(ValueError, match=missing_key):
            RadarConfig(path)

    def test_non_numeric_value_raises_value_error(self, write_config_file):
        path = write_config_file(overrides={"bandwidth_hz": "not-a-number"})
        with pytest.raises(ValueError, match="numeric values"):
            RadarConfig(path)


class TestValidate:
    @pytest.mark.parametrize(
        "overrides, message",
        [
            ({"bandwidth_hz": 0.0}, "bandwidth_hz"),
            ({"bandwidth_hz": -1.0}, "bandwidth_hz"),
            ({"chirp_duration_s": 0.0}, "chirp_duration_s"),
            ({"chirp_duration_s": -1.0}, "chirp_duration_s"),
            ({"sampling_frequency_hz": 0.0}, "sampling_frequency_hz"),
            ({"sampling_frequency_hz": -1.0}, "sampling_frequency_hz"),
            ({"target_range_m": -1.0}, "target_range_m"),
            ({"speed_of_light_m_per_s": 0.0}, "speed_of_light_m_per_s"),
            ({"speed_of_light_m_per_s": -1.0}, "speed_of_light_m_per_s"),
        ],
    )
    def test_invalid_parameter_raises_value_error(
        self, write_config_file, overrides, message
    ):
        path = write_config_file(overrides=overrides)
        with pytest.raises(ValueError, match=message):
            RadarConfig(path)

    def test_target_range_zero_is_allowed(self, write_config_file):
        path = write_config_file(overrides={"target_range_m": 0.0})
        cfg = RadarConfig(path)
        assert cfg.target_range_m == 0.0


class TestDerivedProperties:
    def test_chirp_slope(self, default_radar_config: RadarConfig):
        expected = 500_000_000.0 / 0.00005
        assert default_radar_config.chirp_slope_hz_per_s == pytest.approx(expected)

    def test_number_of_samples(self, default_radar_config: RadarConfig):
        # 0.00005 s * 5_000_000 Hz = 250 samples
        assert default_radar_config.number_of_samples == 250

    def test_number_of_samples_rounds(self, write_config_file):
        # 0.00005 * 5_000_001 = 250.00005 -> rounds to 250
        path = write_config_file(overrides={"sampling_frequency_hz": 5_000_001.0})
        cfg = RadarConfig(path)
        assert cfg.number_of_samples == 250


class TestReload:
    def test_reload_picks_up_file_changes(self, tmp_path, write_config_file):
        path = write_config_file(overrides={"target_range_m": 15.0})
        cfg = RadarConfig(path)
        assert cfg.target_range_m == 15.0

        write_config_file(overrides={"target_range_m": 30.0}, filename=path.name)
        cfg.reload()

        assert cfg.target_range_m == 30.0

    def test_reload_revalidates(self, write_config_file):
        path = write_config_file(overrides={"bandwidth_hz": 500_000_000.0})
        cfg = RadarConfig(path)

        write_config_file(overrides={"bandwidth_hz": -1.0}, filename=path.name)

        with pytest.raises(ValueError, match="bandwidth_hz"):
            cfg.reload()


class TestSharedConfigSingleton:
    def test_get_config_returns_the_module_singleton(self):
        assert get_config() is CONFIG

    def test_get_config_is_idempotent(self):
        assert get_config() is get_config()

    def test_default_config_file_points_at_configs_default_json(self):
        assert DEFAULT_CONFIG_FILE.name == "default.json"
        assert DEFAULT_CONFIG_FILE.parent.name == "configs"

    def test_reload_config_returns_same_instance_with_fresh_data(self):
        reloaded = reload_config()
        assert reloaded is CONFIG
        # Reloading from the same on-disk file should not change values.
        assert reloaded.bandwidth_hz == CONFIG.bandwidth_hz