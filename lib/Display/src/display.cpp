#include <Arduino.h>
#include "Display.h"

Display::Display(unsigned char pin_clock, unsigned char pin_data, unsigned char pin_strobe, unsigned char pin_reset, unsigned char pin_enable)
{
  digitNum = 0;
  blankScreen = false;
  cursorPos = 0x80;
  numberDisplayed = false;

  pin_ck = pin_clock;
  pin_di = pin_data;
  pin_st = pin_strobe;
  pin_mr = pin_reset;
  pin_en = pin_enable;

  showScreen = !blankScreen;
  pinMode(pin_ck, OUTPUT);
  pinMode(pin_di, OUTPUT);
  pinMode(pin_st, OUTPUT);
  pinMode(pin_mr, OUTPUT);
  pinMode(pin_en, OUTPUT);
  digitalWrite(pin_ck, LOW);
  digitalWrite(pin_di, LOW);
  digitalWrite(pin_st, LOW);
  digitalWrite(pin_en, LOW);
  digitalWrite(pin_mr, HIGH);
}

Display::~Display()
{
  clear();
  pinMode(pin_ck, INPUT);
  pinMode(pin_di, INPUT);
  pinMode(pin_st, INPUT);
  pinMode(pin_mr, INPUT);
  pinMode(pin_en, INPUT);
}

// Retourne 10 à la puissance value
unsigned int Display::powerTen(unsigned char value)
{
  unsigned int pow = 1;
  while (value > 0)
  {
    pow *= 10;
    value--;
  }
  return pow;
}

// Met à jour le point du chiffre indiqué
void Display::writeDot(unsigned char digit, bool value) {
  if (digit < DIGIT_MAX)
  {
    if (value)
    {
      digits[digit] |= 0x01;
    }
    else
    {
      digits[digit] &= 0xfe;
    }
  }
}

// Affiche le chiffre suivant en utilisant du multiplexage
void Display::displayNextDigit() {
  unsigned char digit;
  bool cursorBlinkOn = (millis() % CURSOR_BLINK_PERIOD) < (CURSOR_BLINK_PERIOD / 2);
  bool isDigit0 = (digitNum == 0);

  if (isDigit0)
  {
    showScreen = !blankScreen;
  }
  // Switch off the current digit
  digitalWrite(pin_en, LOW);
  digitalWrite(pin_mr, isDigit0);
  if (cursorBlinkOn && (cursorPos == digitNum))
  { // Cursor visible
    digit = (digits[digitNum] & 0x01) | 0x10;
  }
  else
  {
    digit = digits[digitNum];
  }
  // Send all segments serially
  for(int i=0; i<8; i++)
  {
    digitalWrite(pin_di, bitRead(digit, i));
    digitalWrite(pin_ck, HIGH);
    digitalWrite(pin_ck, LOW);
  }
  // Strobe the shift register
  digitalWrite(pin_st, HIGH);
  digitalWrite(pin_st, LOW);
  // Switch on the current digit if activated
  digitalWrite(pin_en, showScreen);
  delay(2);
  digitNum++;
  digitNum %= DIGIT_MAX;
}

void Display::showCursor() {
  cursorPos &= 0x7f;
}

void Display::hideCursor() {
  cursorPos |= 0x80;
}

bool Display::isCursorVisible() {
  return !(cursorPos & 0x80);
}

void Display::setCursor(unsigned char pos) {
  cursorPos &= 0x80;
  cursorPos |= (pos % DIGIT_MAX) & 0x7f;
}

void Display::moveCursor(bool left) {
  showCursor();
  if (left)
  {
    cursorPos++;
  }
  else
  {
    cursorPos--;
  }
  cursorPos %= CURSOR_MAX + 1;
}

unsigned char Display::getCursor() {
  return cursorPos & 0x7f;
}

void Display::clear() {
  unsigned char p = DIGIT_MAX;
  do {
    p--;
    digits[p] = 0;
  } while (p > 0);
  numberDisplayed = false;
}

void Display::lampTest() {
  unsigned char p = DIGIT_MAX;
  do {
    p--;
    digits[p] = 0xff;
  } while (p > 0);
  numberDisplayed = false;
}

void Display::write(unsigned int value) {
  valueDisplayed = value;
  numberDisplayed = true;
  update();
}

void Display::write(unsigned char address, unsigned char value, bool hex) {
  numberDisplayed = false;
  unsigned char p = DIGIT_MAX;
  if (hex)
  {
    digits[--p] = segments[address >> 4];
    digits[--p] = segments[address & 0x0f];
  }
  else
  {
    digits[--p] = segments[(address / 10) % 10];
    digits[--p] = segments[address % 10] | 0x01;
  }
  digits[--p] = segments[(value / 100) % 10];
  digits[--p] = segments[(value / 10) % 10];
  digits[--p] = segments[value % 10];
}

void Display::write(unsigned char pos, unsigned char digit) {
  pos = pos % DIGIT_MAX;
  digits[pos] = digit;
  numberDisplayed = false;
}

void Display::write(unsigned char pos, unsigned char* digit, unsigned char len) {
  pos = pos % DIGIT_MAX;
  while ((len > 0) && (pos < DIGIT_MAX))
  {
    digits[pos] = digit[pos];
    pos++;
  }
  numberDisplayed = false;
}

void Display::update() {
  bool blank = !leadingZeros;
  unsigned char p = DIGIT_MAX;
  do {
    p--;
    digits[p] &= 0x01; // conserve l'état du point
    unsigned char digit = (valueDisplayed / powerTen(p)) % 10;
    if ((digit > 0) || (p == 0))
    {
      blank = false;
    }
    if (!blank)
    {
      digits[p] |= segments[digit];
    }
  } while (p > 0);
}

void Display::showLeadingZeros() {
  leadingZeros = true;
  if (numberDisplayed)
  {
    update();
  }
}

void Display::hideLeadingZeros() {
  leadingZeros = false;
  if (numberDisplayed)
  {
    update();
  }
}

