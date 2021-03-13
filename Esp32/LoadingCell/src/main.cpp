/*
Kaffeemühle Four-Eng
Ersteller: Christain Heinzmann, Maximilian Mantel
Zustandasautomat der Kaffeemühle welcher die Bedienung, Ansteuerung der Wage, Motor und Display abhandelt.

In dieser Version wurde versucht die Hardware Taster welche über die I/O ports des Arduino ausgelesen werden
durch die Schaltflächen des Nextion- TouchDisplays zu ersetzen.

Hierfür wurden folgende Funktionen angepasst:
-void test_taster(void);
-bool release(int pin, bool alter_zustand);
und zusätzlich wurde die Funktion "void touchinput(void)" eingeführt.

*/

#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h> //Needed to record user settings
#include "SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h" // Click here to get the library: http://librarymanager/All#SparkFun_NAU8702
#include <string.h>
#include <time.h>

NAU7802 myScale; //Create instance of the NAU7802 class

//EEPROM locations to store 4-byte variables
#define LOCATION_CALIBRATION_FACTOR 0 //Float, requires 4 bytes of EEPROM
#define LOCATION_ZERO_OFFSET 10 //Must be more than 4 away from previous spot. Long, requires 4 bytes of EEPROM
#define LOCATION_Siebtraeger 20
#define LOCATION_gewicht_1 30
#define LOCATION_rpm_1 40
#define LOCATION_gewicht_2 50
#define LOCATION_rpm_2 60

bool settingsDetected = false; //Used to prompt user to calibrate their scale

//Create an array to take average of weights. This helps smooth out jitter.
#define AVG_SIZE 20
float avgWeights[AVG_SIZE];
byte avgWeightSpot = 0;

//------------------------------------------------------------------------------
//------------------Definition für bessere lesbarkeit des Codes-----------------
//------------------------------------------------------------------------------

//Definiton der Tasten-Pinbelegung um Pins mit Namen statt Zahlen anzusprechen
//Wird für die Hardware-Taster verwendet
#define pin_PWM 2
#define pin_set 13
#define pin_statistik 12
#define pin_stop_zuruck 11
#define pin_1kaffe 10
#define pin_2kaffe 9
#define pin_cali 8
#define pin_minus 7
#define pin_plus 6

//Definition der Zustände für den Zustandsautomaten der Kaffeemühle
#define z_home 1
#define z_wiegen 2
#define z_fertig 3
#define z_settings 4 
#define z_abbruch 5
#define z_statistik 6
#define z_calib 7

//-------------------------------------------------------
//------------------Variablen-----------------
//-------------------------------------------------------

//----------------------Variablen für die Eingabe------------------------------
//Variablen zur verarbeitung der Touch-Screen-Schaltflächen des Nextion-Displays
int incomingByte [7];     //Buffer für die vom Display gesendete Nachricht über Serielle Schnittstelle 
int bytecunter=0;         //Index für Buffer
bool softpin [7];         //Zustands-Variablen welche den Zustand(gedrückt/nichtgedrückt) der Touch-Screen-Schaltflächen repräsentieren.
                          //Ersätzen Später die Digital.read(pin_...) abfragen der Hardware Taster.
//Zustandsvariable der Taster/Touchflächen 
bool var_set = 0;         
bool var_stop = 0;
bool var_cali = 0;
bool var_plus = 0;
bool var_minus = 0;
bool var_1kaffee = 0;
bool var_2kaffee = 0;
bool var_statistik = 0;
void reset(void); //Rücksetzen der Zustände am ende eines Schleifendurchlaufs
//Hilfs variablen mit zustand des vorhärigen Schleifendurchlaufs zur erkennung der fallenden Flanke
bool var_set_alt = 0;
bool var_stop_alt = 0;
bool var_cali_alt = 0;
bool var_plus_alt = 0;
bool var_minus_alt = 0;
bool var_1kaffee_alt = 0;
bool var_2kaffee_alt = 0;
bool var_statistik_alt = 0;
//Variable zur darstellung der fallenden Flanken
bool var_set_release = 0;
bool var_stop_release = 0;
bool var_cali_release = 0;
bool var_plus_release = 0;
bool var_minus_release = 0;
bool var_1kaffee_release = 0;
bool var_2kaffee_release = 0;
bool var_statistik_release = 0;
//-------------------Variablen für den Zustandsautomaten----------------
//Settings
int var_gewichtoderrpm = 0; // Curserauswahl welcher Wert im Settingmenue ausgewählt ist. 1 Gewicht 0 rpm  2 Automode
bool var_1oder2 = 1;        // Auswahl welche Werte im Setting angezeit/eingestellt werden
                            // bzw. welchee Einstellung gemahlen werden soll. 1 1Kaffee 0 2Kaffee
int var_gewicht_1kaffee = 30; //Gewicht für Taste 1Kaffee
int var_rpm_1kaffee = 1000;   //Drehzahl voreistellung für Taste 1Kaffee
int var_gewicht_2kaffee = 35; //Gewicht für Taste 2Kaffee
int var_rpm_2kaffee = 2000;   //Drehzahl voreistellung für Taste 1Kaffee
bool var_automode=false;      // True=>automatischer start bei Erkennung des leeren Siebträgers
float Siebtraeger_leer=0;     //Gewicht des leeren Siebträgers
//Zustandsautomat und Routinen
int Zustand = 1;              //Zustand des Zustandsautomat
int Zustand_alt = 0;          //Zustand des vorherigen Schleifendurchlaufs => ärkennung von änderungen
int Calib_schritt=0;          //Zustand innerhal der Calibrier-Routine
bool finish = 0;              //Flag symbolisiert Mahlmenge erreicht
bool press_stop_again=false;  //Flag für erneutes Stop drücken bei der Abbruch-Routine
float gewicht_Delta_ok=0.5;   //Aktzeptable Gewichtsabweichung für Fertig, Startklar...
float gewicht;                //gemessenes Gewicht
float rpm;                    //Variable für die PWM zur ansteuerung des Motors
String cmd = "\"";            //Hilfsvariable zum verhindern von Syntaxfehlern beim Senden von Text über Serial.print an das Display 

//-------------------------------------------------------
//------------------Funktionsdekleration-----------------
//-------------------------------------------------------

//Taster input (Diese Funktionen wurden angepasste Digital.read... wurde durch das softpin[] Array ersetzt um Touchscreen zu verwenden)
void read_taster(void);       //Erkennung der gedrückent Tasten
void test_taster(void);       //Hilfsfunktion für Testzwecke: gibt gedrückte Tasten am Serial Monitor des über USB angeschlossen PC aus.
bool release(int pin, bool alter_zustand);  //Erkennung der Fallenden Flanke der Tasten
//TuchDisplayinput
void touchinput(void);      //Empfängt die Info vom Bildschrim über Serial.read ob Schaltfläche gedrückt oder losgelassen wurde.
                            //und setzt die entsprechende Stelle im softpin[] Array auf True/False (Ersatz zum zustand des der Pins mittels Digitalread (High/Low))
//EEPROM
void recordSystemSettings(void); //Speichern aller Setings im EEPROM damit werte auch im Spannungslosen zustand nicht verlohren gehen.
void readSystemSettings(void);   //Lesen der Gespeicherten Werte beim Einschalten
//Funktionen
float wiegen(void);
bool malen(int gewicht, int rpm, int zielgewicht);

//-----------------Display--------------------------------
//Pages             //Umschalten der Pages des Nextion-Displays über Serialle-Schnittstelle
void page_statistik(void);
void page_abbruch(void);
void page_set(void);
void page_main(void);
void page_calib(void);
//Screen          //Funktionen zum füllen und aktualisieren der Test Felder des Nextion-Displays über Serialle-Schnittstelle
void ScreenInit(void);
void ScaleInit(void);
void screenWritingtext(String text);
void screenwiegen(float aktuellgewicht, int zielgewicht);
void screensettinggewicht(int zielgewicht);
void screengewichtoderrpm(int gewichtoderrpm);
void screensettingrpm(int rpm);
void screensetting_automode(bool var_automode);
void screen1kaffeeoder2kaffee(bool kaffee1oder2);
void screenfertig(void);
void screencalibtext(void);
void screensettingrefresh1(void);
void screensettingrefresh2(void);

//Setup wird beim Neustart des Arduinos einmal ausgeführt.
void setup()
{
  Serial.begin(9600);   //Serialle-Schnittstelle zum Computer USB Hautsächlich für Test und Debug zwecke
  Serial1.begin(9600);  //Serielle-Schnittstelle zum Nextion-Display /Umschalten der Pages, füllen der Textfelder und empfangen der Toucheingabe

  ScaleInit();          //Initialisieren des Wägebausteins.
  ScreenInit();         //Intialiesieren Des Displays

  pinMode(pin_PWM, OUTPUT); //Pin 2 des Arduino als Ausgansetzen für die ansteuerung des Motors.
  //Pin 13-6 des Arduino als Eingang setzen für die Hardware Taster
  pinMode(pin_set, INPUT);
  pinMode(pin_statistik, INPUT);
  pinMode(pin_stop_zuruck, INPUT);
  pinMode(pin_1kaffe, INPUT);
  pinMode(pin_2kaffe, INPUT);
  pinMode(pin_cali, INPUT);
  pinMode(pin_minus, INPUT);
  pinMode(pin_plus, INPUT);

  readSystemSettings(); //laden der Variablen aus dem EEPROM
}

void loop()
{ 
  touchinput();
  read_taster();
  test_taster();

  switch (Zustand)
  {
  case z_home:
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
    gewicht = wiegen();
    if((var_1kaffee && !var_automode) || (var_2kaffee && !var_automode) || (var_automode && (Siebtraeger_leer-gewicht_Delta_ok <= gewicht && gewicht <= Siebtraeger_leer+gewicht_Delta_ok)))
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
      var_gewichtoderrpm = 0;
      page_set();
      screen1kaffeeoder2kaffee(var_1oder2);
      if (var_1oder2 == 1){
        screensettingrefresh1();
      }
      else{
        screensettingrefresh2();
      }
    }
    

    if (var_stop){
      EEPROM.put(LOCATION_gewicht_1, var_gewicht_1kaffee);
      EEPROM.put(LOCATION_gewicht_2, var_gewicht_2kaffee);
      EEPROM.put(LOCATION_rpm_1, var_rpm_1kaffee);
      EEPROM.put(LOCATION_rpm_2, var_rpm_2kaffee);
      Zustand = z_home;
    }
    if (var_set_release) var_gewichtoderrpm = (var_gewichtoderrpm+1)%3;
    if (var_1kaffee){
      var_1oder2 = true;
      screen1kaffeeoder2kaffee(var_1oder2);
    }
    if (var_2kaffee){
      var_1oder2 = false;
      screen1kaffeeoder2kaffee(var_1oder2);
    } 
    if (var_1oder2 == 1) // kaffee 1
    {
      if(var_gewichtoderrpm == 0) // Gewicht
      {
        if (var_plus){
          var_gewicht_1kaffee +=1;
          screensettingrefresh1();
        }
        if (var_minus){
          var_gewicht_1kaffee -=1;
          screensettingrefresh1();
        }
      }
      if(var_gewichtoderrpm == 1) // rpm
      { 
        if (var_plus){
          var_rpm_1kaffee +=1;
          screensettingrefresh1();
        }
        if (var_minus){
          var_rpm_1kaffee -=1;
          screensettingrefresh1();
        }
      }
      if(var_gewichtoderrpm == 2) // Auto/Manuel mode
      {
        if (var_plus_release || var_minus_release){
          var_automode=!var_automode;
          screensettingrefresh1();
        }
      }
    }
    else
    {
      if(var_gewichtoderrpm == 0) // Gewicht
      {
        if (var_plus){
          var_gewicht_2kaffee +=1;
          screensettingrefresh2();
        }
        if (var_minus){
          var_gewicht_2kaffee -=1;
          screensettingrefresh2();
        }
      }
      if(var_gewichtoderrpm == 1) // rpm
      { 
        if (var_plus){
          var_rpm_2kaffee +=1;
          screensettingrefresh2();
        }
        if (var_minus){
          var_rpm_2kaffee -=1;
          screensettingrefresh2();
        }
      }
      if(var_gewichtoderrpm == 2) // Auto/Manuel mode
      {
        if (var_plus_release || var_minus_release){
          var_automode=!var_automode;
          screensettingrefresh2();
        }
      }
    }
    break;
  case z_abbruch: // Abbruch
    if(Zustand_alt != Zustand)
    {
      Zustand_alt = Zustand;
      page_abbruch();
      //delay(3000);
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
          //"Gewicht entnehmen und siebträger einsetzen." Anzeigen!!!
          screencalibtext();
          Serial.println("Gewicht entnehmen und siebträger einsetzen.");
          //bei Calib taste drücken und loslassen
          if(var_cali_release){
            //Gewicht des leeren Siebträgers Speichern!!!
            Siebtraeger_leer=wiegen();
            Calib_schritt=3;
          }
          break;
        case 1:
          //"100 gramm einsetzen." Anzeigen
          screencalibtext();
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
          screencalibtext();
          Serial.println("Aufnahme leeren.");
          //bei Calib taste drücken und loslassen 
          if(var_cali_release){
            //Nullpunkt wage Ermitteln!!!
            myScale.calculateZeroOffset(64); //Zero or Tare the scale. Average over 64 readings.
            Calib_schritt=1;
          }
          break;
        case 3:
          screencalibtext();
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
  EEPROM.put(LOCATION_Siebtraeger, Siebtraeger_leer);
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
  EEPROM.get(LOCATION_Siebtraeger, Siebtraeger_leer);
  if (Siebtraeger_leer == 0xFFFFFFFF)
  {
    Siebtraeger_leer = 500L; //Default to 1000 so we don't get inf
    EEPROM.put(LOCATION_Siebtraeger, Siebtraeger_leer);
  }

  //var_gewicht_1kaffee
  EEPROM.get(LOCATION_gewicht_1, var_gewicht_1kaffee);
  if (var_gewicht_1kaffee == 0xFFFFFFFF)
  {
    var_gewicht_1kaffee = 30L; //Default to 1000 so we don't get inf
    EEPROM.put(LOCATION_gewicht_1, var_gewicht_1kaffee);
  }
  //var_gewicht_2kaffee
  EEPROM.get(LOCATION_gewicht_2, var_gewicht_2kaffee);
  if (var_gewicht_2kaffee == 0xFFFFFFFF)
  {
    var_gewicht_2kaffee = 35L; //Default to 1000 so we don't get inf
    EEPROM.put(LOCATION_gewicht_2, var_gewicht_2kaffee);
  }
  //var_rpm_1kaffee
  EEPROM.get(LOCATION_rpm_1, var_rpm_1kaffee);
  if (var_rpm_1kaffee == 0xFFFFFFFF)
  {
    var_rpm_1kaffee = 1000L; //Default to 1000 so we don't get inf
    EEPROM.put(LOCATION_rpm_1, var_rpm_1kaffee);
  }
  //var_rpm_2kaffee
  EEPROM.get(LOCATION_rpm_2, var_rpm_2kaffee);
  if (var_rpm_2kaffee == 0xFFFFFFFF)
  {
    var_rpm_2kaffee = 2000L; //Default to 1000 so we don't get inf
    EEPROM.put(LOCATION_rpm_2, var_rpm_2kaffee);
  }
  
}


float wiegen(void)
{
  long currentReading = myScale.getReading();
  float currentWeight = myScale.getWeight();
  avgWeights[avgWeightSpot++] = currentWeight;
  if(avgWeightSpot == AVG_SIZE) avgWeightSpot = 0;
  // Mitteln von AVG_SIZE Werten
  float avgWeight = 0;
  for (int x = 0 ; x < AVG_SIZE ; x++)
    avgWeight += avgWeights[x];
  avgWeight /= AVG_SIZE;
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
}

void ScaleInit(void)
{
  Wire.begin();
  Wire.setClock(400000); //Qwiic Scale is capable of running at 400kHz if desired

  if (myScale.begin() == false)
  {
    Serial.println("Scale not detected. Please check wiring. Freezing...");
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
  Serial1.print("t6.txt=" + cmd + Siebtraeger_leer + cmd);
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

void screenwiegen(float aktuellgewicht, int zielgewicht)
{
  Serial1.print("t0.txt=" + cmd + aktuellgewicht + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
}

void screenWritingtext(String text)
{
  Serial1.print("t0.txt=" + cmd + text + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
}


void screenfertig(void)
{
  Serial1.print("t0.txt=" + cmd + "Fertig!" + cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF); 
  Serial1.write(0xFF);
}

void screensettinggewicht(int zielgewicht)
{
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

void screensettingrpm(int rpm)
{
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

void screencalibtext(void){
  switch(Calib_schritt){
    case 0:
      screenWritingtext("Aufnahme leeren. Mit Calib. Taste Vortfahren");
      break;
    case 1:
      screenWritingtext("100 gramm einsetzen. Mit Calib. Taste Vortfahren");
      break;
    case 2:
      screenWritingtext("Gewicht entnehmen und Siebträger einsetzen. Mit Calib. Taste Vortfahren");
      break;
    case 3:
      screenWritingtext("Fertig!");
      break;
  }
}

void screensettingrefresh1(void){
  screensettinggewicht(var_gewicht_1kaffee);
  screensettingrpm(var_rpm_1kaffee);
  screengewichtoderrpm(var_gewichtoderrpm);
  screensetting_automode(var_automode);
}

void screensettingrefresh2(void){
  screensettinggewicht(var_gewicht_2kaffee);
  screensettingrpm(var_rpm_2kaffee);
  screengewichtoderrpm(var_gewichtoderrpm);
  screensetting_automode(var_automode);
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
  if(softpin[2]==true){     //"Digital.read(pin...)==High" wurde jeweils durch "softpin[..]==True" ersetz,
    var_stop=1;             //So wurden die Hardwaretasten durch die Schaltflächen des Nextion-Displays ersätzt.
    var_stop_alt=1;
    return;
  }
  if(softpin[0]==true){
    var_set=1;
    var_set_alt=1;
    return;
  }
  if(softpin[5]==true){
    var_cali=1;
    var_cali_alt=1;
    return;
  }
  if(softpin[7]==true){
    var_plus=1;
    var_plus_alt=1;
    return;
  }
  if(softpin[6]==true){
    var_minus=1;
    var_minus_alt=1;
    return;
  }
  if(softpin[3]==true){
    var_1kaffee=1;
    var_1kaffee_alt=1;
    return;
  }
  if(softpin[4]==true){
    var_2kaffee=1;
    var_2kaffee_alt=1;
    return;
  }
  if(softpin[1]==true){
    var_statistik=1;
    var_statistik_alt=1;
    return;
  }
}

void test_taster(void){
  if(var_set==1)Serial.print("set");
  if(var_stop==1)Serial.print("stop");
  if(var_statistik==1)Serial.print("statistik");
  if(var_1kaffee==1)Serial.print("kaffe_1");
  if(var_2kaffee==1)Serial.print("kaffe_2");
  if(var_cali==1)Serial.print("calib.");
  if(var_minus==1)Serial.print("minus");
  if(var_plus==1)Serial.print("plus");
}

bool release(int pin, bool alter_zustand){
  if(softpin[-1*(pin-13)]==false && alter_zustand){   //"Digital.read(pin)==High" wurde jeweils durch "softpin[-1*(pin-13)]==false" ersetz,
    //alter_zustand=false;
    return true;
  }
  else{
    return false;
  }
}

void touchinput(void){                          //Empfängt Daten des Displays über Serialle Schnitstelle und Setzt die Softpin Variablen welche die Pysikalischen
  if (Serial1.available() > 0) {                //zustände der Pins des Arduino ersetzen.
    incomingByte[bytecunter] = Serial1.read();  //Buffer wird mit Bytes vom Display gefühlt.
    bytecunter++;
  }
  if(bytecunter==7){                            //Sieben Bytes empfangen (länge einer Nachricht)
    if(incomingByte[4]==255 && incomingByte[5]==255 && incomingByte[5]==255){ //Gültigkeit testen Ende muss aus drei Bytes mit 0xFF bestehen.
      switch (incomingByte[2])                  //Drittes Byte gibt ID der Jeweiligen Schaltfläche an.
      {
        case 1:
          if(incomingByte[3]==1){               //Viertes Byte: 1=steigende Flanke 0=fallende Flanke
            softpin[0]=true;
            Serial.print(incomingByte[2]);
          }
          if(incomingByte[3]==0){
            softpin[0]=false;
          }
          break;
        case 2:
          if(incomingByte[3]==1){
            softpin[1]=true;
            Serial.print(incomingByte[2]);
          }
          if(incomingByte[3]==0){
            softpin[1]=false;
          }
          break;
        case 3:
          if(incomingByte[3]==1){
            softpin[2]=true;
            Serial.print(incomingByte[2]);
          }
          if(incomingByte[3]==0){
            softpin[2]=false;
          }
          break;
        case 4:
          if(incomingByte[3]==1){
            softpin[3]=true;
            Serial.print(incomingByte[2]);
          }
          if(incomingByte[3]==0){
            softpin[3]=false;
          }
          break;
        case 5:
          if(incomingByte[3]==1){
            Serial.print(incomingByte[2]);
            softpin[4]=true;
          }
          if(incomingByte[3]==0){
            softpin[4]=false;
          }
          break;
        case 6:
          if(incomingByte[3]==1){
            Serial.print(incomingByte[2]);
            softpin[5]=true;
          }
          if(incomingByte[3]==0){
            softpin[5]=false;
          }
          break;
        case 7:
          if(incomingByte[3]==1){
            Serial.print(incomingByte[2]);
            softpin[6]=true;
          }
          if(incomingByte[3]==0){
            softpin[6]=false;
          }
          break;
        case 8:
          if(incomingByte[3]==1){
            Serial.print(incomingByte[2]);
            softpin[7]=true;
          }
          if(incomingByte[3]==0){
            softpin[7]=false;
          }
          break;
    
        default:
          Serial.print("Erro touch");
          break;
      }
    }
    else{
      Serial.print("Erro touch keine drei ff");
      for (int i=0; i <7; i++){
        Serial.print(incomingByte[i]);
      }
      bytecunter=0;
      while (Serial1.available() > 0) {
        incomingByte[bytecunter] = Serial1.read();
        delay(10);
      }
    }
    bytecunter=0;
  }
}