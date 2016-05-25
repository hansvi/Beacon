// PSTR : https://lowpowerlab.com/forum/index.php?topic=1199.5;wap2
// DS1820: gnd:1 2:data 3:vdd (flat on top, pins towards you -> 1 is left) 4k7 between data and vdd

// TODO:
// * GPS
// * Logging

#include "BeaconController.h"
#include "Sensors.h"
#include "Config.h"
#include "ControlPanel.h"
#include "WebServer.h"

// These need to be included for the libraries to be compiled in - Arduino specific
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <SD.h>
#include <TimeLib.h>
#include <Ethernet.h>

#define TIME_HEADER "T"

/* Example running 9 beacons simultaneously.
   If there is a problem with translating the texts to morse code,
   an error message will be printed on the serial port and the transmission will not start
*/

// Pin 4 is SD CS
// Pin 10 is Eth CS
// PIN 11 is SPI (Eth, SD)
// PIN 12 is SPI (Eth, SD)
// Pin 13 is SPI (Eth, SD)
// Pin 2 is OneWire DS1820 Temperature sensors

Beacon beacons[BEACON_COUNT];

// IO pins used by the beacons (normal/inverted/PM0/PM1)
const byte beaconPins[BEACON_COUNT][4] = 
{
  {9,8,7,6},
  {22,23,24,25},
  {26,27,28,29},
  {30,31,32,33},
  {34,35,36,37},
  {38,39,40,41},
  {42,43,44,45},
  {46,47,48,49},
  {50,51,52,52}
};

byte beaconMessages[BEACON_COUNT][BEACON_MESSAGE_LENGTH];
char textBuffer[BEACON_MESSAGE_LENGTH];

time_t last_log=0;

// TODO: Replace with GPS sychronisation
void processSyncMessage()
{
  unsigned long pctime;
  const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013

  if(Serial.find(TIME_HEADER))
  {
    pctime = Serial.parseInt();
    if( pctime >= DEFAULT_TIME)  // check the integer is a valid time (greater than Jan 1 2013)
    {
      setTime(pctime); // Sync Arduino clock to the time received on the serial port
    }
  }
}

void setup()
{
  Serial.begin(9600);
  WebServerInit();
  controlPanelInit();
  sensorsInit();
  for(int i=0; i<BEACON_COUNT; i++)
  {
    beacons[i].begin(beaconPins[i][0], beaconPins[i][1], beaconPins[i][2], beaconPins[i][3]);
    textBuffer[0] = 0;
    getBeaconMessage(i, BEACON_DEFMSG, textBuffer, BEACON_MESSAGE_LENGTH);
    if(morseEncodeMessage(beaconMessages[i], textBuffer, BEACON_MESSAGE_LENGTH)==false)
    {
      Serial.print(F("Error parsing text for beacon nr "));
      Serial.print(i);
      Serial.println(":");
      Serial.println(morseGetError());
      while(true)
      {
        // General strike! We demand better code!
      }
    }
    beacons[i].setNextMessage(beaconMessages[i]);
  }
  
  // Wait until the time is synchronized
  Serial.println("Enter the time (format: T<number> where <number> is the unix time)");
  while(!Serial.available()) {}
  processSyncMessage();
  last_log=now();
  Serial.print("Date: ");
  Serial.print(year());
  Serial.print("-");
  Serial.print(month());
  Serial.print("-");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(hour());
  Serial.print(":");
  Serial.println(minute());
  
  // TODO: For debugging only!!
    SD.remove("/log/2016/05/25.CSV");
  
  // Setup TIMER 1 hardware directly
  cli();//stop interrupts

  //set timer1 interrupt at 10Hz
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for 1hz increments
  OCR1A = 1562;// = (16*10^6) / (10*1024) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS12 and CS10 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);

  sei();//allow interrupts
}

// Interrupt routine for TIMER 1
// Run the beacons
ISR(TIMER1_COMPA_vect)
{
  for(int i=0; i<BEACON_COUNT; i++)
  {
    beacons[i].tick();
  }
}

void loop()
{
  for(int i=0; i<BEACON_COUNT; i++)
  {
    if(beacons[i].isDone())
    {
      getCurrentMessage(i, textBuffer, BEACON_MESSAGE_LENGTH);
      if(morseEncodeMessage(beaconMessages[i], textBuffer, BEACON_MESSAGE_LENGTH)==false)
      {
        Serial.print(F("Error parsing text for beacon nr "));
        Serial.print(i);
        Serial.println(":");
        Serial.println(morseGetError());
        while(true)
        {
          // General strike! We demand better code!
        }
      }
      cli();
      beacons[i].setNextMessage(beaconMessages[i]);
      sei();
    }
  }
  sensorsTick();
  WebServerTick();
  if(Serial.available())
  {
    processSyncMessage();
  }
  time_t t=now();
  if((t-last_log) > LOG_INTERVAL)
  {
    writeLog(t);
    last_log += LOG_INTERVAL; // prevent drifting of the logging times
  }
  delay(20);
}
