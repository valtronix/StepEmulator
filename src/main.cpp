#include <Arduino.h>
#include <Servo.h>
#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/power.h>

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
#define PIN_SERVO        10 // Only 9 or 10 is supported
#define PIN_BUZZER       11
#define PIN_POWER        12

Servo myservo;
Display disp(PIN_SR_CK, PIN_SR_DI, PIN_SR_ST, PIN_CD4017_MR, PIN_DIGIT_ENA);
Button encbtn(PIN_ENCS);

unsigned int stepsRemaining;          // Number of remaining steps
unsigned char speed;                  // Actual speed (steps by second)
unsigned long lastUserInteractionAt;  // Last time digits was changed
unsigned long longPressDelay;         // Long press delay (previously LONG_PRESS)
unsigned long powerOffDelay;          // Delay before to switch off
unsigned long setTimeout;             // Delay before leaving set mode (timeout)

volatile char rot;

#define DEBOUNCE_TIME           5

#define DOT_STEP                0
#define DOT_SPEED               1
#define DOT_TIME                2
#define DOT_RESERVED            3
#define DOT_LONGPRESS           4

struct MyConfig_t {
  unsigned char pos_stepdown;         // Servo motor position when foot is down
  unsigned char pos_stepup;           // Servo motor position when foot is up
  unsigned int  steps_init;           // Number of steps by default (at startup)
  unsigned int  steps_min;            // Number of steps at minimum
  unsigned int  steps_max;            // Number of steps at maximum
  unsigned char speed_init;           // Default speed (at startup) (steps by minute)
  unsigned char speed_min;            // Lowest speed (steps by minute)
  unsigned char speed_max;            // Highest speed (steps by minute)
  unsigned char step_ratio;           // Step ratio (step up/down)
  unsigned char delay_longpress;      // Delay for a long press (previously LONG_PRESS) but in 10th of seconds
  unsigned char delay_set;            // Delay to exit set mode (previously DIGIT_TIMEOUT) but in 10th of seconds
  unsigned char delay_off;            // Delay before displaying OFF message (in seconds)
  unsigned char delay_offmsg;         // Delay before auto power off after OFF message (in 10th of seconds)
};

MyConfig_t config;
const MyConfig_t defaultConfig = {
  22,         // 0x00: 0x16
  130,        // 0x01: 0x82
  1000,       // 0x02: 0xe8 0x03 [232 3]
  10,         // 0x04: 0x0a 0x00 [10 0]
  20000,      // 0x06: 0x20 0x4e [32 78]
  100,        // 0x08: 0x64
  16,         // 0x09: 0x10
  255,        // 0x0a: 0xff
  50,         // 0x0b: 0x32
  10,         // 0x0c: 0x0a (1 second)
  15,         // 0x0d: 0x0f (1.5 second)
  60,         // 0x0e: 0x3c (60 seconds)
  50          // 0x0f: 0x32 (5 seconds)
};


enum States : char {
  PowerOff = -1,
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
  delay(2);
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
 * Emulate steps
 */
bool walk()
{
  static unsigned long nextStepAt = 0;
  static bool footUp = false;
  static bool walking = false;
  if (stepsRemaining == 0)
  {
    return true;
  }
  else
  {
    unsigned long now = millis();
    if (!walking)
    { // Start walking
      walking = true;
      myservo.attach(PIN_SERVO);
      footUp = false;
      nextStepAt = 0;
    }
    if (int(nextStepAt - now) < 0)
    {
      nextStepAt = now + (unsigned long)(30000U / (unsigned int)speed); // 60000 / 2
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
    }
  }
  return false;
}

void powerOn()
{
  digitalWrite(PIN_POWER, HIGH);
  delay(10);
}

void powerOff()
{
  digitalWrite(PIN_POWER, LOW);
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
  pinMode(PIN_POWER, OUTPUT);
  powerOn();

  rot = 0;
  lastUserInteractionAt = 0;
  longPressDelay = 1000;  // 1 second
  powerOffDelay = 60000;  // 60 secondes

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, HIGH);
  pinMode(PIN_ENCA, INPUT);
  pinMode(PIN_ENCB, INPUT);
  disp.begin();
  
  // Lamp test
  disp.lampTest();

  // On Uno card, only pins 2 and 3 are hardware interrupts
  attachInterrupt(digitalPinToInterrupt(PIN_ENCA), onEncoderTurned, FALLING);

  bool changeConfig = false;
  while ((millis() < 1000) || encbtn.isPressed())
  {
    digitalWrite(PIN_BUZZER, millis() <= 200);
    disp.displayNextDigit();
    encbtn.check();
    if (encbtn.isPressed() && (encbtn.getPressedDuration() > longPressDelay))
    {
      if (!changeConfig)
      {
        // Display [===]
        disp.write(0, 0xf0);
        disp.write(DIGIT_MAX-1, 0x9c);
        for (int i = 1; i < DIGIT_MAX-1; i++)
        {
          disp.write(i, 0x90);
        }
        encbtn.handled();
        changeConfig = true;
      }
    }
  }

  // If button stay pressed lng enough, switch to configuration mode
  if (changeConfig)
  {
    unsigned char address = 0;
    bool addressSelected = true;
    unsigned char value = EEPROM.read(address);
    disp.write(address, value, true);
    myservo.write(value);
    disp.setCursor(3);
    disp.cursor();
    myservo.attach(PIN_SERVO);
    while (changeConfig)
    {
      disp.displayNextDigit();
      encbtn.check();
      if (encbtn.isReleased())
      {
        if (encbtn.getPressedDuration() > longPressDelay)
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
  longPressDelay = int(config.delay_longpress) * 100U;
  powerOffDelay = int(config.delay_off) * 1000U;
  setTimeout = (unsigned long)config.delay_set * 100UL;
#ifdef DEBUG_SER
  Serial.println("Steps: " + String(stepsRemaining));
  Serial.println("Speed: " + String(speed) + " steps/min");
  Serial.println("Long press: " + String(double(longPressDelay) / 1000.0) + " s");
  Serial.println("Auto power off: " + String(powerOffDelay));
  Serial.println("Set timeout: " + String(setTimeout));
#endif

  // Clear all screen (including dots)
  disp.clear();
  disp.noCursor();
  disp.noLeadingZeros();
  disp.write(stepsRemaining);

  // timer1 (TCCR1) used by servo
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
      disp.cursor();
      break;
    case Emulate:
      // disp.setCursor(2);
      break;
    case PowerOff:
      digitalWrite(PIN_BUZZER, LOW);
      disp.clear();
      disp.display();
      disp.noCursor();
      disp.write(2, 0xfc); // O
      disp.write(1, 0x8e); // F
      disp.write(0, 0x8e); // F
#ifdef DEBUG_SER
      Serial.println("**Power Off");
#endif
      break;
    default:
      break;
  }
  lastUserInteractionAt = millis();
  state = newstate;
}

void wakeUpInterrupt() {
  // cancel sleep as a precaution
  sleep_disable();
  // precautionary while we do other stuff
  detachInterrupt(digitalPinToInterrupt(PIN_ENCS));
}

/*****************************************************************************
 * Main loop -----------------------------------------------------------------
 *****************************************************************************/
void loop() {
  disp.writeDot(DOT_LONGPRESS, encbtn.isPressed() && (encbtn.getPressedDuration() > longPressDelay));
  disp.displayNextDigit();
  encbtn.check();

  switch (state) {
    case States::Init:
#ifdef DEBUG_SER
      Serial.println("**Init");
#endif
      analogReference(INTERNAL);
      digitalWrite(PIN_BUZZER, LOW);
      if (encbtn.isPressed())
      {
        encbtn.handled();
      }
      stepsRemaining = config.steps_init;
      speed = config.speed_init;
      disp.clear();
      disp.noLeadingZeros();
      disp.noCursor();
      disp.setCursor(3);
      disp.write(stepsRemaining);
      disp.writeDot(DOT_STEP, true);
      disp.display();
      SetState(States::SetSteps);
      break;
    case States::SetSteps:
      if (encbtn.isReleased())
      {
        if (encbtn.getPressedDuration() > longPressDelay)
        {
          SetState(States::Emulate);
        }
        else
        {
          SetState(States::AdjustSteps);
        }
      }
      else if ((millis() - lastUserInteractionAt) > powerOffDelay)
      {
        SetState(States::PowerOff);
      }
      break;
    case States::AdjustSteps:
      if (encbtn.isReleased())
      {
        if (encbtn.getPressedDuration() > longPressDelay)
        {
          config.steps_init = stepsRemaining;
          // TODO: save to EEPROM
          SetState(States::Emulate);
        }
        else
        {
          disp.moveCursor(false);
        }
        lastUserInteractionAt = millis();
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
          if ((stepsRemaining - config.steps_min) < stepdiff)
          {
            stepsRemaining = config.steps_min;
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
        lastUserInteractionAt = millis();
      }
      else if ((millis() - lastUserInteractionAt) > setTimeout)
      { // If no user interaction
        disp.noCursor();
        SetState(States::SetSteps);
      }
      break;
    case States::Emulate:
      if (encbtn.isReleased())
      {
        if (encbtn.getPressedDuration() > longPressDelay)
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
        // disp.cursor();
        SetState(States::ChangeSpeed);
        lastUserInteractionAt = millis();
      }
      else
      {
        if (walk())
        {
          disp.leadingZeros();
          SetState(States::Finished);
        }
      }
      break;
    case States::ChangeSpeed:
      if (encbtn.isReleased())
      {
        if (encbtn.getPressedDuration() > longPressDelay)
        {
          SetState(States::Init);
        }
        else
        {
          disp.noCursor();
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
          disp.noCursor();
          disp.leadingZeros();
          disp.write(stepsRemaining);
          disp.writeDot(DOT_SPEED, false);
          disp.writeDot(DOT_STEP, true);
          SetState(States::Finished);
        }
        else if (rot)
        {
          unsigned int r = (unsigned int)abs(rot);
          if (rot > 0)
          {
            if ((speed + r) > config.speed_max)
            {
              r = config.speed_max - speed;
              buzzer_clic();
            }
            speed += r;
          }
          else
          {
            if ((speed - r) < config.speed_min)
            {
              r = speed - config.speed_min;
              buzzer_clic();
            }
            speed -= r;
          }
          disp.write(speed);
          lastUserInteractionAt = millis();
        }
        else if ((millis() - lastUserInteractionAt) > setTimeout)
        { // If no user interaction
          disp.noCursor();
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
        if (encbtn.getPressedDuration() > longPressDelay)
        {
          SetState(States::Init);
        }
        else
        {
          disp.display();
          SetState(States::Emulate);
        }
      }
      else
      {
        if ((millis() % 500) < 250)
        {
          disp.noDisplay();
        }
        else
        {
          disp.display();
        }
      }
      break;
    case States::Finished:
      if (encbtn.isReleased())
      {
        SetState(States::Init);
      }
      else if ((millis() - lastUserInteractionAt) > powerOffDelay)
      {
        SetState(States::PowerOff);
      }
      else
      {
        if ((millis() % 500) < 250)
        {
          disp.noDisplay();
          digitalWrite(PIN_BUZZER, HIGH);
        }
        else
        {
          disp.display();
          digitalWrite(PIN_BUZZER, LOW);
        }
      }
      break;
    case States::PowerOff:
      if (encbtn.isPressed())
      {
        encbtn.handled();
        powerOn();
        SetState(States::Init);
      }
      else if ((millis() - lastUserInteractionAt) > (unsigned long)(config.delay_offmsg * 100))
      {
        disp.noDisplay();
        if (!disp.isDisplay())
        {
          digitalWrite(PIN_BUZZER, LOW);
          if (myservo.attached())
          {
            myservo.detach();
          }
          digitalWrite(PIN_SERVO, LOW);
          powerOff(); // Power off external components

          analogReference(DEFAULT);
          byte old_ADCSRA = ADCSRA;
          ADCSRA = 0; // disable ADC
          set_sleep_mode(SLEEP_MODE_PWR_DOWN);
          sleep_enable();
          
          // Do not interrupt before we go to sleep, or the
          // ISR will detach interrupts and we won't wake.
          noInterrupts ();

          power_all_disable();  // turn off various modules

          // will be called when PIN_ENCS goes low
          attachInterrupt(digitalPinToInterrupt(PIN_ENCS), wakeUpInterrupt, FALLING);
          EIFR = bit (digitalPinToInterrupt(PIN_ENCS));  // clear flag for interrupt 0 or 1

          // turn off brown-out enable in software
          // BODS must be set to one and BODSE must be set to zero within four clock cycles
          MCUCR = bit (BODS) | bit (BODSE);
          // The BODS bit is automatically cleared after three clock cycles
          MCUCR = bit (BODS); 

          digitalWrite(LED_BUILTIN, HIGH);
          // We are guaranteed that the sleep_cpu call will be done
          // as the processor executes the next instruction after
          // interrupts are turned on.
          interrupts ();  // one cycle
          sleep_cpu ();   // one cycle

          // Here we are sleeping --- Zzzz Zzzz Zzzz Zzzz Zzzz Zzzz Zzzz
          // Waiting RESET or interrupt on INT0 (pin 2 = PIN_ENCS)


          digitalWrite(LED_BUILTIN, LOW);

          power_all_enable ();   // enable modules again
          ADCSRA = old_ADCSRA;   // re-enable ADC conversion

          powerOn();  // Power on external components
          SetState(States::Init);
        }
      }
      break;
  }

  rot = 0;
}
