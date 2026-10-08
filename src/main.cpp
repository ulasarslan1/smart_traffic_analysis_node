#include <Arduino.h>

#include "Logger.h"
#include "DecisionTreeModel.h"
#include "SystemUI.h"
#include "Passage.h"


constexpr uint8_t TRIG_PIN = 5;
constexpr uint8_t ECHO_PIN = 7;
constexpr uint8_t LABEL0_PIN = 10;
constexpr uint8_t LABEL1_PIN = 11;

constexpr float ENTRY = 100.0f;
constexpr float EXIT = 120.0f;

constexpr uint8_t COUNT = 3;


bool classificationDisplayed = false;
uint32_t classificationDisplayStartMs = 0;
uint32_t lastSampleMs = 0;


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

Passage passage;
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


bool readData(VehicleClass& vehicleClass)
{
    const bool label0 = digitalRead(LABEL0_PIN);
    const bool label1 = digitalRead(LABEL1_PIN);

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

void startClassificationDisplay(VehicleClass prediction)
{
    systemUI.showClassification(prediction, vehicleCounters);
    classificationDisplayed = true;
    classificationDisplayStartMs = millis();
}

void updateUI(uint32_t now)
{
    if (!classificationDisplayed) {return;}
    if (now - classificationDisplayStartMs < 1500) {return;}
    classificationDisplayed = false;
    if (currentState == State::EMPTY) {systemUI.showReady(vehicleCounters);}
        
}


void startPassage(uint32_t now, float firstDistance)
{
    passage.start(nextPassageId++, now, firstDistance);
    classificationDisplayed = false;
    systemUI.showDetecting(vehicleCounters);
}


void addPassageSample(float distance)
{
    if (!passage.isActive()) {return;}
    if (distance >= ENTRY) {return;}
    passage.addSample(distance);
}

void cancelPassage()
{
    passage.cancel();
    classificationDisplayed = false;
    systemUI.showReady(vehicleCounters);
}

void completePassage(uint32_t now)
{
    if (!passage.isActive()) {return;}

    const uint32_t passageId = passage.getId();

    PassageFeatures features;

    if (!passage.complete(now, features)) {
        Logger::error("Insufficient passage samples");
        classificationDisplayed = false;
        systemUI.showError(vehicleCounters);
        passage.cancel();
        return;
    }

    const VehicleClass prediction = predictVehicle(features);
    updateVehicleCounters(prediction);

    VehicleClass actual;

    if (readData(actual)) {
        Logger::vehicle(passageId, actual, prediction);
    }
    else {
        Logger::error("Invalid ground truth label");
    }

    startClassificationDisplay(prediction);

    passage.cancel();
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
            if (distance > EXIT) {
                currentState = State::EMPTY;

                systemUI.showReady(vehicleCounters);
            }
            else if (distance < ENTRY) {
                currentState = State::OCCUPIED;
            }

            break;

        case State::EMPTY:
            if (distance < ENTRY) {
                currentState = State::ENTRY;
                candidateCount = 1;

                startPassage(now, distance);
            }

            break;

        case State::ENTRY:
            if (distance < ENTRY) {
                addPassageSample(distance);

                ++candidateCount;

                if (candidateCount >= COUNT) {
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
            if (distance < ENTRY) {
                addPassageSample(distance);
            }
            else if (distance > EXIT) {
                currentState = State::EXIT;
                candidateCount = 1;
            }

            break;

        case State::EXIT:
            if (distance > EXIT) {
                ++candidateCount;

                if (candidateCount >= COUNT) {
                    currentState = State::EMPTY;
                    candidateCount = 0;

                    completePassage(now);
                }
            }
            else {
                currentState = State::OCCUPIED;
                candidateCount = 0;

                if (distance < ENTRY) {
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

    pinMode(LABEL0_PIN, INPUT);
    pinMode(LABEL1_PIN, INPUT);

    digitalWrite(TRIG_PIN, LOW);

    systemUI.begin();

    Logger::system("Smart Traffic Node ready");
}


void loop()
{
    const uint32_t now = millis();

    updateUI(now);

    if (now - lastSampleMs >= 300) {
        lastSampleMs = now;
        updateFSM();
    }
}