from __future__ import annotations

import numpy as np
import pytest

from src.noise import AddNoise, add_complex_awgn


class TestAddComplexAwgn:
    def test_raises_on_empty_signal(self):
        empty_signal = np.array([], dtype=np.complex128)
        with pytest.raises(ValueError, match="empty"):
            add_complex_awgn(empty_signal, snr_db=10.0)

    def test_output_shape_matches_input(self):
        signal = np.exp(1j * np.linspace(0, 2 * np.pi, 128)).astype(np.complex128)
        rng = np.random.default_rng(0)
        noisy = add_complex_awgn(signal, snr_db=10.0, rng=rng)
        assert noisy.shape == signal.shape
        assert np.iscomplexobj(noisy)

    def test_is_deterministic_given_same_seeded_rng(self):
        signal = np.ones(64, dtype=np.complex128)
        noisy_a = add_complex_awgn(signal, snr_db=5.0, rng=np.random.default_rng(123))
        noisy_b = add_complex_awgn(signal, snr_db=5.0, rng=np.random.default_rng(123))
        np.testing.assert_array_equal(noisy_a, noisy_b)

    def test_different_seeds_give_different_noise(self):
        signal = np.ones(64, dtype=np.complex128)
        noisy_a = add_complex_awgn(signal, snr_db=5.0, rng=np.random.default_rng(1))
        noisy_b = add_complex_awgn(signal, snr_db=5.0, rng=np.random.default_rng(2))
        assert not np.allclose(noisy_a, noisy_b)

    def test_default_rng_is_used_when_none_provided(self):
        signal = np.ones(16, dtype=np.complex128)
        # Should not raise, and should return a valid complex result.
        noisy = add_complex_awgn(signal, snr_db=10.0, rng=None)
        assert noisy.shape == signal.shape
        assert np.all(np.isfinite(noisy))

    def test_high_snr_stays_close_to_original_signal(self):
        signal = np.ones(4096, dtype=np.complex128)
        rng = np.random.default_rng(42)
        noisy = add_complex_awgn(signal, snr_db=60.0, rng=rng)
        # At very high SNR the noise power is tiny relative to signal power.
        error_power = float(np.mean(np.abs(noisy - signal) ** 2))
        assert error_power < 1e-3

    def test_measured_noise_power_matches_requested_snr(self):
        signal = np.ones(200_000, dtype=np.complex128)
        rng = np.random.default_rng(7)
        snr_db = 10.0
        noisy = add_complex_awgn(signal, snr_db=snr_db, rng=rng)

        signal_power = float(np.mean(np.abs(signal) ** 2))
        measured_noise_power = float(np.mean(np.abs(noisy - signal) ** 2))
        expected_noise_power = signal_power / (10.0 ** (snr_db / 10.0))

        # Statistical estimate over a large array; allow generous tolerance.
        assert measured_noise_power == pytest.approx(
            expected_noise_power, rel=0.1
        )

    def test_noise_has_zero_mean_real_and_imag_parts(self):
        signal = np.zeros(200_000, dtype=np.complex128)
        rng = np.random.default_rng(99)
        noisy = add_complex_awgn(signal, snr_db=0.0, rng=rng)
        assert np.mean(noisy.real) == pytest.approx(0.0, abs=0.05)
        assert np.mean(noisy.imag) == pytest.approx(0.0, abs=0.05)

    def test_works_with_arbitrary_complex_signal(self):
        t = np.linspace(0, 1, 500)
        signal = (np.cos(2 * np.pi * 5 * t) + 1j * np.sin(2 * np.pi * 5 * t)).astype(
            np.complex128
        )
        noisy = add_complex_awgn(signal, snr_db=15.0, rng=np.random.default_rng(3))
        assert noisy.shape == signal.shape
        assert noisy.dtype == np.complex128


class TestAddNoiseBackwardsCompatibleClass:
    def test_class_staticmethod_matches_function(self):
        signal = np.ones(32, dtype=np.complex128)
        via_function = add_complex_awgn(signal, snr_db=8.0, rng=np.random.default_rng(11))
        via_class = AddNoise.add_complex_awgn(
            signal, snr_db=8.0, rng=np.random.default_rng(11)
        )
        np.testing.assert_array_equal(via_function, via_class)
