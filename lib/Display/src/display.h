#ifndef DISPLAY_H
#define DISPLAY_H

#define DIGIT_MAX               5
#define CURSOR_MAX              3
#define CURSOR_BLINK_PERIOD   400

class Display
{

/*
 * Tables de caractères
 * Les segments sont dans l'ordre classiques: a b c d e f g dp
 * Caractères disponibles: 0 1 2 3 4 5 6 7 8 9 A B C D E F
 */
const unsigned char segments[17] = {
  0xfc, // 0: b11111100
  0x60, // 1: b01100000
  0xda, // 2: b11011010
  0xf2, // 3: b11110010
  0x66, // 4: b01100110
  0xb6, // 5: b10110110
  0xbe, // 6: b10111110
  0xe0, // 7: b11100000
  0xfe, // 8: b11111110
  0xf6, // 9: b11110110
  0xee, // A: b11101110
  0x3e, // b: b00111110
  0x1a, // c: b00011010
  0x7a, // d: b01111010
  0x9e, // E: b10011110
  0x8e, // F: b10001110
  0x02  // -: b00000010
};

private:
    unsigned char digits[DIGIT_MAX], digitNum;
    bool showScreen;
    unsigned char pin_en, pin_mr, pin_ck, pin_di, pin_st;
    unsigned char cursorPos;          // Position du curseur. Si le bit 7 est à 1, il n'est pas affiché.
    unsigned int valueDisplayed;
    unsigned int powerTen(unsigned char value);
    void update();
    bool leadingZeros;
    bool numberDisplayed;

public:
    bool blankScreen;
    Display(unsigned char pin_clock, unsigned char pin_data, unsigned char pin_strobe, unsigned char pin_reset, unsigned char pin_enable);
    ~Display();
    void writeDot(unsigned char digit, bool value);
    void showCursor();
    void hideCursor();
    bool isCursorVisible();
    unsigned char getCursor();
    void setCursor(unsigned char pos);
    void moveCursor(bool left);
    void displayNextDigit();
    void write(unsigned int value);
    void write(unsigned char address, unsigned char value, bool hex);
    void write(unsigned char pos, unsigned char digit);
    void write(unsigned char pos, unsigned char* digit, unsigned char len);
    void clear();
    void lampTest();
    void showLeadingZeros();
    void hideLeadingZeros();
};

#endif
