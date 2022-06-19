#pragma once

#include "globals.h"

/// Methods to use built-in LED
namespace builtinled
{
/// Setup built-in LED
void setupBuiltInLed()
{
    pinMode(LED_BUILTIN, OUTPUT);
}

/// Switch ON built-in LED
void ledOn()
{
    digitalWrite(LED_BUILTIN, HIGH);
}

/// Switch OFF built-in LED
void ledOff()
{
    digitalWrite(LED_BUILTIN, LOW);
}
} // namespace builtinled
