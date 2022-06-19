#pragma once

#include "globals.h"

/// Methods to use a buzzer
namespace buzzer
{
/// Pin on which the buzzer is connected
const uint8_t pinBuzzer=11;

/// Initialize the buzzer
void setupBuzzer()
{
    pinMode(pinBuzzer, OUTPUT);
    digitalWrite(pinBuzzer, HIGH);
}

/// Tests the buzzer
void testBuzzer(unsigned long duration)
{
    digitalWrite(pinBuzzer, millis() <= duration);
}

/// Emits a short "clic" sound
void clicBuzzer() {
    bool clic = digitalRead(pinBuzzer);
    digitalWrite(pinBuzzer, !clic);
    delay(2);
    digitalWrite(pinBuzzer, clic);
}

/// Mutes the buzzer
void muteBuzzer() {
    digitalWrite(pinBuzzer, LOW);
}

/// Ring the buzzer
void ringBuzzer() {
    digitalWrite(pinBuzzer, HIGH);
}

} // namespace buzzer
