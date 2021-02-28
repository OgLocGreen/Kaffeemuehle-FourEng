#include <Wire.h>
#include <EEPROM.h> //Needed to record user settings
#include "SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h" // Click here to get the library: http://librarymanager/All#SparkFun_NAU8702
#include <string.h>
#include <time.h>
#include <Wire.h>

NAU7802 myScale; //Create instance of the NAU7802 class

//EEPROM locations to store 4-byte variables
#define LOCATION_CALIBRATION_FACTOR 0 //Float, requires 4 bytes of EEPROM
#define LOCATION_ZERO_OFFSET 10 //Must be more than 4 away from previous spot. Long, requires 4 bytes of EEPROM

bool settingsDetected = false; //Used to prompt user to calibrate their scale

//Create an array to take average of weights. This helps smooth out jitter.
#define AVG_SIZE 20
float avgWeights[AVG_SIZE];
byte avgWeightSpot = 0;

//#define RXD 1
//#define TXD 3
//#define RXD2 16
//#define TXD2 17
#define pin_PWM 2
#define pin_home 12
#define pin_start 13

#define z_home 1
#define z_wiegen 2
#define z_fertig 3
#define z_settings 4 
#define z_abbruch 5
#define z_statistik 6

bool var_set = 0;
bool var_stop = 0;
bool var_start = 0;
bool var_plus = 0;
bool var_minus = 0;
bool var_1kaffee = 0;
bool var_2kaffee = 0;
bool var_statistik = 0;

bool var_gewichtoderrpm = 1; // 1 Gewicht 0 rpm
bool var_1oder2 = 1;        // 1 1Kaffee 0 2Kaffee
int var_gewicht_1kaffee = 30;
int var_rpm_1kaffee = 1000;
int var_gewicht_2kaffee = 35;
int var_rpm_2kaffee = 2000;
bool finish = 0;


float gewicht;
float rpm;
int numb;
String cmd = "\"";
int Zustand = 1;
int Zustand_alt = 0;
//Scale
void calibrateScale(void);
void recordSystemSettings(void);
void readSystemSettings(void);
//Funktionen
float wiegen(void);
bool malen(int gewicht, int rpm, int zielgewicht);
void reset(void);

//Screen
void ScreenInit(void);
void ScaleInit(void);
void screenWritingtext(String text);
void screenwiegen(float aktuellgewicht, int zielgewicht);
void screensettinggewicht(int zielgewicht);
void screengewichtoderrpm(bool gewichtoderrpm);
void screensettingrpm(int rpm);
void screen1kaffeeoder2kaffee(bool kaffee1oder2);
void screenfertig(void);

//Pages
void page_statistik(void);
void page_abbruch(void);
void page_set(void);
void page_main(void);


void setup()
{
  Serial.begin(9600);
  //Serial1.begin(9600, RXD, TXD);  //Screen
  //Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); // Arduino
  Serial1.begin(9600);  //Screen
  Serial2.begin(9600);  //Arduino

  ScaleInit();
  ScreenInit();

  pinMode(pin_home, INPUT);
  pinMode(pin_start, INPUT);


}

void loop()
{
  Serial.print(Zustand);
  Serial.print("\n");

  switch (Zustand)
  {
  case z_home:
    screenWritingtext("Bereit!");
    if(Zustand_alt != Zustand) 
    { 
      Zustand_alt = Zustand;
      page_main();
      screenWritingtext("Bereit!");
    }
    if(var_set) Zustand = z_settings;
    if(var_1kaffee || var_2kaffee)
    {
      Zustand = z_wiegen;
      if (var_1kaffee) var_1oder2 = 1;
      else var_1oder2 = 0;
    }
    if(var_statistik) Zustand = z_statistik;
      break;
  case z_wiegen: // Wiegen
    if(Zustand_alt != Zustand)
    {
      Zustand_alt = Zustand;
      page_main();
    }
    //if(myScale.available() == false) 
    //{
    //  Zustand = 0;
    //  break;
    //}
    if(var_stop) Zustand = z_abbruch;

    if(var_1oder2 == 1) // 1Kaffee
    {
      gewicht = wiegen();
      finish = malen(int(gewicht),  var_rpm_1kaffee,  var_gewicht_1kaffee);
      if(finish) Zustand = z_fertig;
      screenwiegen(gewicht,  var_gewicht_1kaffee);
    }
    else   //2Kaffee
    {  
      gewicht = wiegen();
      finish = malen(int(gewicht),  var_rpm_2kaffee,  var_gewicht_2kaffee);
      if(finish) Zustand = z_fertig;
      screenwiegen( gewicht,  var_gewicht_2kaffee);
    }
    break;
  case z_fertig: // Fertig
    if(Zustand_alt != Zustand)
    {
      Zustand_alt = Zustand;
      page_main();
    }
    screenfertig();
    gewicht = wiegen();
    if (int(gewicht) <= 10) Zustand = 1;
    if (var_stop) Zustand = 1;
    break;
  case z_settings: // Setting
    if(Zustand_alt != Zustand)
    {
      Zustand_alt = Zustand;
      page_set();
    }
    screen1kaffeeoder2kaffee(var_1oder2);

    if (var_stop) Zustand = z_home;
    if (var_set) var_gewichtoderrpm = !var_gewichtoderrpm;
    if (var_1kaffee) var_1oder2 = true;
    if (var_2kaffee) var_1oder2 = false;
    if (var_1oder2 == 1) // kaffee 1
    {
      screensettinggewicht(var_gewicht_1kaffee);
      screensettingrpm(var_rpm_1kaffee);
      screengewichtoderrpm(var_gewichtoderrpm);
      if(var_gewichtoderrpm == 1) // Gewicht
      { 
        if (var_plus) var_gewicht_1kaffee +=1;
        if (var_minus) var_gewicht_1kaffee -=1;
      }
      else //rpm
      { 
        if (var_plus) var_rpm_1kaffee +=1;
        if (var_minus) var_rpm_1kaffee -=1;
      }
    }
    else
    {
      screensettinggewicht(var_gewicht_2kaffee);
      screensettingrpm(var_rpm_2kaffee);
      screengewichtoderrpm(var_gewichtoderrpm);
      if(var_gewichtoderrpm == 1) // Gewicht
      {
        if (var_plus) var_gewicht_2kaffee +=1;
        if (var_minus) var_gewicht_2kaffee -=1;
      }
      else //rpm
      { 
        if (var_plus) var_rpm_2kaffee +=1;
        if (var_minus) var_rpm_2kaffee -=1;
      } 
    }
    break;
  case z_abbruch: // Abbruch
    if(Zustand_alt != Zustand)
    {
      Zustand_alt = Zustand;
      page_abbruch();
    }
    if(var_stop) Zustand = z_home;
    break;
  case z_statistik: // Statistik
    break;    
  default: // Error
    Serial.println("Error");
    if (var_stop) Zustand = z_home;
    break;
  }


  reset();


  if (Serial.available())
  {
    while (Serial.available()) Serial.read(); //Clear anything in RX buffer
    while (Serial.available() == 0) delay(10); //Wait for user to press key
    byte incoming = Serial.read();
    if (incoming == 't') //Tare the scale
      myScale.calculateZeroOffset();
    else if (incoming == 'c') //Calibrate
    {
      calibrateScale();
    }
    else if (incoming == 's')
    {
      var_set = 1;
      var_stop = 0;
      var_start= 0;
      var_plus= 0;
      var_minus= 0;
      var_1kaffee= 0;
      var_2kaffee= 0;
      var_statistik= 0;

    }
    else if (incoming == 'z')
    {
      var_set = 0;
      var_stop = 1;
      var_start = 0;
      var_plus = 0;
      var_minus = 0;
      var_1kaffee = 0;
      var_2kaffee = 0;
      var_statistik = 0;
    }
    else if (incoming == '1')
    {
      var_set = 0;
      var_stop = 0;
      var_start = 0;
      var_plus = 0;
      var_minus = 0;
      var_1kaffee = 1;
      var_2kaffee = 0;
      var_statistik = 0;
    }
    else if (incoming == '2')
    {
      var_set  = 0;
      var_stop  = 0;
      var_start = 0;
      var_plus = 0;
      var_minus = 0;
      var_1kaffee = 0;
      var_2kaffee = 1;
      var_statistik = 0;
    }
    else if (incoming == 'p')
    {
      var_set = 0;
      var_stop = 0;
      var_start = 0;
      var_plus = 1;
      var_minus = 0;
      var_1kaffee = 0;
      var_2kaffee = 0;
      var_statistik = 0;
    }
    else if (incoming == 'm')
    {
      var_set = 0;
      var_stop  = 0;
      var_start = 0;
      var_plus = 0;
      var_minus = 1;
      var_1kaffee = 0;
      var_2kaffee = 0;
      var_statistik = 0;
    }
  }
}

//Gives user the ability to set a known weight on the scale and calculate a calibration factor
void calibrateScale(void)
{
  Serial.println();
  Serial.println();
  Serial.println(F("Scale calibration"));

  Serial.println(F("Setup scale with no weight on it. Press a key when ready."));
  while (Serial.available()) Serial.read(); //Clear anything in RX buffer
  while (Serial.available() == 0) delay(10); //Wait for user to press key

  myScale.calculateZeroOffset(64); //Zero or Tare the scale. Average over 64 readings.
  Serial.print(F("New zero offset: "));
  Serial.println(myScale.getZeroOffset());

  Serial.println(F("Place known weight on scale. Press a key when weight is in place and stable."));
  while (Serial.available()) Serial.read(); //Clear anything in RX buffer
  while (Serial.available() == 0) delay(10); //Wait for user to press key

  Serial.print(F("Please enter the weight, without units, currently sitting on the scale (for example '4.25'): "));
  while (Serial.available()) Serial.read(); //Clear anything in RX buffer
  while (Serial.available() == 0) delay(10); //Wait for user to press key

  //Read user input
  float weightOnScale = Serial.parseFloat();
  Serial.println();

  myScale.calculateCalibrationFactor(weightOnScale, 64); //Tell the library how much weight is currently on it
  Serial.print(F("New cal factor: "));
  Serial.println(myScale.getCalibrationFactor(), 2);

  Serial.print(F("New Scale Reading: "));
  Serial.println(myScale.getWeight(), 2);

  recordSystemSettings(); //Commit these values to EEPROM
}

//Record the current system settings to EEPROM
void recordSystemSettings(void)
{
  //Get various values from the library and commit them to NVM
  EEPROM.put(LOCATION_CALIBRATION_FACTOR, myScale.getCalibrationFactor());
  EEPROM.put(LOCATION_ZERO_OFFSET, myScale.getZeroOffset());
}

//Reads the current system settings from EEPROM
//If anything looks weird, reset setting to default value
void readSystemSettings(void)
{
  float settingCalibrationFactor; //Value used to convert the load cell reading to lbs or kg
  long settingZeroOffset; //Zero value that is found when scale is tared

  //Look up the calibration factor
  EEPROM.get(LOCATION_CALIBRATION_FACTOR, settingCalibrationFactor);
  if (settingCalibrationFactor == 0xFFFFFFFF)
  {
    settingCalibrationFactor = 0; //Default to 0
    EEPROM.put(LOCATION_CALIBRATION_FACTOR, settingCalibrationFactor);
  }

  //Look up the zero tare point
  EEPROM.get(LOCATION_ZERO_OFFSET, settingZeroOffset);
  if (settingZeroOffset == 0xFFFFFFFF)
  {
    settingZeroOffset = 1000L; //Default to 1000 so we don't get inf
    EEPROM.put(LOCATION_ZERO_OFFSET, settingZeroOffset);
  }

  //Pass these values to the library
  myScale.setCalibrationFactor(settingCalibrationFactor);
  myScale.setZeroOffset(settingZeroOffset);

  settingsDetected = true; //Assume for the moment that there are good cal values
  if (settingCalibrationFactor < 0.1 || settingZeroOffset == 1000)
    settingsDetected = false; //Defaults detected. Prompt user to cal scale.
}


float wiegen(void)
{
  long currentReading = myScale.getReading();
  float currentWeight = myScale.getWeight();

  Serial.print("\tReading: ");
  Serial.print(currentReading);
  Serial.print("\tWeight: ");
  Serial.print(currentWeight, 2); //Print 2 decimal places

  avgWeights[avgWeightSpot++] = currentWeight;
  if(avgWeightSpot == AVG_SIZE) avgWeightSpot = 0;


  // Mitteln von AVG_SIZE Werten
  float avgWeight = 0;
  for (int x = 0 ; x < AVG_SIZE ; x++)
    avgWeight += avgWeights[x];
  avgWeight /= AVG_SIZE;


  Serial.print("\tAvgWeight: ");
  Serial.print(avgWeight, 2); //Print 2 decimal places
  //if(settingsDetected == false)
  //{
  //  Serial.print("\tScale not calibrated. Press 'c'.");
  //}
  Serial.println();

  return avgWeight;
}


float gramm2rpm(int gramm)
{
  return gramm*100;
}


void ScreenInit(void)
{
    // Bildschim initialisieren
  page_main();
  Serial1.print("t0.txt=" + cmd + " " + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
  Serial1.print("t0.txt=" + cmd + " " + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF); 
}

void ScaleInit(void)
{
  Wire.begin();
  Wire.setClock(400000); //Qwiic Scale is capable of running at 400kHz if desired

  if (myScale.begin() == false)
  {
    Serial.println("Scale not detected. Please check wiring. Freezing...");
    while (1);
  }
  Serial.println("Scale detected!");

  readSystemSettings(); //Load zeroOffset and calibrationFactor from EEPROM

  myScale.setSampleRate(NAU7802_SPS_320); //Increase to max sample rate
  myScale.calibrateAFE(); //Re-cal analog front end when we change gain, sample rate, or channel 

  Serial.print("Zero offset: ");
  Serial.println(myScale.getZeroOffset());
  Serial.print("Calibration factor: ");
  Serial.println(myScale.getCalibrationFactor());
}


// Für jeden Zustand wird eine Seite erstellt.

void page_main(void) {
  Serial1.print("page page0");
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
}

void page_set(void) {
  Serial1.print("page page1");
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);

}

void page_abbruch(void) {
  Serial1.print("page page2");
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);

}

void page_statistik(void) {
  Serial1.print("page page3");
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
}

void screenwiegen(float aktuellgewicht, int zielgewicht)
{
  
  Serial.print("Wiegen");
  Serial.print("\n");
  // Bildschim initialisieren
  Serial1.print("t0.txt=" + cmd + aktuellgewicht + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
}

void screenWritingtext(String text)
{
  Serial.print("Home");
  Serial.print("\n");
  // Bildschim initialisieren
  Serial1.print("t0.txt=" + cmd + text + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
}


void screenfertig(void)
{
  Serial.print("Fertig");
  Serial.print("\n");
  // Bildschim initialisieren
  Serial1.print("t0.txt=" + cmd + "Fertig!" + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
}

void screensettinggewicht(int zielgewicht)
{
  Serial.print("Setting gewicht");
  Serial.print("\n");
  // Bildschim initialisieren
  Serial1.print("t3.txt=" + cmd + zielgewicht + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
  Serial1.print("t3.txt=" + cmd + zielgewicht + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF); 
}

void screensettingrpm(int rpm)
{
  Serial.print("Setting RPM");
  Serial.print("\n");

  // Bildschim initialisieren
  Serial1.print("t4.txt=" + cmd + rpm + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
}

void screengewichtoderrpm(bool gewichtoderrpm)
{
  if (gewichtoderrpm)
  {

    // Bildschim initialisieren
    Serial1.print("p0.pic=" + cmd + 1 + cmd);
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);

    Serial1.print("p1.pic=" + cmd + 0 + cmd);
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);


    // Bildschim initialisieren
    Serial1.print("t1.bco=40179");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);
    Serial1.print("t3.bco=40179");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);
    Serial1.print("t2.bco=65525");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);
    Serial1.print("t4.bco=65525");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);
  }
  else
  {
    // Bildschim initialisieren
    Serial1.print("p0.pic="  + cmd + 0 + cmd);
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);

    Serial1.print("p1.pic=" + cmd + 1 + cmd);
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);

    // Bildschim initialisieren
    Serial1.print("t1.bco=65525");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);
    Serial1.print("t3.bco=65525");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);
    Serial1.print("t2.bco=40179");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);
    Serial1.print("t4.bco=40179");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF); 
  }
  

}

void screen1kaffeeoder2kaffee(bool kaffee1oder2)
{
  if (kaffee1oder2)
  {
    // Bildschim initialisieren
    Serial1.print("b3.bco=40179");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);
    Serial1.print("b4.bco=0");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF); 
  }
  else{
    // Bildschim initialisieren
    Serial1.print("b3.bco=0");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);
    Serial1.print("b4.bco=40179");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF); 
  }
}


bool malen(int gewicht, int rpm, int zielgewicht)
{
  int volt = rpm/(3000/255); //Mahllogic
  

  if (gewicht > zielgewicht)
  {
    analogWrite(pin_PWM,0);
    return true;
  }
  else 
  {
    analogWrite(pin_PWM,volt);
    return false;
  }
  if (1 == 0)
  {
    return true;
  }
}


void reset(void)
{
  var_set = 0;
  var_stop = 0;
  var_start= 0;
  var_plus= 0;
  var_minus= 0;
  var_1kaffee= 0;
  var_2kaffee= 0;
  var_statistik= 0;
}