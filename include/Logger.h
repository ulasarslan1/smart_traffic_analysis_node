#pragma once

#include <Arduino.h>

namespace Logger {

void begin();

void system(const char* message);

void stateTransition(
    const char* from,
    const char* to
);

void vehicleEntered(uint32_t startedAtMs);

void vehicleExited(uint32_t total);

// Raw passage data
void passageStart(
    uint32_t id,
    uint32_t startMs
);

void sample(
    uint32_t id,
    uint32_t relativeTimeMs,
    float distanceCm
);

void passageEnd(
    uint32_t id,
    uint32_t endMs
);

}