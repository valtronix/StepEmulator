#ifndef BUTTON_H
#define BUTTON_H

#define DEBOUNCE_TIME 5

class Button
{
private:
    unsigned char pin_btn;
    unsigned long changedAt, pressedAt, releasedAt;
    bool buttonPressed, buttonReleased;
    bool oldStatus, isHandled;
    bool inverted;
public:
    Button(unsigned char pin);
    Button(unsigned char pin, bool inverted);
    ~Button();
    void check();
    bool isPressed();
    bool isReleased();
    void handled();
    unsigned long getPressedDuration();
};

#endif