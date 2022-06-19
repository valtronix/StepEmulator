#pragma once

#include "globals.h"
#include <Servo.h>

/// Methods to make movements to emulate walk/run.
namespace movements
{
Servo myservo;
/// Define PIN used to connect a servo (only 9 or 10 is supported)
const uint8_t pinServo = 10; // Only 9 or 10 is supported

/// Number of steps remaining
unsigned int stepsRemaining;
// Actual speed (steps by second)
unsigned char speed;

/// Setup movements
void setupMovements()
{
// Nothing to do
}

/// Power ON motor/servo to be ready to move
void powerOnMovements()
{
    // timer1 (TCCR1) used by servo
    myservo.attach(pinServo);
}

/// Power OFF motor/servo
void powerOffMovements()
{
    if (myservo.attached())
    {
        myservo.detach();
    }
    digitalWrite(pinServo, LOW);
}

/// Setup the position of the servo
void setMovements(uint8_t value)
{
    myservo.write(value);
}

/// Emulate walk movements
bool walk()
{
  static unsigned long nextStepAt = 0;
  static bool footUp = false;
  static bool walking = false;
  if (stepsRemaining == 0)
  {
    return true;
  }
  else
  {
    unsigned long now = millis();
    if (!walking)
    { // Start walking
      walking = true;
      powerOnMovements();
      footUp = false;
      nextStepAt = 0;
    }
    if (int(nextStepAt - now) < 0)
    {
      nextStepAt = now + (unsigned long)(30000U / (unsigned int)speed); // 60000 / 2
      footUp = !footUp;
      userinterface::disp.writeDot(DOT_RESERVED, footUp);
      if (footUp)
      {
        myservo.write(config.pos_stepup);
      }
      else
      {
        myservo.write(config.pos_stepdown);
        stepsRemaining--;
        if (stepsRemaining == 0)
        {
          walking = false;
          powerOffMovements();
        }
        if (stateMachine::state == stateMachine::States::Emulate)
        {
          userinterface::displaySteps();
        }
      }
    }
  }
  return false;
}

} // namespace movements
