#pragma once

#include <Arduino.h>

namespace Logger {

void begin();

void system(const char* message);

void sensor(float distance, bool valid);

void stateTransition(const char* from, const char* to);

void fsm(const char* state, float distance, bool valid, uint8_t candidateCount);

void vehicleEntered(uint32_t startedAtMs);

void vehicleExited(uint32_t total);

void passage(uint32_t id, uint32_t startedAtMs, uint32_t endedAtMs,
             uint32_t durationMs, float minCm, float maxCm, float avgCm,
             uint32_t validSamples);


}