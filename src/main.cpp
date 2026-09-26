
#include <Arduino.h>

constexpr uint8_t TRIG_PIN = 5;
constexpr uint8_t ECHO_PIN = 7;
constexpr uint8_t CONFIRM_COUNT = 3;

enum class State {
    UNKNOWN,
    EMPTY,
    ENTRY,
    OCCUPIED,
    EXIT
};

State currentState = State::UNKNOWN;

uint8_t candidateCount = 0;

bool vehicleEntered = false;
bool vehicleExited  = false;


uint32_t vehicleCount = 0;

const char* stateName(State state)
{
    switch (state) {
        case State::UNKNOWN:  return "UNKNOWN";
        case State::EMPTY:    return "EMPTY";
        case State::ENTRY:    return "ENTRY";
        case State::OCCUPIED: return "OCCUPIED";
        case State::EXIT:     return "EXIT";
        default:              return "INVALID";
    }
}


float calculateDistance()
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);

    digitalWrite(TRIG_PIN, LOW);

    unsigned long duration = pulseIn(
        ECHO_PIN,
        HIGH,
        30000
    );

    if (duration == 0) {
        return -1.0f;
    }

    return duration * 0.0343f / 2.0f;
}


void updateFSM()
{
    State previousState = currentState;

    float distance = calculateDistance();
    bool valid = (distance >= 0.0f);

    switch (currentState)
    {
        
        case State::UNKNOWN:

            if (valid) {
                if (distance < 100.0f) {
                    currentState = State::OCCUPIED;
                }
                else if (distance > 120.0f) {
                    currentState = State::EMPTY;
                }
            }
            break;


        // ----------------------
        // EMPTY
        // ----------------------

        case State::EMPTY:

            if (valid && distance < 100.0f) {
                currentState = State::ENTRY;
                candidateCount = 1;
            }
            break;

        case State::ENTRY:

            if (!valid) {
                currentState = State::EMPTY;
                candidateCount = 0;
            }
            else if (distance < 100.0f) {
                candidateCount++;

                if (candidateCount >= CONFIRM_COUNT) {
                    currentState = State::OCCUPIED;
                    candidateCount = 0;

                    vehicleEntered = true;
                }
            }
            else {
                currentState = State::EMPTY;
                candidateCount = 0;
            }
            break;

        case State::OCCUPIED:

            if (valid && distance > 120.0f) {
                currentState = State::EXIT;
                candidateCount = 1;
            }
            break;

        case State::EXIT:

            if (!valid) {
                currentState = State::OCCUPIED;
                candidateCount = 0;
            }
            else if (distance > 120.0f) {
                candidateCount++;

                if (candidateCount >= CONFIRM_COUNT) {
                    currentState = State::EMPTY;
                    candidateCount = 0;

                    vehicleExited = true;
                }
            }
            else {
                currentState = State::OCCUPIED;
                candidateCount = 0;
            }
            break;

        default:
            currentState = State::UNKNOWN;
            candidateCount = 0;
            break;
    }

    
    if (valid) {
        Serial.printf(
            "[FSM] %-8s | Distance: %6.1f cm | Count: %d\r\n",
            stateName(currentState),
            distance,
            candidateCount
        );
    }
    else {
        Serial.printf(
            "[FSM] %-8s | Distance: INVALID | Count: %d\r\n",
            stateName(currentState),
            candidateCount
        );
    }

}


void handleEvents()
{

    if (vehicleEntered) {

        Serial.println(
            "[EVENT] VEHICLE_ENTERED\r\n"
        );


        vehicleEntered = false;
    }


    if (vehicleExited) {

        vehicleCount++;

        Serial.printf(
            "[EVENT] VEHICLE_EXITED | Total: %lu\r\n",
            (unsigned long)vehicleCount
        );

        vehicleExited = false;
    }
}

void setup()
{
    Serial.begin(115200);

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    digitalWrite(TRIG_PIN, LOW);

    Serial.println("[SYSTEM] SENTRY initialized\r\n");
    Serial.println("[SYSTEM] Waiting for sensor...\r\n");
}

void loop()
{
    updateFSM();

    handleEvents();

    delay(300);
}
