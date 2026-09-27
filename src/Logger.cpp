#include "Logger.h"

namespace Logger {

    void begin() { Serial.begin(115200); }

    void system(const char* message) { Serial.printf("[SYSTEM] %s\r\n", message); }

    void stateTransition(const char* from, const char* to) {
        Serial.printf("[FSM] %s -> %s\r\n", from, to);
    }

    void vehicleEntered(uint32_t startedAtMs) {
        Serial.printf("[EVENT] VEHICLE_ENTERED | Started: %lu ms\r\n", (unsigned long)startedAtMs);
    }

    void vehicleExited(uint32_t total) {
        Serial.printf("[EVENT] VEHICLE_EXITED | Total: %lu\r\n", (unsigned long)total);
    }

    void passage(uint32_t id, uint32_t startedAtMs, uint32_t endedAtMs,
                uint32_t durationMs, float minCm, float maxCm, float avgCm,
                uint32_t validSamples) {

        Serial.printf("[PASSAGE] id=%lu,start_ms=%lu,end_ms=%lu,duration_ms=%lu,min_cm=%.1f,max_cm=%.1f,avg_cm=%.1f,valid=%lu\r\n",
                    (unsigned long)id, (unsigned long)startedAtMs,
                    (unsigned long)endedAtMs, (unsigned long)durationMs,
                    minCm, maxCm, avgCm, (unsigned long)validSamples);
    }
    
}
