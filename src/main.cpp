#include <Arduino.h>

#include "Logger.h"
#include "FeatureExtractor.h"
#include "DecisionTreeModel.h"
#include "SystemUI.h"



constexpr uint8_t TRIG_PIN = 5;
constexpr uint8_t ECHO_PIN = 7;

constexpr uint8_t GROUND_TRUTH_LABEL0_PIN = 10;
constexpr uint8_t GROUND_TRUTH_LABEL1_PIN = 11;



constexpr float ENTRY_THRESHOLD_CM = 100.0f;
constexpr float EXIT_THRESHOLD_CM = 120.0f;

constexpr uint8_t CONFIRM_COUNT = 3;

constexpr uint32_t SAMPLE_INTERVAL_MS = 300;
constexpr uint32_t CLASSIFICATION_DISPLAY_MS = 1500;



enum class State {
    UNKNOWN,
    EMPTY,
    ENTRY,
    OCCUPIED,
    EXIT
};

State currentState = State::UNKNOWN;

uint8_t candidateCount = 0;
uint32_t nextPassageId = 1;



struct Passage {
    bool active = false;
    uint32_t id = 0;
    uint32_t startMs = 0;
};

Passage passage;


FeatureExtractor featureExtractor;
SystemUI systemUI;
VehicleCounters vehicleCounters;


float calculateDistance()
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);

    digitalWrite(TRIG_PIN, LOW);

    const unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000);

    if (duration == 0) {
        return -1.0f;
    }

    return duration * 0.0343f / 2.0f;
}


bool readGroundTruth(VehicleClass& vehicleClass)
{
    const bool label0 = digitalRead(GROUND_TRUTH_LABEL0_PIN);
    const bool label1 = digitalRead(GROUND_TRUTH_LABEL1_PIN);

    if (!label1 && !label0) {
        vehicleClass = VehicleClass::MOTORCYCLE;
        return true;
    }

    if (!label1 && label0) {
        vehicleClass = VehicleClass::CAR;
        return true;
    }

    if (label1 && !label0) {
        vehicleClass = VehicleClass::TRUCK;
        return true;
    }

    return false;
}


void updateVehicleCounters(VehicleClass vehicleClass)
{
    ++vehicleCounters.total;

    switch (vehicleClass) {
        case VehicleClass::CAR:
            ++vehicleCounters.car;
            break;

        case VehicleClass::MOTORCYCLE:
            ++vehicleCounters.motorcycle;
            break;

        case VehicleClass::TRUCK:
            ++vehicleCounters.truck;
            break;
    }
}


void startPassage(uint32_t now, float firstDistance)
{
    passage.active = true;
    passage.id = nextPassageId++;
    passage.startMs = now;

    featureExtractor.reset(now);
    featureExtractor.addSample(firstDistance);

    systemUI.showDetecting(vehicleCounters);
}



void addPassageSample(float distance)
{
    if (!passage.active) {
        return;
    }

    if (distance >= ENTRY_THRESHOLD_CM) {
        return;
    }

    featureExtractor.addSample(distance);
}


void cancelPassage()
{
    passage = Passage{};

    systemUI.showReady(vehicleCounters);
}


void completePassage(uint32_t now)
{
    if (!passage.active) {
        return;
    }

    PassageFeatures features;

    if (!featureExtractor.extract(now, features)) {
        Logger::error("Insufficient passage samples");

        systemUI.showError(vehicleCounters);
        delay(CLASSIFICATION_DISPLAY_MS);
        systemUI.showReady(vehicleCounters);

        passage = Passage{};

        return;
    }

    
    const VehicleClass prediction = predictVehicle(features);


    updateVehicleCounters(prediction);

    VehicleClass actual;

    if (readGroundTruth(actual)) {
        Logger::vehicle(passage.id, actual, prediction);
    }
    else {
        Logger::error("Invalid ground truth label");
    }

   
    systemUI.showClassification(prediction, vehicleCounters);

    delay(CLASSIFICATION_DISPLAY_MS);

    systemUI.showReady(vehicleCounters);

    passage = Passage{};
}


void updateFSM()
{
    const float distance = calculateDistance();
    const uint32_t now = millis();

    const bool valid = distance >= 0.0f && distance <= 400.0f;

   
    if (!valid) {
        if (currentState == State::ENTRY) {
            currentState = State::EMPTY;
            candidateCount = 0;

            cancelPassage();
        }
        else if (currentState == State::EXIT) {
            currentState = State::OCCUPIED;
            candidateCount = 0;
        }

        return;
    }

   
    switch (currentState) {
        case State::UNKNOWN:
            if (distance > EXIT_THRESHOLD_CM) {
                currentState = State::EMPTY;

                systemUI.showReady(vehicleCounters);
            }
            else if (distance < ENTRY_THRESHOLD_CM) {
                currentState = State::OCCUPIED;
            }

            break;

        case State::EMPTY:
            if (distance < ENTRY_THRESHOLD_CM) {
                currentState = State::ENTRY;
                candidateCount = 1;

                startPassage(now, distance);
            }

            break;

        case State::ENTRY:
            if (distance < ENTRY_THRESHOLD_CM) {
                addPassageSample(distance);

                ++candidateCount;

                if (candidateCount >= CONFIRM_COUNT) {
                    currentState = State::OCCUPIED;
                    candidateCount = 0;
                }
            }
            else {
                currentState = State::EMPTY;
                candidateCount = 0;

                cancelPassage();
            }

            break;

        case State::OCCUPIED:
            if (distance < ENTRY_THRESHOLD_CM) {
                addPassageSample(distance);
            }
            else if (distance > EXIT_THRESHOLD_CM) {
                currentState = State::EXIT;
                candidateCount = 1;
            }

            break;

        case State::EXIT:
            if (distance > EXIT_THRESHOLD_CM) {
                ++candidateCount;

                if (candidateCount >= CONFIRM_COUNT) {
                    currentState = State::EMPTY;
                    candidateCount = 0;

                    completePassage(now);
                }
            }
            else {
                currentState = State::OCCUPIED;
                candidateCount = 0;

                if (distance < ENTRY_THRESHOLD_CM) {
                    addPassageSample(distance);
                }
            }

            break;
    }
}


void setup()
{
    Logger::begin();

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    pinMode(GROUND_TRUTH_LABEL0_PIN, INPUT);
    pinMode(GROUND_TRUTH_LABEL1_PIN, INPUT);

    digitalWrite(TRIG_PIN, LOW);

    systemUI.begin();

    Logger::system("Smart Traffic Node ready");
}


void loop()
{
    updateFSM();

    delay(SAMPLE_INTERVAL_MS);
}