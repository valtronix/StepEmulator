#pragma once

#include "globals.h"

/// Methods for the main state macheine
namespace stateMachine {

/// List of states
enum States : int8_t {
  PowerOff = -1,
  Init = 0,
  SetSteps,
  AdjustSteps,
  Emulate,
  Paused,
  ChangeSpeed,
  Finished
} state;

/// Initialize the state machine
void setupStateMachine() {
    state = States::Init;
}

/// Must be called to change the state
void changeState(States newstate) {
  switch (newstate) {
    case SetSteps:
      userinterface::displaySteps();
      userinterface::disp.setCursor(3);
      break;
    case AdjustSteps:
      // case ChangeSpeed:
      userinterface::disp.cursor();
      break;
    case Emulate:
      // disp.setCursor(2);
      break;
    case PowerOff:
      buzzer::muteBuzzer();
      userinterface::displayOff();
#ifdef DEBUG_SER
      Serial.println("**Power Off");
#endif
      break;
    default:
      break;
  }
  USER_INTERACTION_DONE
  state = newstate;
}

/// Do action corresponding to the actual state
void doState()
{
  switch (state) {
    case States::Init:
#ifdef DEBUG_SER
      Serial.println("**Init");
#endif
      analogReference(INTERNAL);
      buzzer::muteBuzzer();
      if (BUTTON_PRESSED)
      {
        userinterface::resetEncoderButton();
      }
      movements::stepsRemaining = config.steps_init;
      movements::speed = config.speed_init;
      userinterface::displayClear();
      userinterface::disp.setCursor(3);
      userinterface::displaySteps();
      UNBLANK_SCREEN
      changeState(States::SetSteps);
      break;
    case States::SetSteps:
      if (BUTTON_RELEASED)
      {
        if (BUTTON_LONG_PRESSED)
        {
          changeState(States::Emulate);
        }
        else
        {
          changeState(States::AdjustSteps);
        }
      }
      else if (LAST_USER_INTERACTION_DELAY > powerOffDelay)
      {
        changeState(States::PowerOff);
      }
      break;
    case States::AdjustSteps:
      if (BUTTON_RELEASED)
      {
        if (BUTTON_LONG_PRESSED)
        {
          config.steps_init = movements::stepsRemaining;
          // TODO: save to EEPROM
          changeState(States::Emulate);
        }
        else
        {
          userinterface::disp.moveCursor(false);
        }
        USER_INTERACTION_DONE 
      }
      else if (userinterface::isEncoderRotated())
      {
        int8_t r = userinterface::rot;
        unsigned int stepdiff = (unsigned int)abs(r);
        unsigned int rank = 1;
        unsigned char p = userinterface::disp.getCursor();
        while (p > 0)
        {
          rank *= 10;
          p--;
        }
        if (stepdiff > (config.steps_max / rank))
        {
          stepdiff = config.steps_max;
        }
        else
        {
          stepdiff *= rank;
        }
        if (r < 0)
        {
          if ((movements::stepsRemaining - config.steps_min) < stepdiff)
          {
            movements::stepsRemaining = config.steps_min;
          }
          else
          {
            movements::stepsRemaining -= stepdiff;
          }
        }
        else 
        { // r > 0
          if ((config.steps_max - movements::stepsRemaining) < stepdiff)
          {
            movements::stepsRemaining = config.steps_max;
          }
          else
          {
            movements::stepsRemaining += stepdiff;
          }
        }
        userinterface::displaySteps();
        USER_INTERACTION_DONE
      }
      else if (LAST_USER_INTERACTION_DELAY > setTimeout)
      { // If no user interaction
        userinterface::disp.noCursor();
        changeState(States::SetSteps);
      }
      break;
    case States::Emulate:
      if (BUTTON_RELEASED)
      {
        if (BUTTON_LONG_PRESSED)
        {
          changeState(States::Init);
        }
        else
        {
          changeState(States::Paused);
        }
      }
      else if (userinterface::isEncoderRotated())
      {
        userinterface::displaySpeed();
        // disp.setCursor(0);
        // disp.cursor();
        changeState(States::ChangeSpeed);
        USER_INTERACTION_DONE
      }
      else
      {
        if (movements::walk())
        {
          userinterface::disp.leadingZeros();
          changeState(States::Finished);
        }
      }
      break;
    case States::ChangeSpeed:
      if (BUTTON_RELEASED)
      {
        if (BUTTON_LONG_PRESSED)
        {
          changeState(States::Init);
        }
        else
        {
          userinterface::disp.noCursor();
          userinterface::displaySteps();
          changeState(States::Paused);
        }
      }
      else
      {
        if (movements::walk())
        {
          userinterface::disp.noCursor();
          userinterface::disp.leadingZeros();
          userinterface::displaySteps();
          changeState(States::Finished);
        }
        else if (userinterface::isEncoderRotated())
        {
          unsigned int r = (unsigned int)abs(userinterface::rot);
          if (userinterface::rot > 0)
          {
            if ((movements::speed + r) > config.speed_max)
            {
              r = config.speed_max - movements::speed;
              buzzer::clicBuzzer();
            }
            movements::speed += r;
          }
          else
          {
            if ((movements::speed - r) < config.speed_min)
            {
              r = movements::speed - config.speed_min;
              buzzer::clicBuzzer();
            }
            movements::speed -= r;
          }
          userinterface::displaySpeed();
          USER_INTERACTION_DONE
        }
        else if (LAST_USER_INTERACTION_DELAY > setTimeout)
        { // If no user interaction
          userinterface::disp.noCursor();
          userinterface::displaySteps();
          changeState(States::Emulate);
        }
      }
      break;
    case States::Paused:
      if (BUTTON_RELEASED)
      {
        if (BUTTON_LONG_PRESSED)
        {
          changeState(States::Init);
        }
        else
        {
          UNBLANK_SCREEN
          changeState(States::Emulate);
        }
      }
      else
      {
        if ((millis() % 500) < 250)
        {
          BLANK_SCREEN
        }
        else
        {
          UNBLANK_SCREEN
        }
      }
      break;
    case States::Finished:
      if (BUTTON_RELEASED)
      {
        changeState(States::Init);
      }
      else if (LAST_USER_INTERACTION_DELAY > powerOffDelay)
      {
        changeState(States::PowerOff);
      }
      else
      {
        if ((millis() % 500) < 250)
        {
          BLANK_SCREEN
          buzzer::ringBuzzer();
        }
        else
        {
          UNBLANK_SCREEN
          buzzer::muteBuzzer();
        }
      }
      break;
    case States::PowerOff:
      if (BUTTON_PRESSED)
      {
        userinterface::resetEncoderButton();
        power::powerOn();
        changeState(States::Init);
      }
      else if (LAST_USER_INTERACTION_DELAY > (unsigned long)(config.delay_offmsg * 100))
      {
        BLANK_SCREEN
        if (!userinterface::disp.isDisplay())
        {
            power::goToSleepAndWaitWakeUp();
            changeState(States::Init);
        }
      }
      break;
  }
}
} // namespace stateMachine
