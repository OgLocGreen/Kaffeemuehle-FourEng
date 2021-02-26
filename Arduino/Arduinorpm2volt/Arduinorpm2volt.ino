
#include <SoftwareSerial.h>
#include <stdlib.h>
#include <stdio.h>

//SoftwareSerial Serial2(0, 1); // RX, TX
#define RX2 0
#define TX2 1
SoftwareSerial Serial2(RX2, TX2);
const byte numChars = 32;
char receivedChars[numChars];   // an array to store the received data
boolean newData = false;

using namespace std; 
static unsigned long rpmMax = (3000/255);
int analogPin = 9;
int rpm;
int volt;


int rpm2volt(unsigned long input_rpm);
void recvWithEndMarker(void);
void showNewData();


void setup() {
  pinMode(analogPin, OUTPUT);
  Serial.begin(9600);
  Serial2.begin(9600);
  Serial.println("<Arduino is ready>");
}

void loop() {
  recvWithEndMarker();
  showNewData();
  rpm = strtoul( receivedChars, NULL, 10 );
  volt = rpm2volt(rpm);
  analogWrite(analogPin,volt);
  Serial.print("Volt: ");
  Serial.print(volt);
  Serial.println();
}


int rpm2volt(unsigned long input_rpm){
  return (int)(input_rpm/rpmMax);
}

void recvWithEndMarker() {
  static byte ndx = 0;
  char endMarker = '\n';
  char rc;
 
  while (Serial2.available() > 0 && newData == false) {
      rc  = Serial2.read();   

      if (rc  != endMarker) {
          receivedChars[ndx] = rc;
          ndx++;
          if (ndx >= numChars) {
              ndx = numChars - 1;
          }
      }
      else {
          receivedChars[ndx] = '\0'; // terminate the string
          ndx = 0;
          newData = true;
        }
    }
}

void showNewData() {
    if (newData == true) {
        Serial.print("Rpm: ");
        Serial.print(receivedChars);
        Serial.println();
        newData = false;
    }
}
