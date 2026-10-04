#include <Arduino.h>

#include "Logger.h"
#include "FeatureExtractor.h"
#include "DecisionTreeModel.h"

constexpr uint8_t TRIG_PIN = 5;
constexpr uint8_t ECHO_PIN = 7;

enum class State {
    UNKNOWN,
    EMPTY,
    ENTRY,
    OCCUPIED,
    EXIT
};

State currentState = State::UNKNOWN;

uint8_t candidateCount = 0;
uint32_t vehicleCount = 0;
uint32_t nextPassageId = 1;

struct Passage {
    bool active = false;
    uint32_t id = 0;
    uint32_t startMs = 0;
};

Passage passage;

FeatureExtractor featureExtractor;

const char* stateName(State state)
{
    switch (state) {
        case State::UNKNOWN:
            return "UNKNOWN";

        case State::EMPTY:
            return "EMPTY";

        case State::ENTRY:
            return "ENTRY";

        case State::OCCUPIED:
            return "OCCUPIED";

        case State::EXIT:
            return "EXIT";

        default:
            return "INVALID";
    }
}

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

void startPassage(uint32_t now, float firstDistance)
{
    passage.active = true;
    passage.id = nextPassageId++;
    passage.startMs = now;

    featureExtractor.reset(now);

    featureExtractor.addSample(firstDistance);

    Logger::passageStart(
        passage.id,
        passage.startMs
    );

    Logger::sample(
        passage.id,
        0,
        firstDistance
    );
}

void addPassageSample(uint32_t now, float distance)
{
    if (!passage.active) {
        return;
    }

    if (distance >= 100.0f) {
        return;
    }

    const uint32_t relativeTime =
        now - passage.startMs;

    featureExtractor.addSample(distance);

    Logger::sample(
        passage.id,
        relativeTime,
        distance
    );
}

void cancelPassage()
{
    passage = Passage{};
}

void completePassage(uint32_t now)
{
    if (!passage.active) {
        return;
    }

    ++vehicleCount;

    Logger::vehicleExited(vehicleCount);
    Logger::passageEnd(passage.id, now);

    PassageFeatures features;

    if (featureExtractor.extract(now, features)) {

        const VehicleClass prediction =
            predictVehicle(features);

        Serial.printf("[FEATURES] duration_ms=%.0f,min_cm=%.2f,max_cm=%.2f,mean_cm=%.2f,std_cm=%.2f,range_cm=%.2f,mean_delta_cm=%.2f,valid_samples=%.0f\r\n", features.duration_ms, features.min_cm, features.max_cm, features.mean_cm, features.std_cm, features.range_cm, features.mean_delta_cm, features.valid_samples);

        Serial.printf("[ML] prediction=%s\r\n", vehicleClassName(prediction));
    }
    else {
        Serial.println("[ML] prediction=UNKNOWN | Insufficient samples");
    }

    passage = Passage{};
}

void updateFSM()
{
    const State previousState = currentState;

    const float distance = calculateDistance();
    const uint32_t now = millis();

    const bool valid =
        distance >= 0.0f &&
        distance <= 400.0f;

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
    }

    else {

        switch (currentState) {

            case State::UNKNOWN:

                if (distance > 120.0f) {
                    currentState = State::EMPTY;
                }
                else if (distance < 100.0f) {
                    currentState = State::OCCUPIED;
                }

                break;

            case State::EMPTY:

                if (distance < 100.0f) {
                    currentState = State::ENTRY;
                    candidateCount = 1;

                    startPassage(
                        now,
                        distance
                    );
                }

                break;

            case State::ENTRY:

                if (distance < 100.0f) {

                    addPassageSample(
                        now,
                        distance
                    );

                    ++candidateCount;

                    if (candidateCount >= 3) {
                        currentState = State::OCCUPIED;
                        candidateCount = 0;

                        Logger::vehicleEntered(
                            passage.startMs
                        );
                    }
                }

                else {
                    currentState = State::EMPTY;
                    candidateCount = 0;

                    cancelPassage();
                }

                break;

            case State::OCCUPIED:

                if (distance < 100.0f) {

                    addPassageSample(
                        now,
                        distance
                    );
                }

                else if (distance > 120.0f) {
                    currentState = State::EXIT;
                    candidateCount = 1;
                }

                break;

            case State::EXIT:

                if (distance > 120.0f) {

                    ++candidateCount;

                    if (candidateCount >= 3) {
                        currentState = State::EMPTY;
                        candidateCount = 0;

                        completePassage(now);
                    }
                }

                else {
                    currentState = State::OCCUPIED;
                    candidateCount = 0;

                    if (distance < 100.0f) {
                        addPassageSample(
                            now,
                            distance
                        );
                    }
                }

                break;
        }
    }

    if (previousState != currentState) {
        Logger::stateTransition(
            stateName(previousState),
            stateName(currentState)
        );
    }
}

void setup()
{
    Logger::begin();

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    digitalWrite(TRIG_PIN, LOW);

    Logger::system("System initialized");
}

void loop()
{
    updateFSM();

    delay(300);
}