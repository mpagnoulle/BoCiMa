/*  
 *  Home Boiler and Circulating Pump Management (BoCiMa)
 *  v1.1 (Last Update: 2017-01-22)
 */

// Include libraries
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <avr/wdt.h>

// Declare Constants

#define MINHEATERTEMP 60
#define MAXHEATERTEMP 80
#define MINBOILERTEMP 50
#define MAXBOILERTEMP 60
#define MAXTEMP 90
#define CIRDELAY 180000

#define I2C_ADDR    0x27 
#define BACKLIGHT_PIN     3
#define En_pin  2
#define Rw_pin  1
#define Rs_pin  0
#define D4_pin  4
#define D5_pin  5
#define D6_pin  6
#define D7_pin  7

// Declare global vars

// EEPROM Stuff
int eeAddress = 0;

struct AlertStatus {
  bool isAlertSec;
  bool isAlertBur;
};

struct OverrideStatus {
  bool isC1Ov;
  bool isC2Ov;
};

struct BoilerTemps {
  short Min;
  short Max;
};

struct HeaterTemps {
  short Min;
  short Max;
};

struct LastErrors {
  short ErrCode1;
  short ErrCode2;
  short ErrCode3;
  short rebootCount;
};

// LCD

LiquidCrystal_I2C  lcd(I2C_ADDR, En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin);

// °c
byte charDegC[8] = {
  B01000,
  B10100,
  B01000,
  B00011,
  B00100,
  B00100,
  B00011,
  B00000
};

// 1C

byte char1C[8] = {
  B10000,
  B11000,
  B11100,
  B11001,
  B10011,
  B00001,
  B00001,
  B00001
};

// 2C

byte char2C[8] = {
  B10000,
  B11000,
  B11100,
  B11011,
  B10001,
  B00111,
  B00100,
  B00111
};

// TO

byte charTO[8] = {
  B00000,
  B00001,
  B00010,
  B10100,
  B01000,
  B00000,
  B11111,
  B00000
};

byte charSEC[8] =
{
  B00100,
  B01110,
  B01110,
  B01110,
  B11111,
  B11111,
  B00000,
  B00100
};

byte charBOI[8] = 
{
    B00100,
    B00100,
    B01010,
    B01010,
    B10001,
    B10001,
    B10001,
    B01110,
};

byte charCH[8] =
{
    B00100,
    B01010,
    B01010,
    B01010,
    B01110,
    B11111,
    B11111,
    B01110
};

// Sensor 1-Wire BUS

OneWire oneWireA(8);
DallasTemperature sensorsA(&oneWireA);

OneWire oneWireB(9);
DallasTemperature sensorsB(&oneWireB);

char NumberOfDevices = 0;
DeviceAddress SensorHeater = { 0x28, 0xFF, 0xB4, 0x99, 0x15, 0x15, 0x01, 0x58 };
DeviceAddress SensorBoiler = { 0x28, 0xFF, 0xFF, 0x5D, 0x15, 0x15, 0x01, 0xF6 };
DeviceAddress SensorSecurity = { 0x28, 0xFF, 0xAC, 0x9B, 0x15, 0x15, 0x01, 0x1D };

// The rest

float TempHeater = -1;
float TempBoiler = -1;

float TempSecurity = -1;
float TSMin = 100;
float TSMax = 0;

bool isAlert = false;
bool isAlertSecurity = false;
bool isAlertSensors = false;
bool isC1Ov = false;
bool isC1ON = false;
bool isC2Ov = false;
bool isC2ON = false;
bool wasC2ON = false;
bool isCooldownC2 = false;
bool wasHeaterON = false;
bool heaterStopByTemp = false;
bool cooldownTimeOut = true;

bool TStatus = false;
bool isHeaterOn = false;
bool isBoilerOn = false;

BoilerTemps theBoilerTemps;
HeaterTemps theHeaterTemps;
AlertStatus actualAlert;
LastErrors errorCodeList;

// Pins

int Pump1 = 22;
int Pump2 = 23;
int Burner = 24;

int buzzerPin = 10;
int resetButtonAct = 40;
int resetButtonLED = 30;

int C1ButtonAct = 41;
int C1ButtonLED = 31;

int C2ButtonAct = 42;
int C2ButtonLED = 32;

int BurnerLED = 53;

int AlertLED = 33;
int AlertSecLED = 34;
int AlertSecAct = 43;
int AlertST = 44;

int Thermostat = 50;


// Other

int alertSTC = 0;
bool isTick = false;
bool isConvertOK = false;
bool isActionDone = true;
bool isEditingMode = false;
short editStep = 0;
short editType = 0;
unsigned long ftMillis = 0;
unsigned long currentMillis = 0;
unsigned long alertSTMillis = 0;
unsigned long previousMillis = 0;
unsigned long debounceMillis = 0;
unsigned long debounceMillisC = 0;
unsigned long debounceMillisS = 0;
unsigned long debounceMillisE = 0;
unsigned long resetMillis = 0;
unsigned long pMAlertS = 0;
unsigned long backLightLCD = 0;
unsigned long refreshLCD = 0;
unsigned long emergencyC1 = 0;
unsigned long circDelay = 0;
unsigned long checkSensorDelay = 0;
bool isLCDOn = true;
//short AlertSecScreen = 0;
short ButLEDSt = 0;
bool AlertButSt = false;
bool isFirstClear = true;
bool isSensor1Missing = false;
bool isSensor2Missing = false;
bool isSensor3Missing = false;
char missingType = 0;
char isNotConCount = 0;
bool isC1EmOn = false;
bool isErrMode = false;

void(* resetFunc) (void) = 0;

void setup() 
{
  Serial.begin(9600);

  wdt_disable();
  
  // Setup LCD Screen
  lcd.begin(16, 2);
  lcd.setBacklightPin(BACKLIGHT_PIN, POSITIVE);
  lcd.setBacklight(HIGH);
  lcd.home();
  
  lcd.createChar(0, charDegC);
  lcd.createChar(1, char1C);
  lcd.createChar(2, char2C);
  lcd.createChar(3, charTO);
  lcd.createChar(4, charSEC);
  lcd.createChar(5, charBOI);
  lcd.createChar(6, charCH);

  lcd.setCursor(0, 0);
  lcd.print("BoCiMa v1-170122");
  lcd.setCursor(0, 1);
  lcd.print(">> Check Sensors");

  sensorsA.begin();
  sensorsB.begin();

  sensorsA.setResolution(SensorHeater, 12);
  sensorsA.setResolution(SensorBoiler, 12);
  sensorsB.setResolution(SensorSecurity, 12);

  sensorsA.setWaitForConversion(false);
  sensorsB.setWaitForConversion(false);

  if(sensorsA.isConnected(SensorHeater) && sensorsA.isConnected(SensorBoiler) && sensorsB.isConnected(SensorSecurity))
  {
    sensorsA.requestTemperatures();
    sensorsB.requestTemperatures();

    FetchTemps();
  }
  else
  {
    if(!sensorsA.isConnected(SensorHeater))
    {
      isSensor1Missing = true;
    }

    if(!sensorsA.isConnected(SensorBoiler))
    {
      isSensor2Missing = true;
    }

    if(!sensorsB.isConnected(SensorSecurity))
    {
      isSensor3Missing = true;
    }

    missingType = 1;
    
    isAlertSensors = true;
  }
  
  delay(2000);
  
  // Set Controller Pins
  lcd.setCursor(0, 1);
  lcd.print(">> Param. PINS  ");

  delay(200);
  
  // Relays
  pinMode(22, OUTPUT);
  pinMode(23, OUTPUT);
  pinMode(24, OUTPUT);

  // Reset Button
  pinMode(resetButtonLED, OUTPUT);
  pinMode(resetButtonAct, INPUT_PULLUP);

  // C1 Button
  pinMode(C1ButtonLED, OUTPUT);
  pinMode(C1ButtonAct, INPUT_PULLUP);

  // C2 Button
  pinMode(C2ButtonLED, OUTPUT);
  pinMode(C2ButtonAct, INPUT_PULLUP);

  // Alerts Button
  pinMode(AlertLED, OUTPUT);
  pinMode(AlertSecLED, OUTPUT);
  pinMode(AlertSecAct, INPUT_PULLUP);
  pinMode(AlertST, INPUT);

  pinMode(BurnerLED, OUTPUT);

  // Thermostat
  pinMode(Thermostat, INPUT_PULLUP);

  // Reset All Relays
  lcd.setCursor(0, 1);
  lcd.print(">> Relays to OFF");
  ResetRelays();
  
  delay(200);
  
  // Test all lights

  lcd.print(">> Check Lights ");

  digitalWrite(C1ButtonLED, HIGH);
  digitalWrite(C2ButtonLED, HIGH);
  digitalWrite(AlertLED, HIGH);
  digitalWrite(AlertSecLED, HIGH);
  digitalWrite(BurnerLED, HIGH);

  delay(200);

  lcd.setCursor(0, 1);
  lcd.print(">> EEPROM Check ");

  delay(200);
  
  OverrideStatus ovStatus;
  eeAddress = sizeof(AlertStatus);
  EEPROM.get(eeAddress, ovStatus);
  if(ovStatus.isC1Ov == false)
  {
    isC1Ov = false;
  }
  else if(ovStatus.isC1Ov == true)
  {
    isC1Ov = true;
  }
  else
  {
    isC1Ov = false;
  }
  
  if(ovStatus.isC2Ov == false)
  {
    isC2Ov = false;
  }
  else if(ovStatus.isC2Ov == true)
  {
    isC2Ov = true;
  }
  else
  {
    isC2Ov = false;
  }
  
  eeAddress = sizeof(AlertStatus) + sizeof(OverrideStatus);
  EEPROM.get(eeAddress, theBoilerTemps);
  if(theBoilerTemps.Min == -1)
  {
    theBoilerTemps.Min = MINBOILERTEMP;
  }
  if(theBoilerTemps.Max == -1)
  {
    theBoilerTemps.Max = MAXBOILERTEMP;
  }

  eeAddress = sizeof(AlertStatus) + sizeof(OverrideStatus) + sizeof(theBoilerTemps);
  EEPROM.get(eeAddress, theHeaterTemps);
  if(theHeaterTemps.Min == -1)
  {
    theHeaterTemps.Min = MINHEATERTEMP;
  }
  if(theHeaterTemps.Max == -1)
  {
    theHeaterTemps.Max = MAXHEATERTEMP;
  }

  eeAddress = 0;
  EEPROM.get(eeAddress, actualAlert);

  if(actualAlert.isAlertSec == false)
  {
    isAlertSecurity = false;
  }
  else if(actualAlert.isAlertSec == true)
  {
    isAlertSecurity = true;
  }
  else
  {
    isAlertSecurity = false;
  }
  
  if(actualAlert.isAlertBur == false)
  {
    isAlert = false;
  }
  else if(actualAlert.isAlertBur == true)
  {
    isAlert = true;
  }
  else
  {
    isAlert = false;
  }

  eeAddress = sizeof(AlertStatus) + sizeof(OverrideStatus) + sizeof(theBoilerTemps) + sizeof(theHeaterTemps);
  /* SETUP
  errorCodeList.ErrCode3 = 0;
  errorCodeList.ErrCode2 = 0;
  errorCodeList.ErrCode1 = 0;
  errorCodeList.rebootCount = 0;

  EEPROM.put(eeAddress, errorCodeList);*

  ///////////*/
  EEPROM.get(eeAddress, errorCodeList);

  lcd.setCursor(0, 1);
  lcd.print(">> Start WDT    ");
  
  delay(200);

  wdt_enable(WDTO_8S);

  lcd.setCursor(0, 1);
  lcd.print(">> Fetch Temps.");
  
  FetchTemps();
  
  delay(200);
  lcd.setCursor(0, 1);
  lcd.print(">> READY        ");
  delay(1000);
  tone(buzzerPin, 4000, 75);
  digitalWrite(resetButtonLED, HIGH);
  digitalWrite(C1ButtonLED, LOW);
  digitalWrite(C2ButtonLED, LOW);
  digitalWrite(AlertLED, LOW);
  digitalWrite(AlertSecLED, LOW);
  digitalWrite(BurnerLED, LOW);
}

void loop() 
{
  wdt_reset();
  
  // Debug Temps
  /*while (Serial.available() > 0) 
  {
    TempHeater = Serial.parseInt();
    TempBoiler = Serial.parseInt();
    TempSecurity = Serial.parseInt();
    
    if (Serial.read() == '\n') 
    {
      break;
    }
  } */

  currentMillis = millis();

  FetchTemps();
  
  SecurTemp();

  //LCD Backlight management
  if (((unsigned long)(currentMillis - backLightLCD) >= 300000) && isLCDOn == true && isEditingMode == false) 
  {
    lcd.setBacklight(LOW);
    isLCDOn = false;
    backLightLCD = currentMillis;
  }
  
  if(digitalRead(AlertST) == HIGH)
  {
    if ((unsigned long)(currentMillis - alertSTMillis) >= 1500) 
    {
      isAlert = true;
      SaveAlert();
      alertSTMillis = currentMillis;
      alertSTC++;
    }
  }
  else if(alertSTC < 3)
  {
    alertSTMillis = currentMillis;
  }
  
  if(isAlert == false && isAlertSecurity == false && isAlertSensors == false)
  {
    if(isEditingMode == false && isErrMode == false)
    {
      // Show Updated Data
      ShowScreen();
    }
    else if(isErrMode == true)
    {
      ShowErrScreen();
    }
    else
    {
      ShowEditScreen();
    }

    // Actions
    ResetButtonCheck();
    ButtonLEDStatus();

    // Management
    CheckBoiler();
    CheckHeater();
    CheckThermo();
    
    if(digitalRead(C1ButtonAct) == LOW)
    {
      /*if(isLCDOn == false)
      {
        lcd.setBacklight(HIGH);
        isLCDOn = true;
        backLightLCD = currentMillis;
      }*/
  
      if(isEditingMode == false && isLCDOn == true)
      {
        if ((unsigned long)(currentMillis - debounceMillisC) >= 1000) 
        {
          if(isC1Ov == false)
          {
            tone(buzzerPin, 4000, 10);
            isC1Ov = true;
            SaveOverride();
          }
          else
          {
            tone(buzzerPin, 4000, 10);
            isC1Ov = false;
            SaveOverride();
            digitalWrite(C1ButtonLED, LOW);
          }
          debounceMillisC = currentMillis;
        }
      }
      else if(isLCDOn == true)
      {
        if ((unsigned long)(currentMillis - debounceMillisC) >= 250) 
        {
          if(editType == 0)
          {
            tone(buzzerPin, 4000, 10);
            editType = 1;
          }
          else if(editType == 1)
          {
            if(editStep == 0)
            {
              if(theHeaterTemps.Min > 20 && theHeaterTemps.Min <= theHeaterTemps.Max)
              {
                tone(buzzerPin, 4000, 10);
                theHeaterTemps.Min--;
              }
            }
            else
            {
              if(editStep == 1)
              {
                if(theHeaterTemps.Max > theHeaterTemps.Min)
                {
                  tone(buzzerPin, 4000, 10);
                  theHeaterTemps.Max--;
                }
              }
            }
          }
          else if(editType == 2)
          {
            if(editStep == 0)
            {
              if(theBoilerTemps.Min > 20 && theBoilerTemps.Min <= theBoilerTemps.Max)
              {
                tone(buzzerPin, 4000, 10);
                theBoilerTemps.Min--;
              }
            }
            else
            {
              if(editStep == 1)
              {
                if(theBoilerTemps.Max > theBoilerTemps.Min)
                {
                  tone(buzzerPin, 4000, 10);
                  theBoilerTemps.Max--;
                }
              }
            }
          }
          
          debounceMillisC = currentMillis;
        }
      }
    }
    else
    {
      debounceMillisC = currentMillis;
    }

    if(digitalRead(C2ButtonAct) == LOW)
    { 
      if(isEditingMode == false && isLCDOn == true)
      {
        if ((unsigned long)(currentMillis - debounceMillis) >= 1000) 
        {
          if(isC2Ov == false)
          {
            tone(buzzerPin, 4000, 10);
            isC2Ov = true;
            SaveOverride();
          }
          else
          {
            tone(buzzerPin, 4000, 10);
            isC2Ov = false;
            SaveOverride();
            digitalWrite(C2ButtonLED, LOW);
          }
          debounceMillis = currentMillis;
        } 
      }
      else if(isLCDOn == true)
      {
        if ((unsigned long)(currentMillis - debounceMillis) >= 250) 
        {
          if(editType == 0)
          {
            tone(buzzerPin, 4000, 10);
            editType = 2;
          }
          else if(editType == 1)
          {
            if(editStep == 0)
            {
              if(theHeaterTemps.Min < theHeaterTemps.Max)
              {
                tone(buzzerPin, 4000, 10);
                theHeaterTemps.Min++;
              }
            }
            else
            {
              if(editStep == 1)
              {
                if(theHeaterTemps.Max < MAXTEMP)
                {
                  tone(buzzerPin, 4000, 10);
                  theHeaterTemps.Max++;
                }
              }
            }
          }
          else if(editType == 2)
          {
            if(editStep == 0)
            {
              if(theBoilerTemps.Min < theBoilerTemps.Max)
              {
                tone(buzzerPin, 4000, 10);
                theBoilerTemps.Min++;
              }
            }
            else
            {
              if(editStep == 1)
              {
                if(theBoilerTemps.Max < MAXTEMP)
                {
                  tone(buzzerPin, 4000, 10);
                  theBoilerTemps.Max++;
                }
              }
            }
          }
          debounceMillis = currentMillis;
        }
      }
    }
    else
    {
      debounceMillis = currentMillis;
    }

    if(digitalRead(AlertSecAct) == LOW)
    {
      if ((unsigned long)(currentMillis - debounceMillisE) >= 500) 
      {
        if(isErrMode == false)
          isErrMode = true;
        else
          isErrMode = false;
      }
    }
    else
    {
      debounceMillisE = currentMillis;
    }
  }
  else
  {
    if(isAlertSecurity == true)
    {
      ResetRelays();

      if(isC1EmOn == false)
      {
        digitalWrite(Pump1, HIGH);
        isC1EmOn = true;
        emergencyC1 = currentMillis;
      }

      if ((unsigned long)(currentMillis - emergencyC1) >= 300000) 
      {
        digitalWrite(Pump1, LOW);
      }

      isLCDOn = true;
      backLightLCD = currentMillis;

      digitalWrite(C1ButtonLED, LOW);
      digitalWrite(C2ButtonLED, LOW);
      digitalWrite(BurnerLED, LOW);
      
      /*if(AlertSecScreen == 0)
      {
        lcd.setCursor(0, 0);
        lcd.print("! SECURITE 90C !");
        lcd.setCursor(0, 1);
        lcd.print("Temp. > 90C     ");
      }
      else if(AlertSecScreen == 1)
      {*/
         lcd.setCursor(0, 0);
         lcd.print("! SAFETY > 90C !");
         lcd.setCursor(0, 1);
         lcd.print("MIN ");
         lcd.print(TSMin, 0);
         lcd.print(" | MAX ");
         lcd.print(TSMax, 0);
     /* }
      else
      {
        AlertSecScreen = 0;
      }*/

      if ((unsigned long)(currentMillis - debounceMillis) >= 350) 
      {
        if(AlertButSt == false)
        {
          AlertButSt = true;
          digitalWrite(AlertSecLED, HIGH);
          delay(100);
          digitalWrite(AlertSecLED, LOW);
          delay(100);
          digitalWrite(AlertSecLED, HIGH);
        }
        else
        {
          AlertButSt = false;
          digitalWrite(AlertSecLED, LOW);
        }

        debounceMillis = currentMillis;
        
      }
      
      if ((unsigned long)(currentMillis - pMAlertS) >= 2000) 
      {
        //AlertSecScreen++;
        tone(buzzerPin, 4000, 500);
        pMAlertS = currentMillis;
      }
      
      if(digitalRead(AlertSecAct) == LOW)
      {
        if ((unsigned long)(currentMillis - previousMillis) >= 250) 
        {
          isAlertSecurity = false;
          //AlertSecScreen = 0;
          tone(buzzerPin, 3000, 250);
          previousMillis = currentMillis;
          digitalWrite(AlertSecLED, LOW);
          SaveAlert();
        }
      }
    }
    else if(isAlert == true)
    {
      ResetRelaysExBur(); 

      digitalWrite(C1ButtonLED, LOW);
      digitalWrite(C2ButtonLED, LOW);

      isLCDOn = true;
      backLightLCD = currentMillis;

      lcd.setCursor(0, 0);
      lcd.print("! BOILER > OFF !");
      lcd.setCursor(0, 1);
      lcd.print("  Safety  Mode  ");

      digitalWrite(AlertLED, HIGH);
      
      if(digitalRead(AlertST) == LOW)
      {
        isAlert = false;
        SaveAlert();
        digitalWrite(AlertLED, LOW);
      }
    }
    else if(isAlertSensors == true && isAlert == false)
    {
      ResetRelays();

      short errorTemp = 0;

      /*lcd.setBacklight(HIGH);
      isLCDOn = true;
      backLightLCD = currentMillis;*/

      digitalWrite(C1ButtonLED, LOW);
      digitalWrite(C2ButtonLED, LOW);
      digitalWrite(BurnerLED, LOW);
      
      if(missingType == 1)
      {
        if(isSensor1Missing)
        {
          errorTemp = 11;
        }
        if(isSensor2Missing)
        {
          errorTemp = 12;
        }
        if(isSensor3Missing)
        {
          errorTemp = 13;
        }
      }
      else if(missingType == 2)
      {
        if(isSensor1Missing)
        {
          errorTemp = 21;
        }
        if(isSensor2Missing)
        {
          errorTemp = 22;
        }
        if(isSensor3Missing)
        {
          errorTemp = 23;
        }
      }

      if(errorCodeList.rebootCount < 5)
      {
        //wdt_disable();

        lcd.setCursor(0, 0);
        lcd.print("! ERROR SENSOR !");

        if(errorCodeList.ErrCode1 == 11)
        {
          lcd.setCursor(0, 1);
          lcd.print("Type1 // Sensor1");
        }
        else if(errorCodeList.ErrCode1 == 12)
        {
          lcd.setCursor(0, 1);
          lcd.print("Type1 // Sensor2");
        }
        else if(errorCodeList.ErrCode1 == 13)
        {
          lcd.setCursor(0, 1);
          lcd.print("Type1 // Sensor3");
        }
        else if(errorCodeList.ErrCode1 == 21)
        {
          lcd.setCursor(0, 1);
          lcd.print("Type2 // Sensor1");
        }
        else if(errorCodeList.ErrCode1 == 22)
        {
          lcd.setCursor(0, 1);
          lcd.print("Type2 // Sensor2");
        }
        else if(errorCodeList.ErrCode1 == 23)
        {
          lcd.setCursor(0, 1);
          lcd.print("Type2 // Sensor3");
        }
        else
        {
          lcd.setCursor(0, 1);
          lcd.print("UNK // UNKNOWN  ");
        }

        errorCodeList.rebootCount = -1;
        AddErrorToList(errorTemp);
        
        while(1)
        {
          digitalWrite(AlertSecLED, HIGH);
          digitalWrite(AlertLED, LOW);
          delay(100);
          digitalWrite(AlertSecLED, LOW);
          digitalWrite(AlertLED, LOW);
          delay(50);
          digitalWrite(AlertSecLED, LOW);
          digitalWrite(AlertLED, HIGH);
          delay(50);
          digitalWrite(AlertSecLED, LOW);
          digitalWrite(AlertLED, HIGH);
          delay(100);
          digitalWrite(AlertSecLED, HIGH);
          digitalWrite(AlertLED, LOW);
          delay(100);
          digitalWrite(AlertSecLED, LOW);
          digitalWrite(AlertLED, LOW);
          delay(50);
          digitalWrite(AlertSecLED, LOW);
          digitalWrite(AlertLED, HIGH);
          delay(50);
          digitalWrite(AlertSecLED, LOW);
          digitalWrite(AlertLED, HIGH);
          delay(100);
          tone(buzzerPin, 4000, 250);
          delay(500);
        }
      }
      else
      {
        resetFunc();
        AddErrorToList(errorTemp);
      }

      //wdt_disable();
    }
  }
}

void FetchTemps()
{
   if ((unsigned long)(currentMillis - ftMillis) >= 500) 
   {
      char isAllConvert = 0;
      
      sensorsA.requestTemperatures();
      sensorsB.requestTemperatures();

      if(!sensorsA.isConnected(SensorHeater) || !sensorsA.isConnected(SensorBoiler) || !sensorsB.isConnected(SensorSecurity))
      {
        if(isNotConCount == 0)
        {
          checkSensorDelay = currentMillis;
          isNotConCount++;
        }
        
        if((unsigned long)(currentMillis - checkSensorDelay) >= 5000 && (unsigned long)(currentMillis - checkSensorDelay) <= 10000 && isNotConCount == 1) 
        {
          missingType = 1;
          isSensor1Missing = true;
          isAlertSensors = true;
        }
        
        /*if(isNotConCount == 3)
        {
          missingType = 1;
          isSensor1Missing = true;
          isAlertSensors = true;
        }
        
        isNotConCount++;*/
      }
      
      if(sensorsA.isConversionAvailable(SensorHeater))
      {
        TempHeater = sensorsA.getTempC(SensorHeater);
        
        isAllConvert++;
      }

      if(sensorsA.isConversionAvailable(SensorBoiler))
      {
        TempBoiler = sensorsA.getTempC(SensorBoiler);

        isAllConvert++;
      }

      if(sensorsB.isConversionAvailable(SensorSecurity))
      {
        TempSecurity = sensorsB.getTempC(SensorSecurity);

        isAllConvert++;
      }

      if(isAllConvert == 3)
      {
        isConvertOK = true;
      }

      ftMillis = currentMillis;
   }
}

void CheckHeater()
{
  if(TStatus == true || isBoilerOn == true)
  {
    if(TempHeater < theHeaterTemps.Min)
    {
      SetRelay(3, true);
      isHeaterOn = true;
      heaterStopByTemp = false;
      digitalWrite(BurnerLED, HIGH);
    }
    else if(TempHeater > theHeaterTemps.Max)
    {
      SetRelay(3, false);
      if(isHeaterOn == true)
      {
        wasHeaterON = true;
      }
      heaterStopByTemp = true;
      isHeaterOn = false;
      digitalWrite(BurnerLED, LOW);
    }
  }
  else
  { 
    if(isHeaterOn == true)
    {
      wasHeaterON = true;
      heaterStopByTemp = false;
    }
    SetRelay(3, false);
    isHeaterOn = false;
    digitalWrite(BurnerLED, LOW);
  }
}

void CheckBoiler()
{
  if(isC2Ov == false)
  {
    if(TempBoiler < theBoilerTemps.Min)
    {
      isBoilerOn = true;
      SetRelay(2, true);
      cooldownTimeOut = false;
      
      digitalWrite(C2ButtonLED, HIGH);
      isC2ON = true;
    }
    else if(TempBoiler > theBoilerTemps.Max)
    {
      if(isBoilerOn == true)
      {
        wasC2ON = true;
        circDelay = currentMillis;
      }
      
      isBoilerOn = false;
      isC2ON = false;
      cooldownTimeOut = false;
      
      if(wasHeaterON == true || isHeaterOn == true)
      {
        isCooldownC2 = true;
      }
      else
      {
        isCooldownC2 = false;
      }
      
      if(isEditingMode == false)
      {
        //digitalWrite(C2ButtonLED, LOW);
      }
      else
      {
        digitalWrite(C2ButtonLED, HIGH);
      }
    }
    
    if(wasC2ON == true && isCooldownC2 == true && heaterStopByTemp == false && cooldownTimeOut == false)
    {
      if ((unsigned long)(currentMillis - circDelay) >= CIRDELAY) 
      {
        SetRelay(2, false);
        wasC2ON = false;
        isCooldownC2 = false;
        cooldownTimeOut = true;
        
        if(wasHeaterON == true)
        {
          wasHeaterON = false;
        }
        
        digitalWrite(C2ButtonLED, LOW);
      }

      //Serial.println("CDTO = false & HSBT = false");
    }
    else if(wasC2ON == true && isCooldownC2 == true && heaterStopByTemp == true && cooldownTimeOut == false)
    {
      SetRelay(2, false);
      wasC2ON = false;
      isCooldownC2 = false;
      wasHeaterON = false;
      digitalWrite(C2ButtonLED, LOW);

      //Serial.println("CDTO = false & HSBT = true");
    }
    else if(wasC2ON == true && isC2ON == false && heaterStopByTemp == true)
    {
      SetRelay(2, false);
      wasC2ON = false;
      isCooldownC2 = false;
      wasHeaterON = false;
      digitalWrite(C2ButtonLED, LOW);

      //Serial.println("CDTO = true & HSBT = true");
    }
  }
  else
  {
    isBoilerOn = false;
    SetRelay(2, false);
    isC2ON = false;
    isCooldownC2 = false;
    wasHeaterON = false;
  }
}

void CheckThermo()
{
  if(isC1Ov == false)
  {
    if(digitalRead(Thermostat) == LOW)
    {
      TStatus = true;
      SetRelay(1, true);
      digitalWrite(C1ButtonLED, HIGH);
      isC1ON = true;
    }
    else
    {
      TStatus = false;
      SetRelay(1, false);
      isC1ON = false;
      if(isEditingMode == false)
      {
        digitalWrite(C1ButtonLED, LOW);
      }
      else
      {
        digitalWrite(C1ButtonLED, HIGH);
      }
    }
  }
  else
  {
    TStatus = false;
    SetRelay(1, false);
    isC1ON = false;
  }
}

void ShowScreen()
{
  if ((unsigned long)(currentMillis - refreshLCD) >= 500) 
  {
    if(isFirstClear == true)
    {
      lcd.clear();
      isFirstClear = false;
    }
    
    lcd.setCursor(0, 0);
    lcd.write((byte)6);
    lcd.print(" ");
    lcd.print(TempHeater, 1);
    lcd.write((byte)0);
    lcd.print("  ");
    lcd.write((byte)5);
    lcd.print(" ");
    lcd.print(TempBoiler, 1);
    lcd.write((byte)0);
    lcd.setCursor(0, 1);
    lcd.write((byte)4);
    lcd.print(" ");
    lcd.print(TempSecurity, 1);
    lcd.write((byte)0);
    lcd.print("  ");

    if(TStatus == true)
    {
      lcd.print((char)0xB7);
    }
    else
    {
      lcd.print(" ");
    }

    if(isHeaterOn == true)
    {
      lcd.print((char)0x2A);
      lcd.print(" ");
    }
    else
    {
      lcd.print("  ");
    }
  
    if(isC1Ov == false && isC1ON == true)
    {
      lcd.write((byte)1);
    }
    else
    {
      lcd.print(" ");
    }
  
    if(isC2Ov == false && isC2ON == true)
    {
      lcd.write((byte)2);
      lcd.print(" ");
    }
    else
    {
      lcd.print("  ");
    }
  
    if(isConvertOK == true)
    {
      isConvertOK = false;
      lcd.write((byte)3);
    }
    else
    {
      lcd.print(" ");
    }
  
    /*Serial.print("Boiler : ");
    Serial.println(TempHeater, DEC);
    Serial.print("Hot Water : ");
    Serial.println(TempBoiler, DEC);
    Serial.print("Security : ");
    Serial.println(TempSecurity, DEC);
    Serial.print("Status :");
  
    if(isC1Ov == false && isC1ON == true)
    {
      Serial.print(" C1");
    }
    
    if(isC2Ov == false && isC2ON == true)
    {
      Serial.print(" C2");
    }
    
    Serial.println(" ");
    Serial.println("------------------");
    Serial.println(" ");
    Serial.println(" ");*/

    refreshLCD = currentMillis;
   }
}

void ShowErrScreen()
{
  
}

void SetRelay(short ID, bool ST)
{
  short pinNum;

  if(ID == 1)
  {
    pinNum = Pump1;
  }
  else if(ID == 2)
  {
    pinNum = Pump2;
  }
  else if(ID == 3)
  {
    pinNum = Burner;
  }
  
  if(ST == true)
  {
    digitalWrite(pinNum, HIGH);
  }
  else
  {
    digitalWrite(pinNum, LOW);
  }
}

void ResetRelays()
{
  digitalWrite(Pump1, LOW);
  digitalWrite(Pump2, LOW);
  digitalWrite(Burner, LOW);
}

void ResetRelaysExBur()
{
  digitalWrite(Pump1, LOW);
  digitalWrite(Pump2, LOW);
  //digitalWrite(Burner, LOW);
}

void ResetButtonCheck()
{
  if(isLCDOn == false && digitalRead(resetButtonAct) == LOW)
  {
    lcd.setBacklight(HIGH);
    isLCDOn = true;
    backLightLCD = currentMillis;
  }
  
  if(isEditingMode == false)
  {
    if(digitalRead(resetButtonAct) == LOW)
    {
      isActionDone = false;
    }
    else
    {
      if (((unsigned long)(currentMillis - resetMillis) >= 1500) && ((unsigned long)(currentMillis - resetMillis) < 4000) && isActionDone == false) 
      {
        digitalWrite(resetButtonLED, LOW);
        tone(buzzerPin, 4000, 15);
        isEditingMode = true;
        isActionDone = true;
      }
      else if ((unsigned long)(currentMillis - resetMillis) >= 4000 && isActionDone == false) 
      {
        digitalWrite(resetButtonLED, LOW);
        tone(buzzerPin, 4000, 15);
        delay(100);
        digitalWrite(resetButtonLED, HIGH);
        tone(buzzerPin, 4000, 15);
        delay(100);
        digitalWrite(resetButtonLED, LOW);
        delay(100);
        digitalWrite(resetButtonLED, HIGH);
        delay(100);
        digitalWrite(resetButtonLED, LOW);
        delay(100);
        digitalWrite(resetButtonLED, HIGH);
        delay(100);
        digitalWrite(resetButtonLED, LOW);
        resetFunc();
      }
  
      resetMillis = currentMillis;
    }
  }
  else
  {
    if(digitalRead(resetButtonAct) == LOW)
    {
      if ((unsigned long)(currentMillis - debounceMillisS) >= 500) 
      {
        if(editType == 1)
        {
          tone(buzzerPin, 4000, 10);
          editStep++;
          
          if(editStep == 2)
          {
            SaveHeaterTemp();
            editStep = 0;
            editType = 0;
            isEditingMode = false;
            ShowSaveScreen();
            digitalWrite(resetButtonLED, HIGH);
            tone(buzzerPin, 3000, 50);
            delay(50);
            tone(buzzerPin, 3500, 50);
          }
        }
        else if(editType == 2)
        {
          tone(buzzerPin, 4000, 10);
          editStep++;
          if(editStep == 2)
          {
            SaveBoilerTemp();
            editStep = 0;
            editType = 0;
            isEditingMode = false;
            ShowSaveScreen();
            digitalWrite(resetButtonLED, HIGH);
            tone(buzzerPin, 3000, 50);
            delay(50);
            tone(buzzerPin, 3500, 50);
          }
        }
  
        debounceMillisS = currentMillis;
      }
      else
      {
        debounceMillisS = currentMillis;
      }
    }
  }
}

void ButtonLEDStatus()
{
  if(isEditingMode == false)
  {
    if(isC1Ov == true)
    {
      if(ButLEDSt == 0)
      {
        digitalWrite(C1ButtonLED, LOW);
      }
      else if(ButLEDSt == 1)
      {
        digitalWrite(C1ButtonLED, HIGH);
      }
    }
  
    if(isC2Ov == true)
    {
      if(ButLEDSt == 0)
      {
        digitalWrite(C2ButtonLED, LOW);
      }
      else if(ButLEDSt == 1)
      {
        digitalWrite(C2ButtonLED, HIGH);
      }
    }
  
    if ((unsigned long)(currentMillis - previousMillis) >= 250) 
    {
      ButLEDSt++;
      previousMillis = currentMillis;
    }
  
    if(ButLEDSt > 1)
    {
      ButLEDSt = 0;
    }
  }
  else
  {
    digitalWrite(C1ButtonLED, HIGH);
    digitalWrite(C2ButtonLED, HIGH);
  }
}

// Editing Mode

void ShowEditScreen()
{
  char  str[8]; 
  lcd.setCursor(0, 0);
  lcd.print(">> EDIT SETTINGS");
  lcd.setCursor(0, 1);

  if(editType == 0)
  {
    lcd.print("CH(C1) or BA(C2)");
  }
  else if(editType == 1)
  {
    if(editStep == 0)
    {
      lcd.print("Min CH : ");
      sprintf(str, "%-7d", (int)round(theHeaterTemps.Min));
      lcd.print(str);
    }
    else if(editStep == 1)
    {
      lcd.print("Max CH : ");
      sprintf(str, "%-7d", (int)round(theHeaterTemps.Max));
      lcd.print(str);
    }
  }
  else if(editType == 2)
  {
    if(editStep == 0)
    {
      lcd.print("Min BA : ");
      sprintf(str, "%-7d", (int)round(theBoilerTemps.Min));
      lcd.print(str);
    }
    else if(editStep == 1)
    {
      lcd.print("Max BA : ");
      sprintf(str, "%-7d", (int)round(theBoilerTemps.Max));
      lcd.print(str);
    }
  }
}

void ShowSaveScreen()
{
  char  str[8]; 
  lcd.setCursor(0, 0);
  lcd.print(">> EDIT SETTINGS");
  lcd.setCursor(0, 1);
  lcd.write((byte)3);
  lcd.print("  Saved         ");
  delay(1000);
}

// Save data for reuse

void SaveAlert()
{
  AlertStatus alerts = {
    isAlertSecurity,
    isAlert
  };

  if(alerts.isAlertSec != actualAlert.isAlertSec || alerts.isAlertBur != actualAlert.isAlertBur)
  {
    actualAlert.isAlertSec = isAlertSecurity;
    actualAlert.isAlertBur = isAlert;
    
    eeAddress = 0;
    EEPROM.put(eeAddress, actualAlert);
    //Serial.println("Alert Written EEP");
  }
}

void SaveOverride()
{
  OverrideStatus overrides = {
    isC1Ov,
    isC2Ov
  };

  eeAddress = sizeof(AlertStatus);

  EEPROM.put(eeAddress, overrides);
}

void SaveBoilerTemp()
{
  eeAddress = sizeof(AlertStatus) + sizeof(OverrideStatus);

  EEPROM.put(eeAddress, theBoilerTemps);
}

void SaveHeaterTemp()
{
  eeAddress = sizeof(AlertStatus) + sizeof(OverrideStatus) + sizeof(theBoilerTemps);

  EEPROM.put(eeAddress, theHeaterTemps);
}

void AddErrorToList(short errCode)
{
  eeAddress = sizeof(AlertStatus) + sizeof(OverrideStatus) + sizeof(theBoilerTemps) + sizeof(theHeaterTemps);

  errorCodeList.ErrCode3 = errorCodeList.ErrCode2;
  errorCodeList.ErrCode2 = errorCodeList.ErrCode1;
  errorCodeList.ErrCode1 = errCode;
  errorCodeList.rebootCount++;

  EEPROM.put(eeAddress, errorCodeList);

  Serial.println(errorCodeList.rebootCount);
}

// SECURITY TEMP CHECK

void SecurTemp()
{
  if (TempSecurity < TSMin) 
  {
    TSMin = TempSecurity;
  }

  if (TempSecurity > TSMax) 
  {
    TSMax = TempSecurity;
  }
  
  if(TempSecurity >= MAXTEMP && isHeaterOn == true)
  {
    isAlertSecurity = true;
    SaveAlert();
  }
}
