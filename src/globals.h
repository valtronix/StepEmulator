#pragma once

#include <Arduino.h>

#define BUTTON_PRESSED (userinterface::encbtn.isPressed())
#define BUTTON_RELEASED (userinterface::encbtn.isReleased())
#define BUTTON_LONG_PRESSED (userinterface::encbtn.getPressedDuration() > userinterface::longPressDelay)
#define USER_INTERACTION_DONE userinterface::lastUserInteractionAt = millis();
#define LAST_USER_INTERACTION_DELAY (millis() - userinterface::lastUserInteractionAt)
#define BLANK_SCREEN userinterface::disp.noDisplay();
#define UNBLANK_SCREEN userinterface::disp.display();
