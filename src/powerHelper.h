#pragma once

#include "globals.h"
#include <avr/sleep.h>
#include <avr/power.h>

/// Methods to control power
namespace power
{
const uint8_t pinPower = 12;
const uint8_t pinWakeUp = 2; // INT0 = ENCS

///  Initialize power
void setupPower()
{
      pinMode(pinPower, OUTPUT);
      digitalWrite(pinPower, HIGH);
}

/// Power ON the external components
void powerOn()
{
    digitalWrite(pinPower, HIGH);
    delay(10);
}

/// Power OFF the external components
void powerOff()
{
    digitalWrite(pinPower, LOW);
}

/// Interrupt called at wakeup
void wakeUpInterrupt() {
  // cancel sleep as a precaution
  sleep_disable();
  // precautionary while we do other stuff
  detachInterrupt(INT0);
}

/// Go to sleep and wait to wakeup
void goToSleepAndWaitWakeUp()
{
    buzzer::muteBuzzer();
    movements::powerOffMovements();
    power::powerOff(); // Power off external components

    analogReference(DEFAULT);
    uint8_t old_ADCSRA = ADCSRA;
    ADCSRA = 0; // disable ADC
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    
    // Do not interrupt before we go to sleep, or the
    // ISR will detach interrupts and we won't wake.
    noInterrupts();

    power_all_disable();  // turn off various modules

    // will be called when INT0 (=PIN_ENCS) goes low
    attachInterrupt(INT0, wakeUpInterrupt, FALLING);
    EIFR = bit (INT0);  // clear flag for interrupt 0 or 1

    // turn off brown-out enable in software
    // BODS must be set to one and BODSE must be set to zero within four clock cycles
    MCUCR = bit (BODS) | bit (BODSE);
    // The BODS bit is automatically cleared after three clock cycles
    MCUCR = bit (BODS); 

    builtinled::ledOn();
    // We are guaranteed that the sleep_cpu call will be done
    // as the processor executes the next instruction after
    // interrupts are turned on.
    interrupts ();  // one cycle
    sleep_cpu ();   // one cycle


    // Here we are sleeping --- Zzzz Zzzz Zzzz Zzzz Zzzz Zzzz Zzzz
    // Waiting RESET or interrupt on INT0 (pin 2 = PIN_ENCS)


    builtinled::ledOff();

    power_all_enable ();   // enable modules again
    ADCSRA = old_ADCSRA;   // re-enable ADC conversion

    power::powerOn();  // Power on external components
}
} // namespace power
