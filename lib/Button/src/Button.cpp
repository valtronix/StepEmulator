#include <Arduino.h>
#include "Button.h"

Button::Button(unsigned char pin)
{
    pin_btn = pin;
    pinMode(pin_btn, INPUT_PULLUP);
    isHandled = false;
    pressedAt = 0;
    releasedAt = 0;
    changedAt = 0;
    buttonPressed = false;
    buttonReleased = false;
    oldStatus = false;
}

Button::~Button()
{
    pinMode(pin_btn, INPUT);
}

void Button::check()
{
    bool btn = !digitalRead(pin_btn);
    unsigned long now = millis();
    if ((btn != oldStatus) && ((now - changedAt) > DEBOUNCE_TIME))
    {
        if (btn)
        {
            buttonPressed = true;
            buttonReleased = false;
            pressedAt = now;
            releasedAt = 0;
        }
        else
        {
            buttonPressed = false;
            buttonReleased = !isHandled;
            releasedAt = now;
            isHandled = false;
        }
        changedAt = now;
        oldStatus = btn;
    }
}

bool Button::isPressed()
{
    return buttonPressed;
}

bool Button::isReleased()
{
    if (buttonReleased)
    {
        buttonReleased = false;
        return true;
    }
    else
    {
        return false;
    }
}

void Button::handled()
{
    if (buttonPressed)
    {
        isHandled = true;
    }
    else
    {
        buttonReleased = false;
    }
}

unsigned long Button::getPressedDuration()
{
    if (releasedAt != 0)
    {
        return releasedAt - pressedAt;
    }
    else
    {
        return millis() - pressedAt;
    }
}
