#include <Arduino.h>
#include "Encoder.h"

Encoder::Encoder(unsigned char pin_a, unsigned char pin_b)
{
    pinA = pin_a;
    pinB = pin_b;
    pinMode(pinA, INPUT);
    pinMode(pinB, INPUT);
    clear();
    attachInterrupt(digitalPinToInterrupt(pinA), onEncoderTurned, FALLING);
}

Encoder::~Encoder()
{
    detachInterrupt(digitalPinToInterrupt(pinA));
    clear();
}

int Encoder::read()
{
    return rot;
}

void Encoder::clear()
{
    rot = 0;
}

void Encoder::onEncoderTurned() {
  if ((millis() - rotatedAt) > DEBOUNCE_TIME)
  { // Debounce
    rot += digitalRead(pinB)?-1:1;
    rotatedAt = millis();
  }
}
