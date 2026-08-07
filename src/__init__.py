"""Public API for the FMCW radar package.

Importing from ``src`` gives direct access to the most commonly used
configuration values, functions and classes from the modules in this package.
"""

from .config import (
    CONFIG,
    DEFAULT_CONFIG_FILE,
    PROJECT_ROOT,
    RadarConfig,
    get_config,
    reload_config,
)
from .noise import AddNoise, add_complex_awgn
from .pipeline import PipelineResult, RadarPipeline
from .processing import Process, compute_range_fft, estimate_target_range
from .signal_generator import (
    SignalGenerator,
    calculate_beat_frequency,
    generate_beat_signal,
)

__all__ = [
    "CONFIG",
    "DEFAULT_CONFIG_FILE",
    "PROJECT_ROOT",
    "RadarConfig",
    "get_config",
    "reload_config",
    "AddNoise",
    "add_complex_awgn",
    "Process",
    "compute_range_fft",
    "estimate_target_range",
    "SignalGenerator",
    "calculate_beat_frequency",
    "generate_beat_signal",
    "PipelineResult",
    "RadarPipeline",
]




