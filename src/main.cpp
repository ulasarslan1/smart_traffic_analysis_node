#include <Arduino.h>

// Sensor A
constexpr uint8_t TRIG_A = 5;
constexpr uint8_t ECHO_A = 7;

// Sensor B
constexpr uint8_t TRIG_B = 6;
constexpr uint8_t ECHO_B = 18;

float readDistanceCm(uint8_t trigPin, uint8_t echoPin)
{
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);

    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);

    digitalWrite(trigPin, LOW);

    unsigned long duration =
        pulseIn(echoPin, HIGH, 30000);

    if (duration == 0)
    {
        return -1.0;
    }

    // Speed of sound ≈ 0.0343 cm/us
    return duration * 0.0343f / 2.0f;
}

void setup()
{
    Serial.begin(115200);

    pinMode(TRIG_A, OUTPUT);
    pinMode(ECHO_A, INPUT);

    pinMode(TRIG_B, OUTPUT);
    pinMode(ECHO_B, INPUT);

    Serial.println();
    Serial.println("============================");
    Serial.println("       SENTRY SYSTEM");
    Serial.println("============================");

    Serial.println("[SENTRY] Ultrasonic sensors initialized");

    Serial.println("[SENSOR A] TRIG GPIO 5 / ECHO GPIO 7");
    Serial.println("[SENSOR B] TRIG GPIO 6 / ECHO GPIO 18");

    Serial.println("[SENTRY] System ready");
}

void loop()
{
    float distanceA = readDistanceCm(TRIG_A, ECHO_A);

    // HC-SR04 sensors should not be triggered
    // at exactly the same time.
    delay(30);

    float distanceB = readDistanceCm(TRIG_B, ECHO_B);

    Serial.print("[A] ");
    Serial.print(distanceA);
    Serial.print(" cm");

    Serial.print(" | ");

    Serial.print("[B] ");
    Serial.print(distanceB);
    Serial.println(" cm");

    delay(200);
}