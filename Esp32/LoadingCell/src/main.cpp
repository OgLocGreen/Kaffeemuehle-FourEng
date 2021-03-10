#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h> //Needed to record user settings
#include "SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h" // Click here to get the library: http://librarymanager/All#SparkFun_NAU8702
#include <string.h>
#include <time.h>
#include <Wire.h>
#include <BME280_t.h>




NAU7802 myScale; //Create instance of the NAU7802 class

//EEPROM locations to store 4-byte variables
#define LOCATION_CALIBRATION_FACTOR 0 //Float, requires 4 bytes of EEPROM
#define LOCATION_ZERO_OFFSET 5 //Must be more than 4 away from previous spot. Long, requires 4 bytes of EEPROM
#define LOCATION_Siebtraeger 10
#define LOCATION_gewicht_1 15
#define LOCATION_rpm_1 20
#define LOCATION_gewicht_2 25
#define LOCATION_rpm_2 30
#define LOCATION_Throughput_number 35
#define LOCATION_first_start 40

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
#define pin_set 13
#define pin_statistik 12
#define pin_stop_zuruck 11
#define pin_1kaffe 10
#define pin_2kaffe 9
#define pin_cali 8
#define pin_minus 7
#define pin_plus 6

#define z_home 1
#define z_wiegen 2
#define z_fertig 3
#define z_settings 4 
#define z_abbruch 5
#define z_statistik 6
#define z_calib 7

bool var_set = 0;
bool var_stop = 0;
bool var_cali = 0;
bool var_plus = 0;
bool var_minus = 0;
bool var_1kaffee = 0;
bool var_2kaffee = 0;
bool var_statistik = 0;

bool var_set_alt = 0;
bool var_stop_alt = 0;
bool var_cali_alt = 0;
bool var_plus_alt = 0;
bool var_minus_alt = 0;
bool var_1kaffee_alt = 0;
bool var_2kaffee_alt = 0;
bool var_statistik_alt = 0;

bool var_set_release = 0;
bool var_stop_release = 0;
bool var_cali_release = 0;
bool var_plus_release = 0;
bool var_minus_release = 0;
bool var_1kaffee_release = 0;
bool var_2kaffee_release = 0;
bool var_statistik_release = 0;

int var_gewichtoderrpm = 0; // 1 Gewicht 0 rpm
bool var_1oder2 = 1;        // 1 1Kaffee 0 2Kaffee
float var_gewicht_1kaffee = 30;
float var_rpm_1kaffee = 1000;
float var_gewicht_2kaffee = 35;
float var_rpm_2kaffee = 2000;
bool finish = 0;
bool press_stop_again=false;

int var_thoughput_number = 0;

bool var_automode=false; // True=>automatischer start bei Erkennung des leeren Siebträgers
float var_Siebtraeger_leer=0;
float gewicht_Delta_ok=0.5; //Gewichtsabweichung für Fertig, Startklar...
bool first_start;

float gewicht;
float rpm;
int numb;
String cmd = "\"";
int Zustand = 1;
int Zustand_alt = 0;
int Calib_schritt=0;

// variables will change:
// variable for reading the pushbutton status
int buttonState_up = 0;
int buttonState_down = 0; 
// value is controled by the two buttons
int value=0;
// used to meassure how long the buttons a pressd
unsigned long time_up;
unsigned long time_down;
// inkrement to change the value
int inkrement=1;
// used to adjust the inkrement
int i=0;


//BME Sensor
#define ASCII_ESC 27
#define MYALTITUDE  150.50
char bufout[10];
BME280<> BMESensor; 

//Taster input
void read_taster(void);
void test_taster(void);
bool release(int pin, bool alter_zustand);
//Scale
//void calibrateScale(void);
void recordSystemSettings(void);
void readSystemSettings(void);

//Bme Sensor
void BmeInit(void);


//Funktionen
float wiegen(void);
bool mahlen(float gewicht, float rpm, float zielgewicht);
void reset(void);
void zurueck(void);



//Screen
void ScreenInit(void);
void ScaleInit(void);
void screenWritingtext(String text);
void screenwiegen(float aktuellgewicht, float zielgewicht);
void screensettinggewicht(float zielgewicht);
void screengewichtoderrpm(int gewichtoderrpm);
void screensettingrpm(float rpm);
void screensetting_automode(bool var_automode);
void screen1kaffeeoder2kaffee(bool kaffee1oder2);
void screenfertig(void);
void screencalibtext(void);
void screencalibgewicht(void);


//Pages
void page_statistik(void);
void page_abbruch(void);
void page_set(void);
void page_main(void);
void page_calib(void);

void setup()
{
  // Clearen von EEPROM falls var NAN

  //EEPROM.get(LOCATION_first_start, first_start);
  //if (first_start == 0)
  //{
  //  first_start += 1;
  //  EEPROM.put(LOCATION_first_start, first_start);
  //  for (int i = 0 ; i < EEPROM.length() ; i++) {
  //  EEPROM.write(i, 0);
  //}
  //}
  
  Zustand = z_home;

  Serial.begin(9600);
  Serial1.begin(9600);  //Screen
  Serial2.begin(9600);  //Arduino

  ScaleInit();
  ScreenInit();

                                               // initialize I2C that connects to sensor
  BMESensor.begin();                                                    // initalize bme280 sensor

  pinMode(pin_PWM, OUTPUT);

  pinMode(pin_set, INPUT);
  pinMode(pin_statistik, INPUT);
  pinMode(pin_stop_zuruck, INPUT);
  pinMode(pin_1kaffe, INPUT);
  pinMode(pin_2kaffe, INPUT);
  pinMode(pin_cali, INPUT);
  pinMode(pin_minus, INPUT);
  pinMode(pin_plus, INPUT);

}

void loop()
{ 
  read_taster();
  //test_taster();
  //Serial.print(Zustand);
  //Serial.print(gewicht);
  //gewicht = wiegen();
  //Serial.print(var_gewicht_2kaffee);
  Wire.begin();
  BMESensor.refresh();


  switch (Zustand)
  {
  case z_home:
    //screenWritingtext("Bereit!");
    if(Zustand_alt != Zustand) 
    { 
      Zustand_alt = Zustand;
      page_main();
      if(var_automode){
      screenWritingtext("Automode!");
      screen1kaffeeoder2kaffee(var_1oder2);
      }
      else{screenWritingtext("Bereit!");}
    }
    if(var_automode&&var_1kaffee_release){
      var_1oder2=true;
      screen1kaffeeoder2kaffee(var_1oder2);
    }
    if(var_automode&&var_2kaffee_release){
      var_1oder2=false;
      screen1kaffeeoder2kaffee(var_1oder2);
    }
    if(var_set_release) Zustand = z_settings;
    if(var_cali_release) Zustand = z_calib;
    if((var_1kaffee && !var_automode) || (var_2kaffee && !var_automode) || (var_automode && (var_Siebtraeger_leer-gewicht_Delta_ok <= gewicht && gewicht <= var_Siebtraeger_leer+gewicht_Delta_ok)))
    {
      Zustand = z_wiegen;
      if (var_1kaffee&& !var_automode) var_1oder2 = 1; //Nur Variablen Auswahl
      if (var_2kaffee&& !var_automode) var_1oder2 = 0; //Nur Variablen Auswahl
    }
    if(var_statistik) Zustand = z_statistik;
      break;
  case z_wiegen: // Wiegen
    if(Zustand_alt != Zustand)
    {
      Zustand_alt = Zustand;
      page_main();
    }
    if(var_stop) Zustand = z_abbruch;

    if(var_1oder2 == 1) // 1Kaffee
    {
      gewicht = wiegen();
      finish = mahlen(gewicht,  (var_rpm_1kaffee+var_Siebtraeger_leer),  (var_gewicht_1kaffee+var_Siebtraeger_leer));
      if(finish) Zustand = z_fertig;
      screenwiegen((gewicht - var_Siebtraeger_leer),  var_gewicht_1kaffee);
    }
    else   //2Kaffee
    {  
      gewicht = wiegen();
      finish = mahlen(gewicht,  (var_rpm_2kaffee+var_Siebtraeger_leer),  (var_gewicht_2kaffee+var_Siebtraeger_leer));
      if(finish) Zustand = z_fertig;
      screenwiegen((gewicht - var_Siebtraeger_leer),  var_gewicht_2kaffee);
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
      var_gewichtoderrpm = 0;
      page_set();
    }
    screen1kaffeeoder2kaffee(var_1oder2);

    if (var_stop){
      EEPROM.put(LOCATION_gewicht_1, var_gewicht_1kaffee);
      EEPROM.put(LOCATION_gewicht_2, var_gewicht_2kaffee);
      EEPROM.put(LOCATION_rpm_1, var_rpm_1kaffee);
      EEPROM.put(LOCATION_rpm_2, var_rpm_2kaffee);
      Zustand = z_home;
    }
    if (var_set_release) var_gewichtoderrpm = (var_gewichtoderrpm+1)%3;
    if (var_1kaffee) var_1oder2 = true;
    if (var_2kaffee) var_1oder2 = false;
    if (var_1oder2 == 1) // kaffee 1
    {
      screensettinggewicht(var_gewicht_1kaffee);
      screensettingrpm(var_rpm_1kaffee);
      screengewichtoderrpm(var_gewichtoderrpm);
      screensetting_automode(var_automode);
      if(var_gewichtoderrpm == 0) // Gewicht
      {
        if (var_plus) var_gewicht_1kaffee +=1;
        if (var_minus) var_gewicht_1kaffee -=1;

        /*
        while (digitalRead(var_plus) == HIGH)
        {
          if(value<(32767+1-inkrement)) {                 //used to not get a overflow
            value=value+inkrement;
          }
          if(value+inkrement >= 32767){value = 32767;}
          Serial.println(value);                        //"returns" Value
          time_up = millis();
          while (millis() - time_up < 1000/(i+1) and digitalRead(var_plus) == HIGH){   //Mit der While Funktion ist die Zeit am anfang Lange und am ende Kurz die er Wartet um Hochzuzählen.
            //do nothing
            inkrement=i;
          }
          i++;
        }

        // same as Counting UP but DOWN---------------
        while (digitalRead(var_minus) == HIGH)
        {
          if(value>(-32768-1+inkrement)) {
            value=value-inkrement;
          }
          else{
            value=-32768;
          }
          Serial.println(value);
          time_down = millis();
          while (millis() - time_down < 1000/(i+1) and digitalRead(var_minus) == HIGH){
            //do nothing
            inkrement=i;
          }
          i++;
        }

      //resets the inkrement and index i after realesing the buttons
      if (var_minus_release == LOW and var_plus_release == LOW) {
        i=0;
        inkrement=1;
      }
      */
    }

      if(var_gewichtoderrpm == 1) // rpm
      { 
        if (var_plus) var_rpm_1kaffee +=1;
        if (var_minus) var_rpm_1kaffee -=1;
      }
      if(var_gewichtoderrpm == 2) // Auto/Manuel mode
      {
        if (var_plus_release || var_minus_release){
          var_automode=!var_automode;
        }
      }
    }
    else
    {
      screensettinggewicht(var_gewicht_2kaffee);
      screensettingrpm(var_rpm_2kaffee);
      screengewichtoderrpm(var_gewichtoderrpm);
      screensetting_automode(var_automode);
      if(var_gewichtoderrpm == 0) // Gewicht
      {
        if (var_plus) var_gewicht_2kaffee +=1;
        if (var_minus) var_gewicht_2kaffee -=1;
      }
      if(var_gewichtoderrpm == 1) // rpm
      { 
        if (var_plus) var_rpm_2kaffee +=1;
        if (var_minus) var_rpm_2kaffee -=1;
      }
      if(var_gewichtoderrpm == 2) // Auto/Manuel mode
      {
        if (var_plus_release || var_minus_release){
          var_automode=!var_automode;
        }
      }
    }
    break;
  case z_abbruch: // Abbruch
    analogWrite(pin_PWM,0); 
    if(Zustand_alt != Zustand)
    {
      Zustand_alt = Zustand;
      page_abbruch();
      delay(3000);
    }
    if(press_stop_again&&var_stop_release){
      Zustand = z_home;
      press_stop_again=false;
    }
    if(var_stop_release) press_stop_again = true;
    break;
  case z_statistik: // Statistik
    if(Zustand_alt != Zustand)
    {
      Zustand_alt = Zustand;
      page_statistik();
    }
    if(var_stop) Zustand = z_home;
    break;
  case z_calib: // Waage Calibriern
    if(Zustand_alt != Zustand)
    {
      Zustand_alt = Zustand;
      Calib_schritt=0;
      page_calib();
    }
      switch(Calib_schritt){
        case 2:
          screenWritingtext("Gewicht entnehmen und Siebtraeger einsetzen. Mit Calib. Taste fortfahren");
          Serial.println("Gewicht entnehmen und siebträger einsetzen.");
          screencalibgewicht();

          //bei Calib taste drücken und loslassen
          if(var_cali_release){
            //Gewicht des leeren Siebträgers Speichern!!!
            var_Siebtraeger_leer=wiegen();
            Calib_schritt=3;
          }
          break;
        case 1:
          //"100 gramm einsetzen." Anzeigen
          screenWritingtext("100 gramm einsetzen. Mit Calib. Taste fortfahren");
          screencalibgewicht();

          Serial.println("100 gramm einsetzen.");
          //bei Calib taste drücken und loslassen
          if(var_cali_release){
            //Steigung anpassen!!!
            myScale.calculateCalibrationFactor(100.0, 64); //Tell the library how much weight is currently on it
            Calib_schritt=2;
          }
          break;
        case 0:
          //"Aufnahme leeren." Anzeigen
          screenWritingtext("Aufnahme leeren. Mit Calib. Taste fortfahren");
          screencalibgewicht();

          Serial.println("Aufnahme leeren.");
          //bei Calib taste drücken und loslassen 
          if(var_cali_release){
            //Nullpunkt wage Ermitteln!!!
            myScale.calculateZeroOffset(64); //Zero or Tare the scale. Average over 64 readings.
            Calib_schritt=1;
          }
          break;
        case 3:
          screenWritingtext("Fertig!");
          screencalibgewicht();
          recordSystemSettings(); //Commit these values to EEPROM
          delay(3000);
          Zustand = z_home;
          break;
        default:
          Serial.println("default in Calibrierrutine!");
      }
    break;
  default: // Error
    Serial.println("Error");
    if (var_stop) Zustand = z_home;
    break;
  }

  reset();
}

//Record the current system settings to EEPROM
void recordSystemSettings(void)
{
  //Get various values from the library and commit them to NVM
  EEPROM.put(LOCATION_CALIBRATION_FACTOR, myScale.getCalibrationFactor());
  EEPROM.put(LOCATION_ZERO_OFFSET, myScale.getZeroOffset());
  EEPROM.put(LOCATION_Siebtraeger, var_Siebtraeger_leer);
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

  //Siebträger leergewicht
  EEPROM.get(LOCATION_Siebtraeger, var_Siebtraeger_leer);
  if (var_Siebtraeger_leer == 0 || var_Siebtraeger_leer == NAN || var_Siebtraeger_leer == 0xFFFFFFFF)
  {
    var_Siebtraeger_leer = 514; //Default to 1000 so we don't get inf
    EEPROM.put(LOCATION_Siebtraeger, var_Siebtraeger_leer);
  }

  //var_gewicht_1kaffee
  EEPROM.get(LOCATION_gewicht_1, var_gewicht_1kaffee);
  if (var_gewicht_1kaffee == 0 || var_gewicht_1kaffee == NAN)
  {
    var_gewicht_1kaffee = 25; //Default to 1000 so we don't get inf
    EEPROM.put(LOCATION_gewicht_1, var_gewicht_1kaffee);
  }

  //var_gewicht_2kaffee
  EEPROM.get(LOCATION_gewicht_2, var_gewicht_2kaffee);
  if (var_gewicht_2kaffee == 0 || var_gewicht_2kaffee == NAN)
  {
    var_gewicht_2kaffee = 35; //Default to 1000 so we don't get inf
    EEPROM.put(LOCATION_gewicht_2, var_gewicht_2kaffee);
  }
  //var_rpm_1kaffee
  EEPROM.get(LOCATION_rpm_1, var_rpm_1kaffee);
  if (var_rpm_1kaffee == 0 || var_rpm_1kaffee == NAN)
  {
    var_rpm_1kaffee = 1000; //Default to 1000 so we don't get inf
    EEPROM.put(LOCATION_rpm_1, var_rpm_1kaffee);
  }
  //var_rpm_2kaffee
  EEPROM.get(LOCATION_rpm_2, var_rpm_2kaffee);
  if (var_rpm_2kaffee == 0 || var_rpm_2kaffee == NAN)
  {
    var_rpm_2kaffee = 2000; //Default to 1000 so we don't get inf
    EEPROM.put(LOCATION_rpm_2, var_rpm_2kaffee);
  }

  //Look up the calibration factor
  EEPROM.get(LOCATION_Throughput_number, var_thoughput_number);
  if (var_thoughput_number == 0 || var_thoughput_number == NAN)
  {
    var_thoughput_number = 0; //Default to 0
    EEPROM.put(LOCATION_Throughput_number, var_thoughput_number);
  }
  
  EEPROM.get(LOCATION_first_start, first_start);
  if (first_start == 0 || first_start == NAN)
  {
    first_start = 0; //Default to 0
    EEPROM.put(LOCATION_first_start, first_start);
  }

}

float wiegen(void)
{
  long currentReading = myScale.getReading();
  float currentWeight = myScale.getWeight();

  //Serial.print("\tReading: ");
  //Serial.print(currentReading);
  //Serial.print("\tWeight: ");
  //Serial.print(currentWeight, 2); //Print 2 decimal places

  avgWeights[avgWeightSpot++] = currentWeight;
  if(avgWeightSpot == AVG_SIZE) avgWeightSpot = 0;


  // Mitteln von AVG_SIZE Werten
  float avgWeight = 0;
  for (int x = 0 ; x < AVG_SIZE ; x++)
    avgWeight += avgWeights[x];
  avgWeight /= AVG_SIZE;


  //Serial.print("\tAvgWeight: ");
  //Serial.print(avgWeight, 2); //Print 2 decimal places
 // //if(settingsDetected == false)
 // //{
 // //  Serial.print("\tScale not calibrated. Press 'c'.");
 // //}
  //Serial.println();

  return avgWeight;
}

float gramm2rpm(float gramm)
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
    //while (1);
  }
  Serial.println("Scale detected!");


  Serial.print("Bevor Readsetting");
  Serial.print(var_gewicht_2kaffee);
  readSystemSettings(); //Load zeroOffset and calibrationFactor from EEPROM
  Serial.print("nach Settings");
  Serial.print(var_gewicht_2kaffee);

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
  Serial1.print("b3.txt=" + cmd + var_gewicht_1kaffee + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
  Serial1.print("b4.txt=" + cmd + var_gewicht_2kaffee + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
}

void page_set(void) {
  Serial1.print("page page1");
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
  Serial1.print("b3.txt=" + cmd + var_gewicht_1kaffee + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
  Serial1.print("b4.txt=" + cmd + var_gewicht_2kaffee + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
  Serial1.print("t6.txt=" + cmd + var_Siebtraeger_leer + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);

}

void page_abbruch(void) {
  Serial1.print("page page2");
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
  Serial1.print("b3.txt=" + cmd + var_gewicht_1kaffee + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
  Serial1.print("b4.txt=" + cmd + var_gewicht_2kaffee + cmd);
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

void page_calib(void){
  Serial1.print("page page4");
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
}

void screenwiegen(float aktuellgewicht, float zielgewicht)
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
  //Serial.print("Home");
  //Serial.print("\n");
  // Bildschim initialisieren
  Serial1.print("t0.txt=" + cmd + text + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
}

void screencalibgewicht(void)
{
  float tmp_gewicht = 0;
  tmp_gewicht = wiegen();
  Serial1.print("t2.txt=" + cmd + tmp_gewicht + cmd);
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

void screensettinggewicht(float zielgewicht)
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

  if(var_1oder2){
    Serial1.print("b3.txt=" + cmd + var_gewicht_1kaffee + cmd);
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);
  }
  else{
  Serial1.print("b4.txt=" + cmd + var_gewicht_2kaffee + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
  }
}

void screensettingrpm(float rpm)
{
  Serial.print("Setting RPM");
  Serial.print("\n");

  // Bildschim initialisieren
  Serial1.print("t4.txt=" + cmd + rpm + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
}

void screengewichtoderrpm(int gewichtoderrpm)
{
  if (gewichtoderrpm==0)
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
    Serial1.print("t7.bco=65525");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);
    Serial1.print("t8.bco=65525");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);  
  }
  if (gewichtoderrpm==1)
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
    Serial1.print("t7.bco=65525");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);
    Serial1.print("t8.bco=65525");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);  
  }
  if (gewichtoderrpm==2)
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
    Serial1.print("t2.bco=65525");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);
    Serial1.print("t4.bco=65525");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);
    Serial1.print("t7.bco=40179");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF);
    Serial1.print("t8.bco=40179");
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF); 
  }

}

void screensetting_automode(bool var_automode){
  if(var_automode){
    Serial1.print("t8.txt=" + cmd +"Autostart"+ cmd);
    Serial1.write(0xFF);
    Serial1.write(0xFF); 
    Serial1.write(0xFF); 
  }
  else{
    Serial1.print("t8.txt=" + cmd +"Manuel"+ cmd);
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

bool mahlen(float gewicht, float rpm, float zielgewicht)
{
  var_thoughput_number += 1;
  EEPROM.put(LOCATION_Throughput_number, var_thoughput_number);
  float volt = rpm/(3000/255); //Mahllogic
  

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
  var_cali= 0;
  var_plus= 0;
  var_minus= 0;
  var_1kaffee= 0;
  var_2kaffee= 0;
  var_statistik= 0;
}

void read_taster(void)
{ var_set_release=release(pin_set, var_set_alt);
  if(var_set_release) var_set_alt=false;
  var_stop_release=release(pin_stop_zuruck, var_stop_alt);
  if(var_stop_release) var_stop_alt=false;
  var_cali_release=release(pin_cali, var_cali_alt);
  if(var_cali_release) var_cali_alt=false;
  var_plus_release=release(pin_plus, var_plus_alt);
  if(var_plus_release) var_plus_alt=false;
  var_minus_release=release(pin_minus, var_minus_alt);
  if(var_minus_release) var_minus_alt=false;
  var_1kaffee_release=release(pin_1kaffe, var_1kaffee_alt);
  if(var_1kaffee_release) var_1kaffee_alt=false;
  var_2kaffee_release=release(pin_2kaffe, var_2kaffee_alt);
  if(var_2kaffee_release) var_2kaffee_alt=false;
  var_statistik_release=release(pin_statistik, var_statistik_alt);
  if(var_statistik_release) var_statistik_alt=false;

  reset();
  if(digitalRead(pin_stop_zuruck)==HIGH){
    var_stop=1;
    var_stop_alt=1;
    return;
  }
  if(digitalRead(pin_set)==HIGH){
    var_set=1;
    var_set_alt=1;
    return;
  }
  if(digitalRead(pin_cali)==HIGH){
    var_cali=1;
    var_cali_alt=1;
    return;
  }
  if(digitalRead(pin_plus)==HIGH){
    var_plus=1;
    var_plus_alt=1;
    return;
  }
  if(digitalRead(pin_minus)==HIGH){
    var_minus=1;
    var_minus_alt=1;
    return;
  }
  if(digitalRead(pin_1kaffe)==HIGH){
    var_1kaffee=1;
    var_1kaffee_alt=1;
    return;
  }
  if(digitalRead(pin_2kaffe)==HIGH){
    var_2kaffee=1;
    var_2kaffee_alt=1;
    return;
  }
  if(digitalRead(pin_statistik)==HIGH){
    var_statistik=1;
    var_statistik_alt=1;
    return;
  }
}

void test_taster(void){
  if(var_set==1)screenWritingtext("set");
  if(var_stop==1)screenWritingtext("stop");
  if(var_statistik==1)screenWritingtext("statistik");
  if(var_1kaffee==1)screenWritingtext("kaffe_1");
  if(var_2kaffee==1)screenWritingtext("kaffe_2");
  if(var_cali==1)screenWritingtext("calib.");
  if(var_minus==1)screenWritingtext("minus");
  if(var_plus==1)screenWritingtext("plus");
}

bool release(int pin, bool alter_zustand){
  if(digitalRead(pin)==LOW && alter_zustand){
    //alter_zustand=false;
    return true;
  }
  else{
    return false;
  }
}

void zurueck(void)
{
  if(press_stop_again&&var_stop_release){
  Zustand = z_home;
  press_stop_again=false;

  }
}