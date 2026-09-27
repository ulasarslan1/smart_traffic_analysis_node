#include <Arduino.h>
#include "Logger.h"

constexpr uint8_t TRIG_PIN = 5;
constexpr uint8_t ECHO_PIN = 7;

enum class State { UNKNOWN, EMPTY, ENTRY, OCCUPIED, EXIT };
State currentState = State::UNKNOWN;

uint8_t candidateCount = 0;
uint32_t vehicleCount = 0;

struct Passage {
    bool active = false;
    uint32_t startMs = 0;
    uint32_t validSamples = 0;
    float minCm = 0;
    float maxCm = 0;
    double sumCm = 0;
} passage;

const char* stateName(State state) {
    switch (state) {
        case State::UNKNOWN: return "UNKNOWN";
        case State::EMPTY: return "EMPTY";
        case State::ENTRY: return "ENTRY";
        case State::OCCUPIED: return "OCCUPIED";
        case State::EXIT: return "EXIT";
        default: return "INVALID";
    }
}

float calculateDistance() {

    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    const unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000);

    if (duration == 0) return -1.0f;

    return duration * 0.0343f / 2.0f;
}

void addSample(float cm) {
    if (!passage.active) return;
    if (passage.validSamples == 0) passage.minCm = passage.maxCm = cm;
    else {
        if (cm < passage.minCm) passage.minCm = cm;
        if (cm > passage.maxCm) passage.maxCm = cm;
    }
    passage.sumCm += cm;
    ++passage.validSamples;
}

void startPassage(uint32_t now, float firstCm) {
    passage = Passage{};
    passage.active = true;
    passage.startMs = now;
    addSample(firstCm);
}

void completePassage(uint32_t now) {
    if (!passage.active || passage.validSamples == 0) return;
    ++vehicleCount;
    Logger::vehicleExited(vehicleCount);
    Logger::passage(vehicleCount, passage.startMs, now,
                    now - passage.startMs, passage.minCm, passage.maxCm,
                    static_cast<float>(passage.sumCm / passage.validSamples),
                    passage.validSamples);
    passage = Passage{};
}

void updateFSM() {

    const State previousState = currentState;
    const float distance = calculateDistance();
    const uint32_t now = millis();
    const bool valid = distance >= 0.0f && distance <= 410;

    if (!valid) {
        if (currentState == State::ENTRY) {
            currentState = State::EMPTY;
            candidateCount = 0;
            passage = Passage{};
        } else if (currentState == State::EXIT) {
            currentState = State::OCCUPIED;
            candidateCount = 0;
        }
    } else {
        switch (currentState) {
            case State::UNKNOWN:
                if (distance < 100.0f) currentState = State::OCCUPIED;
                else if (distance > 120.0f) currentState = State::EMPTY;
                break;
            case State::EMPTY:
                if (distance < 100.0f) {
                    currentState = State::ENTRY;
                    candidateCount = 1;
                    startPassage(now, distance);
                }
                break;
            case State::ENTRY:
                if (distance < 100.0f) {
                    addSample(distance);
                    if (++candidateCount >= 3) {
                        currentState = State::OCCUPIED;
                        candidateCount = 0;
                        Logger::vehicleEntered(passage.startMs);
                    }
                } else {
                    currentState = State::EMPTY;
                    candidateCount = 0;
                    passage = Passage{};
                }
                break;
            case State::OCCUPIED:
                if (passage.active) addSample(distance);
                if (distance > 120.0f) {
                    currentState = State::EXIT;
                    candidateCount = 1;
                }
                break;
            case State::EXIT:
                if (passage.active) addSample(distance);
                if (distance > 120.0f) {
                    if (++candidateCount >= 3) {
                        currentState = State::EMPTY;
                        candidateCount = 0;
                        completePassage(now);
                    }
                } else {
                    currentState = State::OCCUPIED;
                    candidateCount = 0;
                }
                break;
        }
    }
    if (previousState != currentState)
        Logger::stateTransition(stateName(previousState), stateName(currentState));
}

void setup() {

    Logger::begin();
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    digitalWrite(TRIG_PIN, LOW);
    Logger::system("System initialized");

}


void loop() {
    updateFSM();
    delay(300);
}
