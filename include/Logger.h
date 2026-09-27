
#pragma once

#include <Arduino.h>

namespace Logger {

    void begin();

    void system(const char* message);

    void sensor(float distance, bool valid);

    void stateTransition(
        const char* from,
        const char* to
    );

    void fsm(
        const char* state,
        float distance,
        bool valid,
        uint8_t candidateCount
    );

    void vehicleEntered();

    void vehicleExited(uint32_t total);

}
