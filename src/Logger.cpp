#include "Logger.h"

namespace Logger {

void begin() {
    Serial.begin(115200);
}

void system(const char* message) {
    Serial.printf(
        "[SYSTEM] %s\r\n",
        message
    );
}

void stateTransition(
    const char* from,
    const char* to
) {
    Serial.printf(
        "[FSM] %s -> %s\r\n",
        from,
        to
    );
}

void vehicleEntered(uint32_t startedAtMs) {
    Serial.printf(
        "[EVENT] VEHICLE_ENTERED | Started: %lu ms\r\n",
        static_cast<unsigned long>(startedAtMs)
    );
}

void vehicleExited(uint32_t total) {
    Serial.printf(
        "[EVENT] VEHICLE_EXITED | Total: %lu\r\n",
        static_cast<unsigned long>(total)
    );
}

void passageStart(
    uint32_t id,
    uint32_t startMs
) {
    Serial.printf(
        "[PASSAGE_START] id=%lu,start_ms=%lu\r\n",
        static_cast<unsigned long>(id),
        static_cast<unsigned long>(startMs)
    );
}

void sample(
    uint32_t id,
    uint32_t relativeTimeMs,
    float distanceCm
) {
    Serial.printf(
        "[SAMPLE] id=%lu,t_ms=%lu,distance_cm=%.2f\r\n",
        static_cast<unsigned long>(id),
        static_cast<unsigned long>(relativeTimeMs),
        distanceCm
    );
}

void passageEnd(
    uint32_t id,
    uint32_t endMs
) {
    Serial.printf(
        "[PASSAGE_END] id=%lu,end_ms=%lu\r\n",
        static_cast<unsigned long>(id),
        static_cast<unsigned long>(endMs)
    );
}

}