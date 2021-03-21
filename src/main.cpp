#include <Arduino.h>
#include <Servo.h>
#include <EEPROM.h>

#include <Display.h>
#include <Button.h>

/*
 * Utilisation:
 *  - soit d'un compteur CD4017BE pour choisir l'afficheur actuel (mux) et de transistors S9014
 *  - soit d'un démultiplexeur CMOS MC74HCT138 pour choisir l'afficheur actuel (mux)
 * Utilisation d'un registre à décalage 4094 pour chaque segment des afficheurs.
 */


// #define DEBUG_SER

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
Button encbtn(PIN_ENCS);

unsigned int stepsRemaining; // Nombre de pas restant
unsigned long digitAt;
bool doit;
bool walking;
bool footUp;
unsigned long stepAt;

volatile unsigned long rotatedAt;
volatile int rot;

#define POS_LOW                90
#define POS_HIGH              180

#define LONG_PRESS           1000
#define DEBOUNCE_TIME           5
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

struct MyConfig_t {
  unsigned char pos_stepdown, pos_stepup;
  unsigned int steps_init;
  unsigned int walk_speed;
  unsigned int run_speed;
  unsigned char walk_ratio;
  unsigned char run_ratio;
};

MyConfig_t config;

/*
 * Méthodes appelées dans des interruptions. *********************************
 */
// Interruption déclenchée à chaque fois que l'encodeur est tourné
void onEncoderTurned() {
  if ((millis() - rotatedAt) > DEBOUNCE_TIME)
  { // Debounce
    rot += digitalRead(PIN_ENCB)?-1:1;
    rotatedAt = millis();
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
    if ((millis() - stepAt) > config.walk_speed)
    {
      footUp = !footUp;
      disp.writeDot(DOT_STEP, footUp);
      if (footUp)
      {
        myservo.write(config.pos_stepup);
      }
      else
      {
        myservo.write(config.pos_stepdown);
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
  const static unsigned char configSize = sizeof(MyConfig_t) - 1;
#ifdef DEBUG_SER
  Serial.begin(9600);
#endif
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  doit = false;
  rot = 0;
  digitAt = 0;
  rotatedAt = 0;
  walking = false;

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, HIGH);
  pinMode(PIN_ENCA, INPUT);
  pinMode(PIN_ENCB, INPUT);
  
  // Lamp test
  disp.lampTest();

  // Sur la carte Uno, les 2 seules PIN gérant les interruptions sont PIN2 et PIN3
  attachInterrupt(digitalPinToInterrupt(PIN_ENCA), onEncoderTurned, FALLING);

  bool changeConfig = false;
  while ((millis() < 1000) || encbtn.isPressed())
  {
    digitalWrite(PIN_BUZZER, millis() <= 250);
    disp.displayNextDigit();
    encbtn.check();
    if (encbtn.isPressed() && (encbtn.getPressedDuration() > LONG_PRESS))
    {
      if (!changeConfig)
      {
        // Affiche [===]
        disp.write(0, 0xf0);
        disp.write(DIGIT_MAX-1, 0x9c);
        for (int i = 1; i < DIGIT_MAX-1; i++)
        {
          disp.write(i, 0x90); // segment haut et bas allumés
        }
        encbtn.handled();
        changeConfig = true;
      }
    }
  }

  // Si le bouton est resté enfoncé assez longtemps, on passe en mode configuration
  if (changeConfig)
  {
    unsigned char address = 0;
    bool addressSelected = true;
    unsigned char value = EEPROM.read(address);
    disp.write(address, value, true);
    myservo.write(value);
    disp.setCursor(3);
    disp.showCursor();
    myservo.attach(PIN_SERVO);
    while (changeConfig)
    {
      disp.displayNextDigit();
      encbtn.check();
      if (encbtn.isReleased())
      {
        if (encbtn.getPressedDuration() > LONG_PRESS)
        {
          EEPROM.update(address, value);
          changeConfig = false;
        }
        else
        {
          addressSelected = !addressSelected;
          disp.setCursor(addressSelected?3:0);
        }
      }
      if (rot != 0)
      {
        if (addressSelected)
        {
          EEPROM.update(address, value);
          if ((rot < 0) && (address < -rot))
          {
            address = 0;
            digitalWrite(PIN_BUZZER, HIGH);
            delay(1);
            digitalWrite(PIN_BUZZER, LOW);
          }
          else if ((rot > 0) && ((int)(configSize - address) < rot))
          {
            address = configSize;
            digitalWrite(PIN_BUZZER, HIGH);
            delay(1);
            digitalWrite(PIN_BUZZER, LOW);
          }
          else
          {
            address += rot;
          }
          value = EEPROM.read(address);
        }
        else
        {
          if ((rot < 0) && (value < -rot))
          {
            value = 0;
            digitalWrite(PIN_BUZZER, HIGH);
            delay(1);
            digitalWrite(PIN_BUZZER, LOW);
          }
          else if ((rot > 0) && ((255 - value) < rot))
          {
            value = 255;
            digitalWrite(PIN_BUZZER, HIGH);
            delay(1);
            digitalWrite(PIN_BUZZER, LOW);
          }
          else
          {
            value += rot;
          }
        }
        disp.write(address, value, true);
        switch (address)
        {
          case 0:
          case 1:
            myservo.write(value);
            break;
          default:
            break;
        }
        rot = 0;
      }
    }
    myservo.detach();
  }

  EEPROM.get(0, config);
  stepsRemaining = config.steps_init;

  // Efface l'écran complètement, points inclus
  disp.clear();
  disp.hideCursor();
  disp.hideLeadingZeros();
  disp.write(stepsRemaining);

  // timer1 (TCCR1) utilisé par le servo moteur
  digitalWrite(LED_BUILTIN, LOW);
  encbtn.handled();
  rot = 0;
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
  encbtn.check();
  digitalWrite(LED_BUILTIN, doit);

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

    if (encbtn.isPressed())
    {
      encbtn.handled();
      doit = false;
      if (stepsRemaining == 0)
      {
        digitalWrite(PIN_BUZZER, LOW);
        stepsRemaining = config.steps_init;
        disp.blankScreen = false;
        disp.hideLeadingZeros();
        disp.hideCursor();
        disp.setCursor(3);
        disp.write(stepsRemaining);
      }
    }

  }
  else
  {
    if (encbtn.isReleased())
    {
      if (encbtn.getPressedDuration() > LONG_PRESS)
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
      disp.writeDot(DOT_LONGPRESS, false);
    }
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
    else if (encbtn.isPressed() && (encbtn.getPressedDuration() > LONG_PRESS))
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
