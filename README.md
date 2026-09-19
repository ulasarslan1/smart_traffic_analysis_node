# SENTRY — Smart Traffic Analysis Node

Welcome to **SENTRY**, a Wokwi-based smart roadside traffic monitoring project built around the **ESP32**.

SENTRY observes simulated vehicles, estimates their speed and travel direction, extracts sensor-based vehicle features, classifies vehicles using a lightweight **Machine Learning model running on the microcontroller**, evaluates traffic risk, displays local warnings, and optionally publishes traffic events to a central system through MQTT.

The project is designed as a small-scale **Edge AI + IoT Intelligent Transportation System**.

---

## Table of Contents

1. [The Big Picture](#the-big-picture)
2. [Your Tasks](#your-tasks)
3. [How Vehicle Analysis Works](#how-vehicle-analysis-works)
4. [ML-Based Vehicle Classification](#ml-based-vehicle-classification)
5. [Risk Engine](#risk-engine)
6. [Repository Structure](#repository-structure)
7. [Development Workflow](#development-workflow)
8. [Traffic Data Model](#traffic-data-model)
9. [MQTT Communication](#mqtt-communication)
10. [Simulation Scenarios](#simulation-scenarios)
11. [Tips & Best Practices](#tips--best-practices)

---

## The Big Picture

```text
┌────────────────────────────────────────────────────────────┐
│                   Wokwi Traffic Simulation                 │
│                                                            │
│                   Simulated Vehicle                        │
│                          │                                 │
│                          ▼                                 │
│                ┌──────────────────┐                        │
│                │ Sensor / Radar   │                        │
│                │ Simulation       │                        │
│                └────────┬─────────┘                        │
│                         │                                  │
│                         ▼                                  │
│                ┌──────────────────┐                        │
│                │ Feature          │                        │
│                │ Extraction       │                        │
│                └────────┬─────────┘                        │
│                         │                                  │
│                         ▼                                  │
│                ┌──────────────────┐                        │
│                │      ESP32       │                        │
│                │                  │                        │
│                │ Speed Analysis   │                        │
│                │ Direction        │                        │
│                │ ML Inference     │                        │
│                │ Risk Engine      │                        │
│                └──────┬─────┬─────┘                        │
│                       │     │                              │
│                       │     └──────► Wi-Fi / MQTT ────┐    │
│                       │                               │    │
│                       ▼                               ▼    │
│                Local Display                  Central      │
│                LED / OLED / TFT               System       │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

**Normal operation:** the ESP32 waits for a vehicle to enter the simulated sensing zone.

**Detection:** virtual sensors generate timing and signal-related measurements.

**Feature extraction:** measurements are converted into features that describe the detected vehicle.

**ML inference:** a lightweight machine learning model classifies the vehicle as a motorcycle, car, or truck.

**Risk analysis:** the detected vehicle and environmental context are evaluated by the local risk engine.

**Response:** SENTRY displays information locally and publishes important traffic events over MQTT.

> **Design principle:** sensing, machine learning inference, and traffic decisions should happen locally at the edge whenever possible.

---

# Your Tasks

The project is divided into five main responsibilities:

| # | Module                    | What it does                                             |
| - | ------------------------- | -------------------------------------------------------- |
| ① | Vehicle Detection         | Detect vehicles passing through the monitoring zone      |
| ② | Speed & Direction         | Calculate vehicle speed and determine travel direction   |
| ③ | ML Vehicle Classification | Extract features and classify vehicles using an ML model |
| ④ | Risk Analysis             | Calculate a contextual traffic risk score                |
| ⑤ | Warning & Communication   | Display warnings and publish important events over MQTT  |

---

## Task ① — Vehicle Detection

Vehicles are detected using two simulated sensing points.

Conceptually:

```text
Vehicle ─────► Sensor A ─────────► Sensor B
                  t1                 t2
```

The ESP32 records the timestamp of each sensor event.

Example:

```text
Sensor A triggered : 1250 ms
Sensor B triggered : 1430 ms

Δt = 180 ms
```

The detection module should reject invalid measurements such as:

* Sensor timeout
* Duplicate trigger
* Incomplete detection
* Impossible sensor sequence
* Unrealistic timing values

---

## Task ② — Speed & Direction Estimation

The distance between Sensor A and Sensor B is known.

Vehicle speed is estimated using:

```text
speed = distance / time
```

Example:

```text
Sensor distance = 4 m
Time difference = 0.18 s

Speed = 22.22 m/s
Speed = 80 km/h
```

The sensor activation order determines direction.

```text
Sensor A → Sensor B = NORMAL

Sensor B → Sensor A = WRONG WAY
```

Wrong-way driving is treated as a high-priority traffic event.

---

## Task ③ — ML Vehicle Classification

Vehicle classification is performed using a **Machine Learning model**.

The model predicts one of three classes:

```text
MOTORCYCLE
CAR
TRUCK
```

The vehicle class is **not manually selected and is not determined using hard-coded thresholds**.

Instead, SENTRY extracts features from simulated radar/sensor measurements and feeds them into a trained ML model.

Example:

```text
Raw Sensor Data
       │
       ▼
Feature Extraction
       │
       ▼
┌──────────────────────┐
│ estimated_length     │
│ occupancy_time       │
│ signal_strength      │
│ speed                │
│ sensor_response      │
└──────────┬───────────┘
           │
           ▼
      ML Classifier
           │
           ▼
 MOTORCYCLE / CAR / TRUCK
```

Example inference:

```text
[ML CLASSIFIER]

Length Estimate : 4.35 m
Occupancy Time  : 212 ms
Signal Strength : 0.64
Speed           : 67.4 km/h

Prediction      : CAR
Confidence      : 0.93
```

---

# ML-Based Vehicle Classification

The ML workflow is divided into two separate stages:

```text
TRAINING                                  INFERENCE
────────                                  ─────────

Simulation Dataset                        ESP32
      │                                     │
      ▼                                     ▼
Feature Extraction                 Sensor Measurements
      │                                     │
      ▼                                     ▼
Train ML Model                     Feature Extraction
      │                                     │
      ▼                                     ▼
Evaluate Model                     Embedded ML Model
      │                                     │
      ▼                                     ▼
Export Model                       Vehicle Prediction
      │
      └────────────► ESP32
```

Machine learning training does **not** happen on the ESP32.

The model is trained offline and the resulting model is deployed to the microcontroller.

The ESP32 only performs inference.

---

## ML Input Features

Possible input features include:

| Feature             | Description                                   |
| ------------------- | --------------------------------------------- |
| `estimated_length`  | Approximate vehicle length                    |
| `occupancy_time`    | Time spent inside the sensor detection region |
| `signal_strength`   | Simulated radar reflection strength           |
| `speed_kmh`         | Estimated vehicle speed                       |
| `sensor_a_duration` | Sensor A detection duration                   |
| `sensor_b_duration` | Sensor B detection duration                   |
| `signal_variation`  | Variation in simulated sensor response        |

The final feature set can be adjusted after dataset analysis.

Example feature vector:

```text
X = [
    4.35,
    0.212,
    0.64,
    67.4,
    0.185,
    0.192,
    0.08
]
```

Output:

```text
Y = CAR
```

---

## Dataset

Because the project runs entirely in Wokwi, the first dataset can be generated using the vehicle simulation layer.

Example:

```csv
length,occupancy,signal,speed,a_duration,b_duration,variation,class
1.9,0.11,0.31,62.4,0.09,0.10,0.04,motorcycle
4.3,0.22,0.63,67.1,0.19,0.20,0.08,car
11.2,0.71,0.91,54.7,0.66,0.69,0.13,truck
```

Multiple variations should be generated for each vehicle type.

The dataset should contain:

```text
Motorcycle samples
Car samples
Truck samples
```

with different:

* Speeds
* Vehicle lengths
* Signal strengths
* Sensor durations
* Measurement noise

Adding controlled noise prevents the ML model from simply memorizing perfectly separated simulated values.

---

## ML Training Pipeline

The initial training workflow can be implemented in Python.

```text
dataset.csv
     │
     ▼
Data Cleaning
     │
     ▼
Feature Analysis
     │
     ▼
Train / Test Split
     │
     ▼
Model Training
     │
     ▼
Model Evaluation
     │
     ▼
Embedded Model Export
     │
     ▼
ESP32 Inference
```

Candidate lightweight classifiers include:

```text
Decision Tree
Random Forest
Logistic Regression
Small Neural Network
```

The selected model should provide a good balance between:

```text
Accuracy
Memory Usage
Inference Time
Implementation Complexity
```

---

## Model Evaluation

The classifier should be evaluated before deployment.

Recommended metrics:

```text
Accuracy
Precision
Recall
F1 Score
Confusion Matrix
```

Example confusion matrix:

```text
                  Predicted
              MOTO   CAR   TRUCK

Actual MOTO    46     3      0
Actual CAR      2    48      1
Actual TRUCK    0     2     47
```

The goal is not only to obtain high accuracy but also to understand which vehicle classes are confused with each other.

---

## Confidence Score

The classifier should provide a confidence value when possible.

Example:

```text
Prediction:
CAR

Confidence:
93%
```

Low-confidence predictions can be represented as:

```text
UNKNOWN
```

Example:

```text
Prediction  : CAR
Confidence  : 0.48

Final Class : UNKNOWN
```

This prevents uncertain predictions from being treated as reliable traffic information.

---

## Edge Inference

The trained model is exported and included in the embedded application.

Conceptually:

```cpp
VehicleFeatures features = extractFeatures(sensorData);

Prediction result = classifier.predict(features);

Serial.println(result.vehicleType);
Serial.println(result.confidence);
```

The important architectural rule is:

> The sensor module provides measurements.
> The feature extractor creates ML inputs.
> The ML classifier decides the vehicle class.

The simulation layer must never directly tell the application:

```text
vehicle = CAR
```

because that would bypass the machine learning problem.

---

## Task ④ — Traffic Risk Analysis

Vehicle classification becomes one of the inputs of the risk engine.

Possible risk inputs:

```text
vehicle_speed
speed_limit
vehicle_type
classification_confidence
direction
traffic_density
weather
road_condition
time_of_day
```

Example:

```text
Vehicle       : TRUCK
ML Confidence : 94%
Speed         : 82 km/h
Speed Limit   : 70 km/h
Weather       : RAIN
Traffic       : HIGH
Direction     : NORMAL

Risk Score    : 87 / 100
Risk Level    : CRITICAL
```

Suggested risk categories:

| Risk Score | Level    |
| ---------- | -------- |
| 0–30       | LOW      |
| 31–60      | MODERATE |
| 61–80      | HIGH     |
| 81–100     | CRITICAL |

---

## Task ⑤ — Warning & Communication

After analysis, SENTRY determines the appropriate response.

| Situation         | Action                      |
| ----------------- | --------------------------- |
| Normal traffic    | Display vehicle information |
| Slight speeding   | Warning                     |
| High speeding     | High-priority warning       |
| High risk score   | Local alert + MQTT event    |
| Wrong-way vehicle | Critical alert + MQTT event |
| Low ML confidence | Mark vehicle as unknown     |
| Sensor error      | Diagnostic warning          |

Example local display:

```text
┌──────────────────────┐
│      82 km/h         │
│                      │
│       TRUCK          │
│      ML: 94%         │
│                      │
│   RISK: 87 / 100     │
└──────────────────────┘
```

---

# How Vehicle Analysis Works

A complete detection follows this pipeline:

```text
IDLE
 │
 ▼
VEHICLE_DETECTED
 │
 ▼
SENSOR_MEASUREMENT
 │
 ▼
SPEED_CALCULATION
 │
 ▼
DIRECTION_ANALYSIS
 │
 ▼
FEATURE_EXTRACTION
 │
 ▼
ML_INFERENCE
 │
 ▼
VEHICLE_CLASSIFICATION
 │
 ▼
RISK_ANALYSIS
 │
 ├──────────► DISPLAY
 │
 └──────────► MQTT
 │
 ▼
IDLE
```

The application should remain **event-driven** and non-blocking.

---

# Risk Engine

The first risk engine can remain deterministic even though vehicle classification uses ML.

Example:

```cpp
risk = 0;

risk += speedingRisk;
risk += vehicleTypeRisk;
risk += weatherRisk;
risk += trafficRisk;
risk += directionRisk;

risk = constrain(risk, 0, 100);
```

This separation is intentional.

```text
ML Model
    │
    └──► What type of vehicle is this?

Risk Engine
    │
    └──► How dangerous is this traffic situation?
```

This makes the behaviour easier to test and explain.

A future version can replace the deterministic risk engine with another learned model.

---

# Repository Structure

```text
sentry/
│
├── src/
│   ├── main.cpp
│   ├── vehicle_detector.cpp
│   ├── speed_analyzer.cpp
│   ├── feature_extractor.cpp
│   ├── ml_classifier.cpp
│   ├── risk_engine.cpp
│   ├── display_manager.cpp
│   └── mqtt_manager.cpp
│
├── include/
│   ├── vehicle_detector.h
│   ├── speed_analyzer.h
│   ├── feature_extractor.h
│   ├── ml_classifier.h
│   ├── risk_engine.h
│   ├── display_manager.h
│   └── mqtt_manager.h
│
├── model/
│   ├── vehicle_classifier.h
│   └── model_metadata.json
│
├── ml/
│   ├── dataset/
│   │   └── vehicle_dataset.csv
│   │
│   ├── notebooks/
│   │   └── train_classifier.ipynb
│   │
│   ├── train.py
│   └── evaluate.py
│
├── simulation/
│   └── scenarios/
│
├── docs/
│   └── diagrams/
│
├── diagram.json
├── wokwi.toml
├── platformio.ini
├── README.md
└── LICENSE
```

Module responsibilities:

```text
vehicle_detector
    ↓
Detect sensor events

speed_analyzer
    ↓
Calculate speed and direction

feature_extractor
    ↓
Convert sensor measurements into ML features

ml_classifier
    ↓
Run embedded ML inference

risk_engine
    ↓
Calculate contextual traffic risk

display_manager
    ↓
Provide local driver feedback

mqtt_manager
    ↓
Send traffic events to the central system
```

---

# Development Workflow

## Step 1 — Create the ESP32 Simulation

Create the Wokwi environment and verify that the ESP32 boots correctly.

Expected output:

```text
[SENTRY] Booting...
[SENTRY] Sensors initialized
[SENTRY] ML classifier initialized
[SENTRY] System ready
```

---

## Step 2 — Implement Vehicle Detection

Start only with the virtual sensors.

```text
[SENSOR] A triggered
[SENSOR] B triggered
```

Do not add machine learning yet.

---

## Step 3 — Calculate Vehicle Speed

Measure the time difference between sensors.

```text
[SPEED]
Delta T = 184 ms
Speed   = 78.2 km/h
```

---

## Step 4 — Detect Direction

Test both possible sensor sequences.

```text
A → B = NORMAL
B → A = WRONG WAY
```

---

## Step 5 — Implement Feature Extraction

Convert raw sensor values into an ML feature vector.

Example:

```text
[FEATURES]

Length Estimate : 4.31
Occupancy       : 0.216
Signal          : 0.67
Speed           : 72.3
Variation       : 0.07
```

---

## Step 6 — Generate the Dataset

Run multiple simulated vehicle scenarios and store the extracted features.

Example:

```text
Motorcycles : 500 samples
Cars        : 500 samples
Trucks      : 500 samples
```

Include measurement variation and noise.

---

## Step 7 — Train the ML Model

Use the generated dataset to train the vehicle classifier.

```text
Dataset
   ↓
Train
   ↓
Validate
   ↓
Evaluate
   ↓
Export
```

Save model evaluation results before deploying it.

---

## Step 8 — Deploy the Model to ESP32

Convert/export the trained classifier into a format that can run inside the embedded application.

Example runtime output:

```text
[ML]

Prediction : CAR
Confidence : 93.4%
Inference  : 2.8 ms
```

Inference time can also be recorded as part of the Edge AI evaluation.

---

## Step 9 — Add the Risk Engine

Pass the ML prediction to the risk engine.

```text
[RISK]

Vehicle Risk : 10
Speed Risk   : 25
Weather Risk : 15
Traffic Risk : 10

Total        : 60
Level        : MODERATE
```

---

## Step 10 — Add the Local Display

Show:

```text
Speed
ML Vehicle Type
ML Confidence
Risk Level
Warning
```

---

## Step 11 — Add Wi-Fi and MQTT

Publish analyzed traffic events.

Example:

```text
[WiFi] Connected
[MQTT] Connected
[MQTT] Traffic event published
```

---

## Step 12 — Test Complete Scenarios

Recommended scenarios:

```text
normal_car
speeding_car
fast_motorcycle
heavy_truck
rainy_truck
heavy_traffic
wrong_way_vehicle
ambiguous_vehicle
sensor_timeout
```

---

# Traffic Data Model

Modules should communicate through shared structures.

```cpp
struct VehicleFeatures {
    float estimatedLength;
    float occupancyTime;
    float signalStrength;
    float speedKmh;
    float signalVariation;
};

struct VehicleEvent {
    VehicleFeatures features;

    VehicleType vehicleType;
    float classificationConfidence;

    Direction direction;

    int riskScore;
    RiskLevel riskLevel;
};
```

Possible enums:

```cpp
enum VehicleType {
    MOTORCYCLE,
    CAR,
    TRUCK,
    UNKNOWN
};

enum Direction {
    NORMAL,
    WRONG_WAY
};

enum RiskLevel {
    LOW,
    MODERATE,
    HIGH,
    CRITICAL
};
```

---

# MQTT Communication

Analyzed traffic events can be published to:

```text
sentry/{node_id}/traffic
```

Example:

```text
sentry/node01/traffic
```

Payload:

```json
{
  "node_id": "SENTRY-01",
  "vehicle_type": "truck",
  "classification_confidence": 0.94,
  "speed_kmh": 82.4,
  "speed_limit": 70,
  "direction": "normal",
  "risk_score": 87,
  "risk_level": "critical"
}
```

Additional topics:

```text
sentry/node01/status
sentry/node01/traffic
sentry/node01/alerts
sentry/node01/diagnostics
```

---

# Simulation Scenarios

## Scenario 1 — Normal Car

```text
ML Prediction : CAR
Confidence    : 96%
Speed         : 52 km/h
Speed Limit   : 70 km/h
Direction     : NORMAL

Expected:
LOW RISK
No alert
```

---

## Scenario 2 — Speeding Car

```text
ML Prediction : CAR
Confidence    : 92%
Speed         : 91 km/h
Speed Limit   : 70 km/h

Expected:
HIGH RISK
Local warning
MQTT event
```

---

## Scenario 3 — Fast Motorcycle

```text
ML Prediction : MOTORCYCLE
Confidence    : 95%
Speed         : 104 km/h
Speed Limit   : 70 km/h

Expected:
HIGH / CRITICAL RISK
Local alert
MQTT event
```

---

## Scenario 4 — Truck in Rain

```text
ML Prediction : TRUCK
Confidence    : 94%
Speed         : 76 km/h
Speed Limit   : 70 km/h
Weather       : RAIN
Traffic       : HIGH

Expected:
HIGH / CRITICAL RISK
Local warning
MQTT event
```

---

## Scenario 5 — Ambiguous Vehicle

```text
ML Prediction : CAR
Confidence    : 46%

Expected:
Vehicle type = UNKNOWN
Do not trust classification
Continue speed analysis
```

---

## Scenario 6 — Wrong-Way Vehicle

```text
Sensor B → Sensor A

Expected:
WRONG WAY
CRITICAL RISK
Immediate local alert
MQTT alert
```

---

# Serial Debug Output

Serial logs should make the complete processing pipeline visible.

```text
[SENTRY] Vehicle detected

[SENSOR]
A = 1250 ms
B = 1432 ms

[SPEED]
Speed = 79.1 km/h

[FEATURES]
Length    = 4.28 m
Occupancy = 0.214 s
Signal    = 0.66

[ML]
Prediction = CAR
Confidence = 93%

[RISK]
Score = 74
Level = HIGH

[ACTION]
Display warning
MQTT event published
```

---

# Tips & Best Practices

**Do not hard-code the vehicle class.** The final motorcycle/car/truck decision must come from the ML classifier.

**Separate feature extraction from classification.** Sensor measurements should first be converted into features and only then passed to the model.

**Train offline, infer on the edge.** Model training belongs in the `ml/` directory while inference belongs on the ESP32.

**Avoid data leakage.** The simulated input must not contain a hidden vehicle-class identifier that the classifier can directly use.

**Add noise to simulated data.** Perfectly separated synthetic classes would make the ML problem unrealistic.

**Measure inference performance.** Record prediction accuracy, model size, memory consumption, and inference time where possible.

**Support uncertain predictions.** Low-confidence classifications should return `UNKNOWN`.

**Keep modules independent.** The ML classifier should not directly publish MQTT messages or control the display.

**Avoid blocking delays.** Sensor and traffic processing should remain responsive.

**Test incrementally.** Detection → speed → direction → features → ML → risk → display → MQTT.

---

# Project Status

```text
Current Stage: Architecture & Wokwi Simulation Design
```

Planned implementation:

```text
Wokwi Setup
      ↓
Vehicle Detection
      ↓
Speed & Direction
      ↓
Feature Extraction
      ↓
Dataset Generation
      ↓
ML Training
      ↓
Model Evaluation
      ↓
ESP32 ML Inference
      ↓
Risk Engine
      ↓
Display
      ↓
MQTT
```

---

# Project Goal

The goal of SENTRY is not to reproduce a certified traffic radar.

The project demonstrates how:

```text
Embedded Systems
        +
Virtual Traffic Sensing
        +
Machine Learning
        +
Edge Inference
        +
Risk Analysis
        +
IoT Communication
        =
Intelligent Roadside Traffic Node
```

can be combined inside a single ESP32-based simulation.

The sensing environment is simulated in Wokwi, while the **vehicle classification model is trained using generated sensor data and deployed to the ESP32 for local inference**.

This allows SENTRY to demonstrate a complete Edge AI pipeline rather than a hard-coded vehicle detection system.
