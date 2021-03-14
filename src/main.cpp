#include <Arduino.h>
#include <Servo.h>
#include <Display.h>

/*
 * Utilisation:
 *  - soit d'un compteur CD4017BE pour choisir l'afficheur actuel (mux) et de transistors S9014
 *  - soit d'un démultiplexeur CMOS MC74HCT138 pour choisir l'afficheur actuel (mux)
 * Utilisation d'un registre à décalage 4094 pour chaque segment des afficheurs.
 */


// #define DEBUG_SER
// #define USE_INT_SW

#define PIN_ENCS          2 // INT0
#define PIN_ENCA          3 // INT1
#define PIN_ENCB          4
#define PIN_SR_DI         5
#define PIN_SR_CK         6
#define PIN_SR_ST         7
#define PIN_CD4017_MR     8
#define PIN_DIGIT_ENA     9
#define PIN_SERVO        10
#define PIN_BUZZER       11

Servo myservo;               // Crée un objet servo pour contrôler un servomoteur
Display disp(PIN_SR_CK, PIN_SR_DI, PIN_SR_ST, PIN_CD4017_MR, PIN_DIGIT_ENA);

unsigned int stepsRemaining; // Nombre de pas restant
unsigned long digitAt;
bool doit;
bool walking;
bool footUp;
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

#define SPEED_WALK            600
#define SPEED_RUN             150
#define STEPS_INIT           1000
#define STEPS_MAX           20000

#define DOT_STEP                0
#define DOT_SPEED               1
#define DOT_TIME                2
#define DOT_RESERVED            3
#define DOT_LONGPRESS           4


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
      disp.writeDot(DOT_STEP, footUp);
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
        disp.write(stepsRemaining);
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
  
  // Lamp test
  disp.lampTest();

  while (millis() < 2000)
  {
    if (digitalRead(PIN_ENCS))
    {
      factoryDefault = false;
    }
    disp.displayNextDigit();
  }

  // Si le bouton est resté enfoncé tout le temps, on restaue la configuration d'usine
  if (factoryDefault)
  {
    // Affiche [===]
    disp.write(0, 0xf0);
    disp.write(DIGIT_MAX-1, 0x9c);
    for (int i = 1; i < DIGIT_MAX-1; i++)
    {
      disp.write(i, 0x90); // segment haut et bas allumés
    }

    // Attends que le bouton soit relâché
    do {
      if (!digitalRead(PIN_ENCS))
      {
        digitAt = millis();
      }
      disp.displayNextDigit();
    } while ((millis() - digitAt) < 100);
  }

  // Efface l'écran complètement, points inclus
  disp.clear();
  disp.hideLeadingZeros();
  disp.write(stepsRemaining);

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

  disp.displayNextDigit();
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
      disp.showLeadingZeros();
      disp.blankScreen = ((millis() % 500) < 250);
      if (disp.blankScreen)
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
      disp.blankScreen = false;
      disp.hideLeadingZeros();
      stepsRemaining = STEPS_INIT;
      disp.hideCursor();
      disp.setCursor(3);
      disp.write(stepsRemaining);
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
          disp.setCursor(CURSOR_MAX);
        }
        disp.hideCursor();
      }
      else
      {
        digitAt = millis();
        disp.moveCursor(false);
      }
      buttonReleased = false;
      disp.writeDot(DOT_LONGPRESS, false);
    }
#endif
    else if ((rot != 0) && disp.isCursorVisible())
    { // Si encodeur tourné et curseur affiché
      digitAt = millis();
      int r = rot;
      unsigned int stepdiff = (unsigned int)abs(r);
      unsigned int rank = 1;
      unsigned char p = disp.getCursor();
      while (p > 0)
      {
        rank *= 10;
        p--;
      }
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
        disp.write(stepsRemaining);
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
        disp.write(stepsRemaining);
      }
    }
    else if (buttonPressed && ((millis() - buttonChangedAt) > LONG_PRESS))
    {
      disp.writeDot(DOT_LONGPRESS, true);
    }
    if (disp.isCursorVisible() && ((millis() - digitAt) > DIGIT_TIMEOUT))
    { // Si curseur affiché et pas de rotation pendant longtemps
      disp.hideCursor();  // Masque le curseur
    }
  }

  rot = 0;

#ifdef DEBUG_SER
  Serial.println();
  delay(100);
#endif
}
