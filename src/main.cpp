#include <Arduino.h>
#include <Servo.h>

/*
 * Utilisation:
 *  - soit d'un compteur CD4017BE pour choisir l'afficheur actuel (mux) et de transistors S9014
 *  - soit d'un démultiplexeur CMOS MC74HCT138 pour choisir l'afficheur actuel (mux)
 * Utilisation d'un registre à décalage 4094 pour chaque segment des afficheurs.
 */


// #define DEBUG_SER
// #define USE_INT_SW
#define USE_CD4017

#define PIN_ENCS          2 // INT0
#define PIN_ENCA          3 // INT1
#define PIN_ENCB          4
#define PIN_SR_DI         5
#define PIN_SR_CK         6
#define PIN_SR_ST         7
#define PIN_DIGIT_ENA     9
#define PIN_SERVO        10
#define PIN_BUZZER       11
#ifdef USE_CD4017
#define PIN_CD4017_MR     8
#else
#define PIN_DIGIT_ADDR0  A0
#define PIN_DIGIT_ADDR1   8
#define PIN_DIGIT_ADDR2  12
#endif

Servo myservo;               // Crée un objet servo pour contrôler un servomoteur
unsigned int stepsRemaining; // Nombre de pas restant
uint16_t cursorPos;          // Position du curseur. Si le bit 7 est à 1, il n'est pas affiché.
bool blankScreen;            // Indique si l'affichage doit être éteint
unsigned long digitAt;
bool doit;
bool walking;
bool footUp;
bool leadingZeros;
unsigned long stepAt;
unsigned long buttonChangedAt, buttonPressedDuration;
bool buttonPressed, buttonReleased;
bool buttonWaitRelease;
bool btnShortPressed, btnLongPressed;

volatile bool btnPressed;
volatile unsigned long pressedAt;
volatile unsigned long rotatedAt;
volatile int rot;

#define POS_LOW                90
#define POS_HIGH              180

#define LONG_PRESS           1000

#define DEBOUNCE_TIME           5
#define SHORT_PRESS            10
#define DIGIT_TIMEOUT       10000
#define CURSOR_BLINK_PERIOD   400

#define SPEED_WALK            600
#define SPEED_RUN             150
#define STEPS_INIT           1000
#define STEPS_MAX           20000
#define DIGIT_MAX               5
#define CURSOR_MAX              3

#define DOT_STEP                0
#define DOT_SPEED               1
#define DOT_TIME                2
#define DOT_RESERVED            3
#define DOT_LONGPRESS           4

uint8_t digits[DIGIT_MAX], digitNum;

/*
 * Tables de caractères
 * Les segments sont dans l'ordre classiques: a b c d e f g dp
 * Caractères disponibles: 0 1 2 3 4 5 6 7 8 9 A B C D E F
 */
uint8_t segments[] = {
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



/*
 * Méthodes appelées dans des interruptions. *********************************
 */
// Méthode appelée quand le bouton est enfoncé
void onButtonPressed() {
  unsigned long interval = millis() - pressedAt;
#ifdef DEBUG_SER
  Serial.print(" vvv");
  Serial.print(interval);
#endif
  if (interval > SHORT_PRESS)
  {
    pressedAt = millis();
#ifdef DEBUG_SER
    Serial.write('S');
  }
  else
  {
    Serial.write(' ');
#endif
  }
#ifdef DEBUG_SER
  Serial.print("vvv ");
#endif
}

// Méthode appelée quand le bouton est relâché
void onButtonReleased() {
  unsigned long interval = millis() - pressedAt;
#ifdef DEBUG_SER
  Serial.print(" ^^^");
  Serial.print(interval);
#endif
  if (interval > LONG_PRESS)
  {
    btnLongPressed = true;
    btnShortPressed = false;
#ifdef DEBUG_SER
    Serial.write0('L');
#endif
  }
  else if (interval > SHORT_PRESS)
  {
    btnLongPressed = false;
    btnShortPressed = true;
#ifdef DEBUG_SER
    Serial.write('S');
  }
  else
  {
    Serial.write(' ');
#endif
  }
#ifdef DEBUG_SER
  Serial.print("^^^ ");
#endif
}

// Interruption déclenchée à quaque fois que le bouton change d'état
void onButtonChanged() {
  if ((millis() - pressedAt) > DEBOUNCE_TIME)
  { // Debounce
    btnPressed = digitalRead(PIN_ENCS);
    pressedAt = millis();
    // if (btnPressed)
    // {
    //   onButtonReleased();
    // }
    // else
    // {
    //   onButtonPressed();
    // }
  }
}

// Interruption déclenchée à chaque fois que l'encodeur est tourné
void onEncoderTurned() {
  if ((millis() - rotatedAt) > DEBOUNCE_TIME)
  { // Debounce
    rot += digitalRead(PIN_ENCB)?-1:1;
    rotatedAt = millis();
  }
}

// Vérifie l'état du bouton
void checkButton() {
  bool btn = digitalRead(PIN_ENCS);
  unsigned long now = millis();
  if ((btn == buttonPressed) && ((now - buttonChangedAt) > DEBOUNCE_TIME))
  {
    if (btn)
    {
      buttonPressed = false;
      buttonReleased = true;
      buttonPressedDuration = now - buttonChangedAt;
    }
    else
    {
      buttonPressed = true;
      buttonReleased = false;
      buttonPressedDuration = 0;
    }
    buttonChangedAt = now;
    buttonPressed = !btn;
  }
}


// Retourne 10 à la puissance value
unsigned int powerTen(uint8_t value)
{
  unsigned int pow = 1;
  while (value > 0)
  {
    pow *= 10;
    value--;
  }
  return pow;
}


/*
 * Méthodes de gestion des afficheurs. ****************************************
 */
// Met à jour la mémoire d'affichage avec la valeur de stepRemaining (conserve les points)
void updateDisplayMemory() {
  bool blank = !leadingZeros;
#ifdef DEBUG_SER
  Serial.print("Afficheur: [");
#endif
  uint16_t p = DIGIT_MAX;
  do {
    p--;
    digits[p] &= 0x01; // conserve l'état du point
    uint16_t digit = (stepsRemaining / powerTen(p)) % 10;
    if ((digit > 0) || (p == 0))
    {
      blank = false;
    }
    if (!blank)
    {
      digits[p] |= segments[digit];
    }
#ifdef DEBUG_SER
    Serial.write(c);
#endif
  } while (p > 0);
#ifdef DEBUG_SER
  Serial.print(']');
#endif
}

// Met à jour le point du chiffre indiqué
void writeDot(uint8_t digit, bool value) {
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
void displayNextDigit() {
  static bool showScreen;
  uint8_t digit;
  bool cursorBlinkOn = (millis() % CURSOR_BLINK_PERIOD) < (CURSOR_BLINK_PERIOD / 2);
  bool isDigit0 = (digitNum == 0);

  if (isDigit0)
  {
    showScreen = !blankScreen;
  }
  digitalWrite(PIN_DIGIT_ENA, LOW);
#ifdef USE_CD4017
  digitalWrite(PIN_CD4017_MR, isDigit0);
#else
  digitalWrite(PIN_DIGIT_ADDR0, bitRead(digitNum, 0));
  digitalWrite(PIN_DIGIT_ADDR1, bitRead(digitNum, 1));
  digitalWrite(PIN_DIGIT_ADDR2, bitRead(digitNum, 2));
#endif
  if (cursorBlinkOn && (cursorPos == digitNum))
  { // Curseur visible
    digit = 0x10;
  }
  else
  {
    digit = digits[digitNum];
  }
  // if ((digitNum == 4) && !(cursorPos & 0x80) && cursorBlinkOn)
  // {
  //   digit |= 0x01;
  // }
  for(int i=0; i<8; i++)
  {
    digitalWrite(PIN_SR_DI, bitRead(digit, i));
    digitalWrite(PIN_SR_CK, HIGH);
    digitalWrite(PIN_SR_CK, LOW);
  }
  digitalWrite(PIN_SR_ST, HIGH);
  digitalWrite(PIN_SR_ST, LOW);
  digitalWrite(PIN_DIGIT_ENA, showScreen);
  delay(2);
  digitNum++;
  digitNum %= DIGIT_MAX;
}



/*
 * Méthodes pour contrôler la mécanique qui simule les pas
 */
bool walk()
{
  if (stepsRemaining == 0)
  {
    return true;
  }
  else
  {
    if (!walking)
    { // On commence à marcher
      walking = true;
      myservo.attach(PIN_SERVO);
      footUp = true;
      stepAt = millis();
    }
    if ((millis() - stepAt) > SPEED_WALK)
    {
      footUp = !footUp;
      writeDot(DOT_STEP, footUp);
      if (footUp)
      {
        myservo.write(POS_HIGH);
      }
      else
      {
        myservo.write(POS_LOW);
        stepsRemaining--;
        if (stepsRemaining == 0)
        {
          walking = false;
          myservo.detach();
        }
        updateDisplayMemory();
      }
      stepAt = millis();
    }
  }
  return false;
}

/*****************************************************************************
 * Initialisation ------------------------------------------------------------
 *****************************************************************************/
void setup() {
  bool factoryDefault = true;
#ifdef DEBUG_SER
  Serial.begin(9600);
#endif
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  stepsRemaining = STEPS_INIT;
  doit = false;
  rot = 0;
  btnShortPressed = false;
  btnLongPressed = false;
  digitAt = 0;
  pressedAt = 0;
  rotatedAt = 0;
  walking = false;

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_ENCA, INPUT);
  pinMode(PIN_ENCB, INPUT);
  pinMode(PIN_ENCS, INPUT_PULLUP);
  
  pinMode(PIN_SR_CK, OUTPUT);
  pinMode(PIN_SR_DI, OUTPUT);
  pinMode(PIN_SR_ST, OUTPUT);
  digitalWrite(PIN_SR_CK, LOW);
  digitalWrite(PIN_SR_ST, LOW);
  digitalWrite(PIN_SR_DI, LOW);
#ifdef USE_CD4017
  pinMode(PIN_CD4017_MR, OUTPUT);
  digitalWrite(PIN_CD4017_MR, HIGH);
#else
  pinMode(PIN_DIGIT_ADDR0, OUTPUT);
  pinMode(PIN_DIGIT_ADDR1, OUTPUT);
  pinMode(PIN_DIGIT_ADDR2, OUTPUT);
#endif
  pinMode(PIN_DIGIT_ENA, OUTPUT);

  // Lamp test
  blankScreen = false;
  digitNum    = 0;
  cursorPos   = CURSOR_MAX | 0x80; // Curseur à la position max et invisible

  // Affiche 43210 mais 1 chiffre à la fois
  for (int i = 0; i < DIGIT_MAX; i++)
  {
    digits[i] = segments[i];
  }
  for (int i = 0; i < DIGIT_MAX; i++)
  {
    displayNextDigit();
    delay(200);
  }

  // Affiche tous les segments, points inclus et vérifie si le bouton est resté toujours enfoncé
  for (int i = 0; i < DIGIT_MAX; i++)
  {
    digits[i] = 0xff;
  }
  while (millis() < 2000)
  {
    if (digitalRead(PIN_ENCS))
    {
      factoryDefault = false;
    }
    displayNextDigit();
  }

  // Si le bouton est resté enfoncé tout le temps, on restaue la configuration d'usine
  if (factoryDefault)
  {
    // Affiche [===]
    for (int i = 1; i < DIGIT_MAX-1; i++)
    {
      digits[i] = 0x90; // segment haut et bas allumés
    }
    digits[0] = 0xf0;
    digits[DIGIT_MAX-1] = 0x9c;

    // Attends que le bouton soit relâché
    do {
      if (!digitalRead(PIN_ENCS))
      {
        digitAt = millis();
      }
      displayNextDigit();
    } while ((millis() - digitAt) < 100);
  }

  // Efface l'écran complètement, points inclus
  for (int i = 0; i < DIGIT_MAX; i++)
  {
    digits[i] = 0x00;
  }
  updateDisplayMemory();

  // Sur la carte Uno, les 2 seules PIN gérant les interruptions sont PIN2 et PIN3
  #ifdef USE_INT_SW
  attachInterrupt(digitalPinToInterrupt(PIN_ENCS), onButtonChanged, CHANGE);
  #endif
  attachInterrupt(digitalPinToInterrupt(PIN_ENCA), onEncoderTurned, FALLING);

  // timer1 (TCCR1) utilisé par le servo moteur
  digitalWrite(LED_BUILTIN, LOW);
  btnShortPressed = false;
  btnLongPressed = false;

  buttonWaitRelease = false;
  buttonPressed = false;
  buttonReleased = false;
  buttonChangedAt = 0;
}

/*****************************************************************************
 * Boucle principale----------------------------------------------------------
 *****************************************************************************/
void loop() {
  /*
  Si à l'arrêt:
    si le bouton est pressé de façon longue et:
      qu'il reste des pas: passer en mouvement
      qu'il ne reste plus de pas: ne rien faire
    si le bouton est pressé de façon courte: passer au chiffre de gauche (cycler si dernier chiffre)
    si l'encodeur est tourné dans le sens des aiguilles d'une montre: incrémenter le nombre de pas
    si l'encodeur est tourné dans le sens inverse: décrémenter le nombre de pas

  Si en mouvement:
    tant qu'il reste des pas: décompte 1 pas, sinon bip et passe à l'arrêt.
    si le bouton est pressé de façon courte, met en pause.
    si l'encodeur est tourné dans le sens des aiguilles d'une montre: incrémenter la vitesse
    si l'encodeur est tourné dans le sens inverse: décrémenter la vitesse
  */

  displayNextDigit();
  digitalWrite(LED_BUILTIN, doit);
#ifndef USE_INT_SW
  checkButton();
#endif

#ifdef DEBUG_SER
  Serial.print(doit);
  Serial.print(" - ");
  Serial.print(btnLongPressed);
  Serial.print(btnShortPressed);
  Serial.print(" - ");
  Serial.print((int)digitNum);
  Serial.print("/");
  Serial.print(rot);
  Serial.print(" - ");
  Serial.print(stepsRemaining);
  Serial.print(" ");
#endif
  if (doit)
  {
    if (walk())
    {
      leadingZeros = true;
      updateDisplayMemory();
      blankScreen = ((millis() % 500) < 250);
      if (blankScreen)
      {
        digitalWrite(PIN_BUZZER, HIGH);
      }
      else
      {
        digitalWrite(PIN_BUZZER, LOW);
      }
    }
#ifdef USE_INT_SW
    if (btnShortPressed || btnLongPressed)
#else
    if (buttonPressed)
#endif
    {
#ifndef USE_INT_SW
      buttonWaitRelease = true;
#endif
      doit = false;
      digitalWrite(PIN_BUZZER, LOW);
      blankScreen = false;
      leadingZeros = false;
      stepsRemaining = STEPS_INIT;
      updateDisplayMemory();
      cursorPos = CURSOR_MAX | 0x80; // Cache le curseur
#ifdef USE_INT_SW
      btnShortPressed = false;
      btnLongPressed = false;
#endif
    }
  }
  else
  {
#ifdef USE_INT_SW
    if (btnLongPressed)
    {
      if (stepsRemaining > 0)
      {
        doit = true;
        cursorPos = CURSOR_MAX;
      }
      cursorPos |= 0x80;  // Cache le curseur
      btnLongPressed = false;
    }
    else if (btnShortPressed)
    {
      digitAt = millis();
      if (cursorPos & 0x80)
      {
        cursorPos &= 0x7f;  // Affiche le curseur
      }
      else
      {
        if (cursorPos > 0)
        { 
          cursorPos--;
        }
        else
        {
          cursorPos = CURSOR_MAX;
        }
      }
      btnShortPressed = false;
    }
#else
    if (buttonReleased)
    {
      if (buttonWaitRelease)
      {
        buttonWaitRelease = false;
      }
      else if (buttonPressedDuration > LONG_PRESS)
      {
        if (stepsRemaining > 0)
        {
          doit = true;
          cursorPos = CURSOR_MAX;
        }
        cursorPos |= 0x80;  // Cache le curseur
      }
      else
      {
        digitAt = millis();
        if (cursorPos & 0x80)
        {
          cursorPos &= 0x7f;  // Affiche le curseur
        }
        else
        {
          if (cursorPos > 0)
          { 
            cursorPos--;
          }
          else
          {
            cursorPos = CURSOR_MAX;
          }
        }
      }
      buttonReleased = false;
      writeDot(DOT_LONGPRESS, false);
    }
#endif
    else if ((rot != 0) && !(cursorPos & 0x80))
    { // Si encodeur tourné et curseur affiché
      digitAt = millis();
      int r = rot;
      unsigned int stepdiff = (unsigned int)abs(r);
      unsigned int rank = powerTen(cursorPos);
      if (stepdiff > (STEPS_MAX / rank))
      {
        stepdiff = STEPS_MAX;
      }
      else
      {
        stepdiff *= rank;
      }
      if (r < 0)
      {
        if (stepsRemaining < stepdiff)
        {
          stepsRemaining = 0;
        }
        else
        {
          stepsRemaining -= stepdiff;
        }
        updateDisplayMemory();
      }
      else 
      { // r > 0
        if ((STEPS_MAX - stepsRemaining) < stepdiff)
        {
          stepsRemaining = STEPS_MAX;
        }
        else
        {
          stepsRemaining += stepdiff;
        }
        updateDisplayMemory();
      }
    }
    else if (buttonPressed && ((millis() - buttonChangedAt) > LONG_PRESS))
    {
      writeDot(DOT_LONGPRESS, true);
    }
    if (!(cursorPos & 0x80) && ((millis() - digitAt) > DIGIT_TIMEOUT))
    { // Si curseur affiché et pas de rotation pendant longtemps
      cursorPos |= 0x80;  // Masque le curseur
    }
  }

  rot = 0;

#ifdef DEBUG_SER
  Serial.println();
  delay(100);
#endif
}
