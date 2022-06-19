
#include "globals.h"
#include "stateMachineHelper.h"
#include "buzzerHelper.h"
#include "powerHelper.h"
#include "userinterfaceHelper.h"
#include "movementsHelper.h"
#include "builtInLedHelper.h"
#include <EEPROM.h>


// #define DEBUG_SER


unsigned long powerOffDelay;          // Delay before to switch off
unsigned long setTimeout;             // Delay before leaving set mode (timeout)

struct MyConfig_t {
  unsigned char pos_stepdown;         // Servo motor position when foot is down
  unsigned char pos_stepup;           // Servo motor position when foot is up
  unsigned int  steps_init;           // Number of steps by default (at startup)
  unsigned int  steps_min;            // Number of steps at minimum
  unsigned int  steps_max;            // Number of steps at maximum
  unsigned char speed_init;           // Default speed (at startup) (steps by minute)
  unsigned char speed_min;            // Lowest speed (steps by minute)
  unsigned char speed_max;            // Highest speed (steps by minute)
  unsigned char step_ratio;           // Step ratio (step up/down)
  unsigned char delay_longpress;      // Delay for a long press (previously LONG_PRESS) but in 10th of seconds
  unsigned char delay_set;            // Delay to exit set mode (previously DIGIT_TIMEOUT) but in 10th of seconds
  unsigned char delay_off;            // Delay before displaying OFF message (in seconds)
  unsigned char delay_offmsg;         // Delay before auto power off after OFF message (in 10th of seconds)
} config;

const MyConfig_t defaultConfig = {
  22,         // 0x00: 0x16
  130,        // 0x01: 0x82
  1000,       // 0x02: 0xe8 0x03 [232 3]
  10,         // 0x04: 0x0a 0x00 [10 0]
  20000,      // 0x06: 0x20 0x4e [32 78]
  100,        // 0x08: 0x64
  16,         // 0x09: 0x10
  255,        // 0x0a: 0xff
  50,         // 0x0b: 0x32
  10,         // 0x0c: 0x0a (1 second)
  15,         // 0x0d: 0x0f (1.5 second)
  60,         // 0x0e: 0x3c (60 seconds)
  50          // 0x0f: 0x32 (5 seconds)
};

/*****************************************************************************
 * Initialisation ------------------------------------------------------------
 *****************************************************************************/
void setup() {
  const static unsigned char configSize = sizeof(MyConfig_t) - 1;
#ifdef DEBUG_SER
  Serial.begin(9600);
#endif
  power::setupPower();
  builtinled::setupBuiltInLed();
  buzzer::setupBuzzer();
  userinterface::setupUI();

  powerOffDelay = 60000;  // 60 secondes
  
  bool changeConfig = false;
  while ((millis() < 1000UL) || BUTTON_PRESSED)
  {
    buzzer::testBuzzer(200UL);
    userinterface::refreshUI();
    if (BUTTON_LONG_PRESSED) //  && BUTTON_PRESSED ?
    {
      if (!changeConfig)
      {
        // Display [===]
        userinterface::displayBars();
        userinterface::resetEncoderButton();
        changeConfig = true;
      }
    }
  }

  // If button stay pressed long enough, switch to configuration mode
  if (changeConfig)
  {
    unsigned char address = 0;
    bool addressSelected = true;
    unsigned char value = EEPROM.read(address);
    userinterface::disp.write(address, value, true);
    movements::setMovements(value);
    userinterface::disp.setCursor(3);
    userinterface::disp.cursor();
    movements::powerOnMovements();
    while (changeConfig)
    {
      userinterface::refreshUI();
      if (BUTTON_RELEASED)
      {
        if (BUTTON_LONG_PRESSED)
        {
          EEPROM.update(address, value);
          changeConfig = false;
        }
        else
        {
          addressSelected = !addressSelected;
          userinterface::disp.setCursor(addressSelected?3:0);
        }
      }
      if (userinterface::isEncoderRotated())
      {
        if (addressSelected)
        {
          EEPROM.update(address, value);
          if (userinterface::encoderChangeValue(&address, configSize))
          {
            buzzer::clicBuzzer();
          }
          value = EEPROM.read(address);
        }
        else
        {
          if (userinterface::encoderChangeValue(&value, 255))
          {
            buzzer::clicBuzzer();
          }
        }
        userinterface::disp.write(address, value, true);
        switch (address)
        {
          case 0:
          case 1:
            movements::setMovements(value);
            break;
          default:
            break;
        }
      }
    } // while changeConfig
    movements::powerOffMovements();
  } // if changeConfig

  EEPROM.get(0, config);
  movements::stepsRemaining = config.steps_init;
  movements::speed = config.speed_init;
  userinterface::longPressDelay = (unsigned long)config.delay_longpress * 100UL;
  powerOffDelay = (unsigned long)config.delay_off * 1000UL;
  setTimeout = (unsigned long)config.delay_set * 100UL;
#ifdef DEBUG_SER
  Serial.println("Steps: " + String(movements::stepsRemaining));
  Serial.println("Speed: " + String(movements::speed) + " steps/min");
  Serial.println("Long press: " + String(double(userinterface::longPressDelay) / 1000.0) + " s");
  Serial.println("Auto power off: " + String(powerOffDelay));
  Serial.println("Set timeout: " + String(setTimeout));
#endif

  userinterface::displayClear();
  userinterface::displaySteps();
  userinterface::resetEncoder();
  stateMachine::setupStateMachine();
}

/*****************************************************************************
 * Main loop -----------------------------------------------------------------
 *****************************************************************************/
void loop() {
  userinterface::refreshUI();
  stateMachine::doState();
  userinterface::resetEncoderPosition();
}
