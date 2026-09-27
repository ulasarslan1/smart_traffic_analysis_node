
#include "Logger.h"

namespace Logger {

    constexpr bool ENABLE_SENSOR_LOG = false;
    constexpr bool ENABLE_FSM_LOG = false;

    void begin()
    {
        Serial.begin(115200);
    }

    void system(const char* message)
    {
        Serial.printf(
            "[SYSTEM] %s\r\n",
            message
        );
    }

    void sensor(float distance, bool valid)
    {
        if (!ENABLE_SENSOR_LOG) {
            return;
        }

        if (valid) {
            Serial.printf(
                "[SENSOR] Distance: %.1f cm\r\n",
                distance
            );
        }
        else {
            Serial.print(
                "[SENSOR] INVALID\r\n"
            );
        }
    }

    void stateTransition(
        const char* from,
        const char* to
    )
    {
        Serial.printf(
            "[FSM] %s -> %s\r\n",
            from,
            to
        );
    }

    void fsm(
        const char* state,
        float distance,
        bool valid,
        uint8_t candidateCount
    )
    {
        if (!ENABLE_FSM_LOG) {
            return;
        }

        if (valid) {
            Serial.printf(
                "[FSM] %-8s | %6.1f cm | Confirm: %u\r\n",
                state,
                distance,
                candidateCount
            );
        }
        else {
            Serial.printf(
                "[FSM] %-8s | INVALID | Confirm: %u\r\n",
                state,
                candidateCount
            );
        }
    }

    void vehicleEntered()
    {
        Serial.print(
            "[EVENT] VEHICLE_ENTERED\r\n"
        );
    }

    void vehicleExited(uint32_t total)
    {
        Serial.printf(
            "[EVENT] VEHICLE_EXITED | Total: %lu\r\n",
            static_cast<unsigned long>(total)
        );
    }

}
