#include <Arduino.h>
#include <Servo.h>
#include <EEPROM.h>

#include <Display.h>
#include <Button.h>

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

Servo myservo;
Display disp(PIN_SR_CK, PIN_SR_DI, PIN_SR_ST, PIN_CD4017_MR, PIN_DIGIT_ENA);
Button encbtn(PIN_ENCS);

unsigned int stepsRemaining;        // Number of remaining steps
unsigned int speed;                 // Actual speed
unsigned long digitAt;              // Last time digits was changed

volatile char rot;

#define DEBOUNCE_TIME           5
#define LONG_PRESS           1000
#define DIGIT_TIMEOUT       10000

#define DOT_STEP                0
#define DOT_SPEED               1
#define DOT_TIME                2
#define DOT_RESERVED            3
#define DOT_LONGPRESS           4

struct MyConfig_t {
  unsigned char pos_stepdown;       // Servo motor position when foot is down
  unsigned char pos_stepup;         // Servo motor position when foot is up
  unsigned int steps_init;          // Number of steps by default (at startup)
  unsigned int speed_init;          // Default speed (at startup)
  unsigned int steps_max;           // Number of steps at maximum
  unsigned int speed_min;           // Lowest speed
  unsigned int speed_max;           // Highest speed
  unsigned char walk_ratio;         // Lowest ratio (step up/down)
  unsigned char run_ratio;          // Highest ratio (step up/down)
};

MyConfig_t config;
const MyConfig_t defaultConfig = {
  90,
  180,
  1000,
  400,
  20000,
  150,
  800,
  50,
  50
};


enum States : char {
  Poweroff = -1,
  Init = 0,
  SetSteps,
  AdjustSteps,
  Emulate,
  Paused,
  ChangeSpeed,
  Finished
};

States state;

void buzzer_clic() {
  bool clic = digitalRead(PIN_BUZZER);
  digitalWrite(PIN_BUZZER, !clic);
  delay(1);
  digitalWrite(PIN_BUZZER, clic);
}


/*
 * Methods called from interruptions *****************************************
 */
// Interrupt raised each time the encoder is turned
void onEncoderTurned() {
  volatile static unsigned long rotatedAt = 0;
  if ((millis() - rotatedAt) > DEBOUNCE_TIME)
  { // Debounce
    if (digitalRead(PIN_ENCB))
    {
      if (rot == -128)
      { // Overflow
        buzzer_clic();
      }
      else
      {
        rot--;
      }
    }
    else
    {
      if (rot == 127)
      { // Overflow
        buzzer_clic();
      }
      else
      {
        rot++;
      }
    }
    rotatedAt = millis();
  }
}


/*
 * Méthodes pour contrôler la mécanique qui simule les pas
 */
bool walk()
{
  static unsigned long stepAt = 0;
  static bool footUp = false;
  static bool walking = false;
  if (stepsRemaining == 0)
  {
    return true;
  }
  else
  {
    if (!walking)
    { // Start walking
      walking = true;
      myservo.attach(PIN_SERVO);
      footUp = false;
      stepAt = millis();
    }
    if ((millis() - stepAt) > speed)
    {
      footUp = !footUp;
      disp.writeDot(DOT_RESERVED, footUp);
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
        if (state == States::Emulate)
        {
          disp.write(stepsRemaining);
        }
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

  rot = 0;
  digitAt = 0;

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
            buzzer_clic();
          }
          else if ((rot > 0) && ((int)(configSize - address) < rot))
          {
            address = configSize;
            buzzer_clic();
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
            buzzer_clic();
          }
          else if ((rot > 0) && ((255 - value) < rot))
          {
            value = 255;
            buzzer_clic();
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
  speed = config.speed_init;

  // Efface l'écran complètement, points inclus
  disp.clear();
  disp.hideCursor();
  disp.hideLeadingZeros();
  disp.write(stepsRemaining);

  // timer1 (TCCR1) utilisé par le servo moteur
  digitalWrite(LED_BUILTIN, LOW);
  encbtn.handled();
  rot = 0;
  state = States::Init;
}

void SetState(States newstate) {
  switch (newstate) {
    case SetSteps:
      disp.write(stepsRemaining);
      disp.setCursor(3);
      break;
    case AdjustSteps:
    // case ChangeSpeed:
      disp.showCursor();
      digitAt = millis();
      break;
    case Emulate:
      // disp.setCursor(2);
      break;
    default:
      break;
  }
  state = newstate;
}

/*****************************************************************************
 * Main loop -----------------------------------------------------------------
 *****************************************************************************/
void loop() {
  disp.displayNextDigit();
  encbtn.check();

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

  switch (state) {
    case States::Init:
      digitalWrite(PIN_BUZZER, LOW);
      stepsRemaining = config.steps_init;
      disp.clear();
      disp.blankScreen = false;
      disp.hideLeadingZeros();
      disp.hideCursor();
      disp.setCursor(3);
      disp.write(stepsRemaining);
      disp.writeDot(DOT_STEP, true);
      SetState(States::SetSteps);
      break;
    case States::SetSteps:
      if (encbtn.isReleased())
      {
        if (encbtn.getPressedDuration() > LONG_PRESS)
        {
          SetState(States::Emulate);
        }
        else
        {
          SetState(States::AdjustSteps);
        }
      }
      /*
      else if ()
      {
        SetState(States::PowerOff);
      }
      */
      break;
    case States::AdjustSteps:
      if (encbtn.isReleased())
      {
        if (encbtn.getPressedDuration() > LONG_PRESS)
        {
          config.steps_init = stepsRemaining;
          // TODO: save to EEPROM
          SetState(States::Emulate);
        }
        else
        {
          disp.moveCursor(false);
        }
        digitAt = millis();
      }
      else if (rot)
      {
        int r = rot;
        unsigned int stepdiff = (unsigned int)abs(r);
        unsigned int rank = 1;
        unsigned char p = disp.getCursor();
        while (p > 0)
        {
          rank *= 10;
          p--;
        }
        if (stepdiff > (config.steps_max / rank))
        {
          stepdiff = config.steps_max;
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
        }
        else 
        { // r > 0
          if ((config.steps_max - stepsRemaining) < stepdiff)
          {
            stepsRemaining = config.steps_max;
          }
          else
          {
            stepsRemaining += stepdiff;
          }
        }
        disp.write(stepsRemaining);
        digitAt = millis();
      }
      else if ((millis() - digitAt) > DIGIT_TIMEOUT)
      { // If no user interaction
        disp.hideCursor();
        SetState(States::SetSteps);
      }
      break;
    case States::Emulate:
      if (encbtn.isReleased())
      {
        if (encbtn.getPressedDuration() > LONG_PRESS)
        {
          SetState(States::Init);
        }
        else
        {
          SetState(States::Paused);
        }
      }
      else if (rot)
      {
        disp.writeDot(DOT_STEP, false);
        disp.writeDot(DOT_SPEED, true);
        disp.write(speed);
        // disp.setCursor(0);
        // disp.showCursor();
        SetState(States::ChangeSpeed);
        digitAt = millis();
      }
      else
      {
        if (walk())
        {
          disp.showLeadingZeros();
          SetState(States::Finished);
        }
      }
      break;
    case States::ChangeSpeed:
      if (encbtn.isReleased())
      {
        if (encbtn.getPressedDuration() > LONG_PRESS)
        {
          SetState(States::Init);
        }
        else
        {
          disp.hideCursor();
          disp.write(stepsRemaining);
          disp.writeDot(DOT_SPEED, false);
          disp.writeDot(DOT_STEP, true);
          SetState(States::Paused);
        }
      }
      else
      {
        if (walk())
        {
          disp.hideCursor();
          disp.showLeadingZeros();
          disp.write(stepsRemaining);
          disp.writeDot(DOT_SPEED, false);
          disp.writeDot(DOT_STEP, true);
          SetState(States::Finished);
        }
        else if (rot)
        {
          if (rot > 0)
          {
            if ((speed + rot) > config.speed_max)
            {
              rot = config.speed_max - speed;
              buzzer_clic();
            }
          }
          else
          {
            if ((speed + rot) < config.speed_min)
            {
              rot = speed - config.speed_min;
              buzzer_clic();
            }
          }
          speed += rot;
          disp.write(speed);
          digitAt = millis();
        }
        else if ((millis() - digitAt) > DIGIT_TIMEOUT)
        {
          disp.hideCursor();
          disp.write(stepsRemaining);
          disp.writeDot(DOT_SPEED, false);
          disp.writeDot(DOT_STEP, true);
          SetState(States::Emulate);
        }
      }
      break;
    case States::Paused:
      if (encbtn.isReleased())
      {
        if (encbtn.getPressedDuration() > LONG_PRESS)
        {
          SetState(States::Init);
        }
        else
        {
          disp.blankScreen = false;
          SetState(States::Emulate);
        }
      }
      else
      {
        disp.blankScreen = ((millis() % 500) < 250);
      }
      break;
    case States::Finished:
      if (encbtn.isReleased())
      {
        SetState(States::Init);
      }
      /*
      else if ()
      {
        SetState(States::Poweroff);
      }
      */
      else
      {
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
      break;
    case States::Poweroff:
      disp.clear();
      disp.write(2, 0);
      disp.write(1, 15);
      disp.write(2, 15);
      break;
  }

  rot = 0;

#ifdef DEBUG_SER
  Serial.println();
  delay(100);
#endif
}
