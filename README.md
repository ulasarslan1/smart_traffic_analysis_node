# SENTRY — Smart Traffic Analysis Node

SENTRY is an ESP32-S3-based smart traffic monitoring system simulated in Wokwi.

The system detects simulated vehicles with two ultrasonic sensors, estimates their speed and direction, classifies them as motorcycle, car, or truck using a lightweight Machine Learning model, calculates a traffic risk score, displays local warnings, and sends important traffic events through MQTT.

## System Overview

```text
Vehicle Simulation
        │
        ▼
 Ultrasonic Sensors (A / B)
        │
        ▼
     ESP32-S3
        │
        ├── Speed Detection
        ├── Direction Detection
        ├── Feature Extraction
        ├── ML Vehicle Classification
        └── Risk Analysis
             │
        ┌────┴────┐
        ▼         ▼
     Display     MQTT
  (OLED / LED /     │
     Buzzer)        ▼
               Central System
```

## Circuit

The circuit is defined in `diagram.json`.

| Component | ESP32-S3 pin | Role |
| --- | --- | --- |
| Ultrasonic sensor A (HC-SR04) | TRIG GPIO 6, ECHO GPIO 7 | First detection gate |
| Ultrasonic sensor B (HC-SR04) | TRIG GPIO 5, ECHO GPIO 18 | Second detection gate |
| Potentiometer | GPIO 4 (ADC) | Adjustable test value (planned: measurement noise level) |
| Traffic switch | GPIO 17 | Selects the traffic condition |
| SSD1306 OLED (I2C) | GPIO 8 (SDA), GPIO 9 (SCL) | Local display |
| RGB LED | GPIO 12 / 13 / 14 | Risk level indicator |
| Buzzer | GPIO 15 | Audible alert |

## Speed & Direction

The distance between sensor A and sensor B is known, so speed is computed from the time difference between the two detections:

```text
speed = distance / time
```

The order of activation gives the direction:

```text
A → B = NORMAL
B → A = WRONG WAY
```

Invalid measurements (timeout, duplicate trigger, incomplete detection, impossible sequence) are rejected.

## Vehicle Classification

Vehicle classification is performed using a Machine Learning model.

The model predicts:

```text
Motorcycle
Car
Truck
```

Possible input features include:

* Vehicle speed
* Estimated vehicle length
* Sensor occupancy time
* Sensor A / B detection duration
* Variation of the measured distance

The class is never hard-coded and never given to the firmware. If the model's confidence is too low, the vehicle is reported as `UNKNOWN`.

The model is trained offline using Python and then exported for inference on the ESP32-S3.

```text
Dataset
   │
   ▼
Python ML Training
   │
   ▼
Trained Model
   │
   ▼
ESP32-S3 Inference
```

## Risk Analysis

A deterministic risk engine combines speed, speed limit, vehicle type, classification confidence, direction, and traffic condition into a score from 0 to 100.

| Risk Score | Level |
| --- | --- |
| 0–30 | LOW |
| 31–60 | MODERATE |
| 61–80 | HIGH |
| 81–100 | CRITICAL |

High-risk and wrong-way events trigger a local alert (RGB LED, buzzer, OLED) and an MQTT message.

## Technologies

* ESP32-S3
* Wokwi
* C++ / PlatformIO
* Python
* Machine Learning / TinyML
* MQTT
* Wi-Fi

## Repository Structure

```text
SENTRY/
│
├── src/
│   └── main.cpp
│
├── ml/
│   ├── generate_dataset.py
│   ├── train.py
│   └── dataset.csv
│
├── model/
│   └── vehicle_model.h
│
├── diagram.json
├── wokwi.toml
├── platformio.ini
├── .gitignore
└── README.md
```

`diagram.json` and `wokwi.toml` already exist. The firmware, the ML files, and `platformio.ini` are still to be created.

## Development Flow

```text
Vehicle Detection
       ↓
Speed & Direction
       ↓
Feature Extraction
       ↓
ML Classification
       ↓
Risk Analysis
       ↓
Display / MQTT
```

## Notes

* The training data is synthetic and includes measurement noise. Reported accuracy describes the simulation, not real traffic.
* Ultrasonic sensors have a short range and low measurement rate, so here they model a measurement gate in a scaled simulation.
* Everything runs in Wokwi. On real hardware, the HC-SR04 ECHO pin outputs 5 V and needs a voltage divider for the ESP32-S3.

## Project Status

```text
Circuit (diagram.json)   ✔
Firmware                 ☐
Dataset + ML training    ☐
Embedded inference       ☐
Risk analysis            ☐
MQTT                     ☐
```

## Project Goal

The goal of SENTRY is to demonstrate how embedded systems, Machine Learning, Edge AI, and IoT communication can be combined to build an intelligent roadside traffic monitoring node without requiring physical hardware.
