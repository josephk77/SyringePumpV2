// Controls a stepper motor via an LCD keypad shield.
// Accepts triggers and serial commands.
// To run, you will need the LCDKeypad library installed - see libraries dir.

// Serial commands:
// Set serial baud rate to 57600 and terminate commands with newlines.
// Send a number, e.g. "100", to set bolus size.
// Send a "+" to push that size bolus.
// Send a "-" to pull that size bolus.
#include <Keyboard.h>
#include <LiquidCrystal.h>
#include <string.h>

/* -- Constants -- */
#define SYRINGE_VOLUME_ML 30.0f
#define SYRINGE_BARREL_LENGTH_MM 80.0f

#define THREADED_ROD_PITCH 1.25f
#define STEPS_PER_REVOLUTION 200.0f
#define MICROSTEPS_PER_STEP 16.0f 

#define SPEED_MICROSECONDS_DELAY 313 //longer delay = lower speed

long ustepsPerMM = MICROSTEPS_PER_STEP * STEPS_PER_REVOLUTION / THREADED_ROD_PITCH;
long ustepsPerML = (MICROSTEPS_PER_STEP * STEPS_PER_REVOLUTION * SYRINGE_BARREL_LENGTH_MM) / (SYRINGE_VOLUME_ML * THREADED_ROD_PITCH );

/* -- Pin definitions -- */
int motorDirPin = 2;
int motorStepPin = 3;

int triggerPin = A3;
int bigTriggerPin = A4;

/* -- Keypad states -- */
int adc_key_val[5] ={700, 820, 875, 915, 950 };          // Original, works after actually reading the 
enum{ KEY_SELECT, KEY_RIGHT, KEY_LEFT, KEY_DOWN, KEY_UP, KEY_NONE};
int NUM_KEYS = 5;
int adc_key_in;
int key = KEY_NONE;

/* -- Enums and constants -- */
enum{RATE, RATE_CHECK, VOLUME, VOLUME_CHECK, RUN, STOP}; //UI states
enum{BELOW_HARD_LIMIT, BELOW_SOFT_LIMIT, WITHIN_LIMITS, ABOVE_SOFT_LIMIT, ABOVE_HARD_LIMIT}; //Limit States

/* -- debounce params -- */
long lastKeyRepeatAt = 0;
long keyRepeatDelay = 400;
long keyDebounce = 1000;
int prevKey = KEY_NONE;

/* -- Display -- */
char charBuf[16];
int uiState = RATE;
int limitState = WITHIN_LIMITS;
float _rate = 0.0;
float _volume = 0.0;

/* -- Rate Limits -- */
float RATE_UPPER_HARD_LIMIT = 7000;
float RATE_UPPER_SOFT_LIMIT = 30;
float RATE_LOWER_SOFT_LIMIT = 10;
float RATE_LOWER_HARD_LIMIT = 5;

/* -- Volume Limits -- */
float VOLUME_UPPER_HARD_LIMIT = 100;
float VOLUME_UPPER_SOFT_LIMIT = 75;
float VOLUME_LOWER_SOFT_LIMIT = 45;
float VOLUME_LOWER_HARD_LIMIT = 5;

//serial
String serialStr = "";
boolean serialStrReady = false;

/* -- Initialize libraries -- */
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

void setup(){
  /* LCD setup */  
  lcd.begin(16, 2);
  PrintScreen(" SyringePump v2", "");
  //lcd.clear();

  //lcd.print();
  delay(3000);

  /* Serial setup */
  //Note that serial commands must be terminated with a newline
  //to be processed. Check this setting in your serial monitor if 
  //serial commands aren't doing anything.
  motorSetup();
  Serial.begin(57600); //Note that your serial connection must be set to 57600 to work!
  updateScreen();
}

void motorSetup(){

  /* Triggering setup */
  pinMode(triggerPin, INPUT);
  pinMode(bigTriggerPin, INPUT);
  digitalWrite(triggerPin, HIGH); //enable pullup resistor
  digitalWrite(bigTriggerPin, HIGH); //enable pullup resistor
  
  /* Motor Setup */ 
  pinMode(motorDirPin, OUTPUT);
  pinMode(motorStepPin, OUTPUT);
  
}

void loop(){
  //check for LCD updates
  readKey();
}

void readKey(){
  //Some UI niceness here. 
        //When user holds down a key, it will repeat every so often (keyRepeatDelay).
        //But when user presses and releases a key, 
        //the key becomes responsive again after the shorter debounce period (keyDebounce).

  adc_key_in = analogRead(0);
  
  key = get_key(adc_key_in); // convert into key press
  
  long currentTime = millis();
  long timeSinceLastPress = (currentTime-lastKeyRepeatAt);
        
  boolean processThisKey = false;
  if (prevKey == key && timeSinceLastPress > keyRepeatDelay){
    processThisKey = true;
  }
  if(prevKey == KEY_NONE && timeSinceLastPress > keyDebounce){
    processThisKey = true;
  }
  if(key == KEY_NONE){
    processThisKey = false;
  }  
  
  prevKey = key;
  
  if(processThisKey){
    doKeyAction(key);
    lastKeyRepeatAt = currentTime;
  }
}

void doKeyAction(unsigned int key){
  if(key == KEY_NONE){
        return;
    }

  if(uiState == RATE){
    if(key == KEY_LEFT){
      ReversePump();
    }
    if(key == KEY_RIGHT){
     // do nothing
    }
    if(key == KEY_UP){
      _rate += 100;
    }
    if(key == KEY_DOWN){
      _rate -= 100;
    }
     if(key == KEY_SELECT){
      if (CheckLimits(RATE_UPPER_HARD_LIMIT,RATE_UPPER_SOFT_LIMIT,RATE_LOWER_SOFT_LIMIT ,RATE_LOWER_HARD_LIMIT,_rate)){
        uiState = RATE_CHECK;
      }
      else {
        uiState = VOLUME;
      }
    }
  }

  else if(uiState == RATE_CHECK){
    if(key == KEY_LEFT){
      uiState = RATE;
    }
    if(key == KEY_RIGHT){
     // do nothing
    }
    if(key == KEY_UP){
      // do nothing
    }
    if(key == KEY_DOWN){
      // do nothing
    }
     if(key == KEY_SELECT){
      if (limitState == BELOW_SOFT_LIMIT || limitState == ABOVE_SOFT_LIMIT){
        uiState = VOLUME;
      }
    }
  }
  
  else if(uiState == VOLUME){
    if(key == KEY_LEFT){
      uiState = RATE;
    }
    if(key == KEY_RIGHT){
      //do nothing
    }
    if(key == KEY_UP){
      _volume += 5;
    }
    if(key == KEY_DOWN){
      _volume -= 5;
    }
     if(key == KEY_SELECT){
      if (CheckLimits(VOLUME_UPPER_HARD_LIMIT,VOLUME_UPPER_SOFT_LIMIT,VOLUME_LOWER_SOFT_LIMIT ,VOLUME_LOWER_HARD_LIMIT,_volume)){
        uiState = VOLUME_CHECK;
      }
      else {
        uiState = RUN;
      }
    }
  }

  else if(uiState = VOLUME_CHECK){
    if(key == KEY_LEFT){
      uiState = VOLUME;
    }
    if(key == KEY_RIGHT){
     // do nothing
    }
    if(key == KEY_UP){
      // do nothing
    }
    if(key == KEY_DOWN){
      // do nothing
    }
    if(key == KEY_SELECT){
      if (limitState == BELOW_SOFT_LIMIT || limitState == ABOVE_SOFT_LIMIT){
        uiState = RUN;
      }
    }
  }
  
  else if(uiState == RUN){
    if(key == KEY_LEFT){
      uiState = RATE;
    }
    if(key == KEY_RIGHT){
      uiState = RATE;
    }
    if(key == KEY_UP){
      uiState = RATE;
    }
    if(key == KEY_DOWN){
      uiState = RATE;
    }
    if(key == KEY_SELECT){
     
    }
  }

  updateScreen();
}

bool CheckLimits(float uhl, float usl, float lsl, float lhl, float value){
  bool limitsViolated = true;
  if (value < lhl){
    limitState = BELOW_HARD_LIMIT;
  }
  else if(value < lsl){
    limitState = BELOW_SOFT_LIMIT;
  }
  else if (value > uhl){
    limitState = ABOVE_HARD_LIMIT;
  }
  else if(value > usl){
    limitState = ABOVE_SOFT_LIMIT;
  }
  else {
    limitState = WITHIN_LIMITS;
    limitsViolated = false;
  }
  return limitsViolated;
}

void updateScreen(){
  //build strings for upper and lower lines of screen
  String s1; //upper line
  String s2; //lower line

  if(uiState == RATE){
    s1 = (String("Rate ") + decToString(_rate) + String("mL/hr"));
    s2 = String("L:PULL   S:NEXT"); 
  }
  if(uiState == RATE_CHECK){
    s2 = String("L:BACK   ");
    switch (limitState) {
      case BELOW_HARD_LIMIT:
        s1 = String("Rate<LHL ") + decToString(RATE_LOWER_HARD_LIMIT) + String("mL/hr"); 
        break;
      case BELOW_SOFT_LIMIT:
        s1 = String("Rate<LSL ") + decToString(RATE_LOWER_SOFT_LIMIT) + String("mL/hr");
        s2 = s2 + String("S:NEXT"); 
        break;
      case ABOVE_SOFT_LIMIT:
        s1 = String("Rate>USL ") + decToString(RATE_UPPER_SOFT_LIMIT) + String("mL/hr");
        s2 = s2 + String("S:NEXT"); 
      break;
      case ABOVE_HARD_LIMIT:
        s1 = String("Rate>UHL ") + decToString(RATE_UPPER_HARD_LIMIT) + String("mL/hr");
      break;
      default:
        break;
    }
  }
  else if(uiState == VOLUME){
    s1 = (String("Volume ") + decToString(_volume) + String(" mL"));  
    s2 = String("L:BACK   S:NEXT");  
  }
  else if (uiState == VOLUME_CHECK){
    s2 = String("L:BACK   ");
    switch (limitState) {
      case BELOW_HARD_LIMIT:
        s1 = String("Volume<LHL ") + decToString(VOLUME_LOWER_HARD_LIMIT) + String("mL"); 
        break;
      case BELOW_SOFT_LIMIT:
        s1 = String("Volume<LSL ") + decToString(VOLUME_LOWER_SOFT_LIMIT) + String("mL");
        s2 = s2 + String("S:NEXT"); 
        break;
      case ABOVE_SOFT_LIMIT:
        s1 = String("Volume>USL ") + decToString(VOLUME_UPPER_SOFT_LIMIT) + String("mL");
        s2 = s2 + String("S:NEXT"); 
      break;
      case ABOVE_HARD_LIMIT:
        s1 = String("Volume>UHL ") + decToString(VOLUME_UPPER_HARD_LIMIT) + String("mL");
      break;
      default:
        break;
    }
  }
  else if (uiState == RUN){
    s1 = String("Running ") + decToString(_volume) + String(" mL");
    s2 = String(" at ") + decToString(_rate) + String(" mL/hr");
    PrintScreen(s1, s2);
    unsigned long startTimeMs = millis();
    RunPump(_rate, _volume);
    unsigned long currentTimeMs = millis();
    unsigned long elapsedTimeSec = (currentTimeMs - startTimeMs)/1000.0f; //convert to seconds  
    s1 = String("Infusion Complete");
    s2 = String(_volume) + String("mL in ") + String(elapsedTimeSec) + String("s");
  }
  PrintScreen(s1, s2);
}

void PrintScreen(String s1, String s2){
  //do actual screen update
  lcd.clear();

  s2.toCharArray(charBuf, 16);
  lcd.setCursor(0, 1);  //line=2, x=0
  lcd.print(charBuf);
  
  s1.toCharArray(charBuf, 16);
  lcd.setCursor(0, 0);  //line=1, x=0
  lcd.print(charBuf);
}


void RunPump(int rate, int volume){
  //Move stepper. Will not return until stepper is done moving.        
  
  //change units to steps
  long steps = (volume * ustepsPerML);

  digitalWrite(motorDirPin, HIGH);

  float usDelay = ((1 / (rate * (SYRINGE_BARREL_LENGTH_MM / SYRINGE_VOLUME_ML) * (1 / THREADED_ROD_PITCH) * (STEPS_PER_REVOLUTION * MICROSTEPS_PER_STEP) * (1 / 3600.0f) * (1/1000000.0f))) / 2);

  for(long i=0; i < steps; i++){ 
    digitalWrite(motorStepPin, HIGH); 
    delayMicroseconds(usDelay); 

    digitalWrite(motorStepPin, LOW); 
    delayMicroseconds(usDelay); 
  }
}

void ReversePump(){
  //Move stepper. Will not return until stepper is done moving.        
  
  //change units to steps
  long steps = (ustepsPerML);

  digitalWrite(motorDirPin, LOW);

  float usDelay = 75; //can go down to 20 or 30

  for(long i=0; i < steps; i++){ 
    digitalWrite(motorStepPin, HIGH); 
    delayMicroseconds(usDelay); 

    digitalWrite(motorStepPin, LOW); 
    delayMicroseconds(usDelay); 
  } 

}

void readSerial(){
    //pulls in characters from serial port as they arrive
    //builds serialStr and sets ready flag when newline is found
    while (Serial.available()) {
      char inChar = (char)Serial.read(); 
      if (inChar == '\n') {
        serialStrReady = true;
      } 
                        else{
        serialStr += inChar;
                        }
    }
}

// Convert ADC value to key number
int get_key(unsigned int input){
  int k;
  for (k = 0; k < NUM_KEYS; k++){
    if (input < adc_key_val[k]){
      return k;
    }
  }
  if (k >= NUM_KEYS){
    k = KEY_NONE;     // No valid key pressed
  }
  return k;
}

String decToString(float decNumber){
  //not a general use converter! Just good for the numbers we're working with here.
  int wholePart = decNumber; //truncate
  int decPart = round(abs(decNumber*1000)-abs(wholePart*1000)); //2 decimal places
        String strZeros = String("");
        if(decPart < 10){
          strZeros = String("");
        }  
        else if(decPart < 100){
          strZeros = String("");
        }
  return String(wholePart);
  //return String(wholePart) + String('.') + strZeros + String(decPart);
}
