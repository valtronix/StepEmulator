#pragma once

#include "globals.h"
#include "buzzerHelper.h"
#include <Button.h>
#include <Display.h>

/// Methods to manage UI (screen and buttons)
namespace userinterface
{
#define PIN_ENCS          2 // INT0
#define PIN_ENCA          3 // INT1
#define PIN_ENCB          4

#define PIN_SR_DI         5
#define PIN_SR_CK         6
#define PIN_SR_ST         7
#define PIN_CD4017_MR     8
#define PIN_DIGIT_ENA     9

#define DOT_STEP                0
#define DOT_SPEED               1
#define DOT_TIME                2
#define DOT_RESERVED            3
#define DOT_LONGPRESS           4

/// Debounce time (in ms)
const unsigned long debounceTime = 5;

Display disp(PIN_SR_CK, PIN_SR_DI, PIN_SR_ST, PIN_CD4017_MR, PIN_DIGIT_ENA);
Button encbtn(PIN_ENCS);

volatile int8_t rot;
/// Last time digits was changed
unsigned long lastUserInteractionAt;
/// Long press delay
unsigned long longPressDelay;

/// Initialize the user interface
void setupUI()
{
    // Set up encoder --------------------------------------------------------
    pinMode(PIN_ENCA, INPUT);
    pinMode(PIN_ENCB, INPUT);

    lastUserInteractionAt = 0UL;
    longPressDelay = 1000;  // 1 second

    rot = 0;
    // On Uno card, only pins 2 and 3 are hardware interrupts
    attachInterrupt(digitalPinToInterrupt(PIN_ENCA), onEncoderTurned, FALLING);

    // Set up display --------------------------------------------------------
    disp.begin();
    // Lamp test
    disp.lampTest();
}

/// Refresh display and check buttons
void refreshUI()
{
  disp.writeDot(DOT_LONGPRESS, encbtn.isPressed() && (encbtn.getPressedDuration() > longPressDelay));
  disp.displayNextDigit();
  encbtn.check();
}

/// Clear all screen (including dots)
void displayClear()
{
  disp.clear();
  disp.noCursor();
  disp.noLeadingZeros();
}

/// Display [===]
void displayBars()
{
  disp.write(0, 0xf0);
  disp.write(DIGIT_MAX-1, 0x9c);
  for (int i = 1; i < DIGIT_MAX-1; i++)
  {
    disp.write(i, 0x90);
  }
}

/// Display OFF
void displayOff()
{
  disp.clear();
  disp.display();
  disp.noCursor();
  disp.write(2, 0xfc); // O
  disp.write(1, 0x8e); // F
  disp.write(0, 0x8e); // F
}

/// Display number of steps
void displaySteps()
{
  disp.write(movements::stepsRemaining);
  disp.writeDot(DOT_SPEED, false);
  disp.writeDot(DOT_STEP, true);
}

/// Display speed
void displaySpeed()
{
  disp.write(movements::speed);
  disp.writeDot(DOT_STEP, false);
  disp.writeDot(DOT_SPEED, true);
}

/// Encoder rotation handled
void resetEncoderPosition()
{
  rot = 0;
}

/// Encoder button handled
void resetEncoderButton()
{
  encbtn.handled();
}

/// Reset encoder
void resetEncoder()
{
  rot = 0;
  encbtn.handled();
}

/// Is encoder been rotated?
bool isEncoderRotated()
{
  return rot != 0;
}

/// Handle encoder rotation
bool encoderChangeValue(uint8_t *value, const uint8_t maxValue)
{
  bool overflow = false;
  if ((rot < 0) && (*value < -rot))
  {
    *value = 0;
    overflow = true;
  }
  else if ((rot > 0) && ((int)(maxValue - *value) < rot))
  {
    *value = maxValue;
    overflow = true;
  }
  else
  {
    *value += rot;
  }
  rot = 0;
  return overflow;
}

/*
 * Methods called from interruptions *****************************************
 */
/// Interrupt raised each time the encoder is turned
void onEncoderTurned() {
  volatile static unsigned long rotatedAt = 0;
  if ((millis() - rotatedAt) > debounceTime)
  { // Debounce
    if (digitalRead(PIN_ENCB))
    {
      if (rot == -128)
      { // Overflow
        buzzer::clicBuzzer();
        delay(2);
        buzzer::clicBuzzer();
        delay(2);
        buzzer::clicBuzzer();
      }
      else
      {
        rot--;
      }
    }
    else
    {
      if (rot == 127)
      { // Overflow
        buzzer::clicBuzzer();
        delay(2);
        buzzer::clicBuzzer();
        delay(2);
        buzzer::clicBuzzer();
      }
      else
      {
        rot++;
      }
    }
    rotatedAt = millis();
  }
}

} // namespace userinterface
