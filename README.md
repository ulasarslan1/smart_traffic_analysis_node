# SENTRY — Smart Edge Node for Traffic Risk

SENTRY is an ESP32-S3-based traffic monitoring prototype simulated in Wokwi. The project uses **one HC-SR04 ultrasonic sensor** to detect passage through a measurement zone. A finite state machine (FSM) filters short-lived readings, confirms vehicle entry and exit, and counts completed passages.

The current implementation focuses on **reliable single-sensor detection and serial logging**. TinyML vehicle classification, traffic-risk analysis, OLED/LED/buzzer feedback, and MQTT integration are planned development stages—not features implemented by the FSM shown here.

> **Scope:** With one distance sensor, SENTRY cannot reliably measure road speed or travel direction. Wrong-way detection and the original two-sensor speed calculation are no longer part of this design. A completed detection cycle is not necessarily proof of one distinct physical vehicle: closely spaced vehicles without a measurable gap may be merged, and brief or noisy detections may be missed.

```

## Hardware and Circuit

The Wokwi circuit is defined in `diagram.json`. The single-sensor version uses the following connections:

| Component | ESP32-S3 connection | Purpose |
| --- | --- | --- |
| HC-SR04 ultrasonic sensor | TRIG GPIO 5; ECHO GPIO 7 | Measure distance to the detection zone |
| Potentiometer | SIG GPIO 4 (ADC) | Adjustable simulation input; future use to be defined |
| SSD1306 OLED (I²C) | SDA GPIO 8; SCL GPIO 9 | Planned local display |
| RGB LED (common cathode) | R GPIO 12; G GPIO 13; B GPIO 14, each through 220 Ω | Planned visual indication |
| Buzzer | GPIO 15 | Planned audible alert |

The HC-SR04 is powered from 5 V with a common ground. **For physical hardware**, its 5 V ECHO signal must be level-shifted or divided to a safe 3.3 V level before connecting to the ESP32-S3. Direct ECHO wiring in Wokwi is simulation-specific.

```

## Planned TinyML Vehicle Classification

The next stage is to collect features from each confirmed passage. Candidate single-sensor features include:

- Time spent in the confirmed detection zone / passage duration
- Minimum and mean measured distance during a passage
- Distance variation and sampled distance profile
- Number of valid measurements and signal-quality indicators

An offline Python training workflow could explore classification into `MOTORCYCLE`, `CAR`, `TRUCK`, and `UNKNOWN` for low-confidence results. **These labels are research targets, not validated capabilities.** One ultrasonic sensor cannot reliably recover vehicle length or speed, and vehicle classes may overlap substantially in these features. Evaluate any model against held-out simulated scenarios and, before real-world claims, representative physical measurements. Do not hard-code the simulated class as a firmware input.

## Planned Traffic Analysis and Alerts

The earlier two-sensor risk formula based on speed and wrong-way direction is **not applicable** to this version. A future single-sensor analysis may use completed passage count per time window, occupancy fraction, and unusually long occupancy as *traffic indicators*. Such indicators are not a validated measure of collision risk by themselves.

After defining and validating an appropriate risk model, the project can show status on the SSD1306 OLED, RGB LED, and buzzer and publish structured events over Wi-Fi/MQTT. Risk thresholds, alert policy, broker details, and payload schema are **to be defined**; no numerical risk score is claimed as implemented.

## Technologies

- ESP32-S3; Arduino/C++ and PlatformIO
- Wokwi simulation
- HC-SR04 ultrasonic sensing
- Finite state machine and threshold hysteresis
- Planned: Python, TinyML, Wi-Fi, MQTT, SSD1306 OLED, RGB LED, buzzer

## Repository Layout

```text
SENTRY/
├── src/
│   └── main.cpp            # Current single-sensor FSM firmware
├── diagram.json            # Wokwi circuit (one HC-SR04)
├── wokwi.toml              # Wokwi configuration, if present
├── platformio.ini          # PlatformIO configuration, if present
├── ml/                     # Planned dataset and training scripts
├── model/                  # Planned exported TinyML model
└── README.md
```

The layout above distinguishes current core files from planned or configuration-dependent files; it is not a claim that every listed file already exists.

## Development Roadmap

1. **Current:** Single HC-SR04 distance measurement, five-state FSM, three-sample confirmation, serial events, and completed-passage counter.
2. **Next:** Timestamp entry/exit, collect per-passage distance features, and handle extended invalid readings.
3. **Research:** Generate and evaluate realistic datasets for single-sensor TinyML classification; quantify ambiguity and missed detections.
4. **Later:** Define occupancy-based traffic indicators and integrate OLED, LED, buzzer, and MQTT.

## Limitations

- The sensor observes presence at one point, **not** vehicle speed or direction.
- A short passage can be missed when it does not produce three qualifying readings at the 300 ms sampling interval.
- A vehicle already present at startup is not counted as a newly entered vehicle.
- Invalid readings during `ENTRY` or `EXIT` reset the candidate; extended invalid readings during `OCCUPIED` currently have no dedicated fault timeout.
- Nearby or back-to-back vehicles may not be distinguishable without a confirmed empty interval.
- Simulated measurements and future synthetic training accuracy must not be presented as field-validated traffic performance.

## Project Goal

Demonstrate an incremental edge-IoT traffic sensing workflow: establish robust, observable single-sensor passage detection first, then evaluate whether its measured features support useful on-device classification and traffic indicators before adding local alerts and MQTT reporting.
