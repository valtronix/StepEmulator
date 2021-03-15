#include <Button.h>

/*
 * Buttons example
 */

#define PIN_BTN1 8
#define PIN_BTN2 7
#define PIN_LED1 3
#define PIN_LED2 5
#define PIN_LED3 6

Button btn1(PIN_BTN1);
Button btn2(PIN_BTN2, false);

#define LED_MAX 3

unsigned char ledSelected;
unsigned char leds[LED_MAX];

void setup() {
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  pinMode(PIN_LED3, OUTPUT);
  ledSelected = 0;
  leds[0] = 0;
  leds[1] = 0;
  leds[2] = 0;
}

void loop() {
  if (btn1.isReleased())
  {
      ledSelected = (ledSelected + 1) % LED_MAX;
  }
  if (btn2.isPressed())
  {
      unsigned long duration = btn2.getPressedDuration() / 10;
      leds[ledSelected] = duration>255?255:(unsigned char)duration;
  }
  analogWrite(PIN_LED1, leds[0]);
  analogWrite(PIN_LED2, leds[1]);
  analogWrite(PIN_LED3, leds[2]);
}
