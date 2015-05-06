#include "BeaconController.h"

/* Example running 10 beacons simultaneously.
   If there is a problem with translating the texts to morse code,
   an error message will be printed on the serial port and the transmission will not start
*/

  


#define BEACON_COUNT 10
#define BEACON_MESSAGE_LENGTH 44


Beacon beacons[BEACON_COUNT];


const byte beaconPins[BEACON_COUNT][4] = 
{
  {9,8,7,6},
  {5,4,3,2},
  {22,23,24,25},
  {26,27,28,29},
  {30,31,32,33},
  {34,35,36,37},
  {38,39,40,41},
  {42,43,44,45},
  {46,47,48,49},
  {50,51,52,53}
};

const char* beaconTexts[BEACON_COUNT] =
{
  "This is a test $-1",  // Beacon 0: Pin 9
  "Dit is een test $-1",  // Beacon 1: Pin 5
  "Ceci est un test $-1",  // Beacon 2: Pin 22 
  "test $-2$+2$-2 $-1",  // Beacon 3: Pin 26
  "cq cq cq de on8vq k $-2",  // Beacon 4: Pin 30
  "$P0 mode 0 $-5 $P1 mode 1 $-5 $P2 mode 2 $-5 $P3 mode 3 $-5", // Beacon 5: Pin 34
  "6  6  6 $-1",   // Beacon 6: Pin 38
  "7  7  7 $-1",   // Beacon 7: Pin 42
  "8  8  8 $-1",    // Beacon 8: Pin 46
  "9  9  9 $-1"    // Beacon 9: Pin 50
};

byte beaconMessages[BEACON_COUNT][BEACON_MESSAGE_LENGTH];

void setup()
{
  Serial.begin(9600);
  for(int i=0; i<BEACON_COUNT; i++)
  {
    beacons[i].begin(beaconPins[i][0], beaconPins[i][1], beaconPins[i][2], beaconPins[i][3]);
    if(morseEncodeMessage(beaconMessages[i], beaconTexts[i], BEACON_MESSAGE_LENGTH)==false)
    {
      Serial.print("Error parsing text for beacon nr ");
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
  
  // Setup TIMER 1 hardware directly
  cli();//stop interrupts

  //set timer1 interrupt at 10Hz
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for 1hz increments
  OCR1A = 1562/2;// = (16*10^6) / (10*1024) - 1 (must be <65536)
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
  for(int i=0; i<10; i++)
  {
    beacons[i].tick();
  }
}

void loop()
{
  for(int i=0; i<10; i++)
  {
    if(beacons[i].isDone())
    {
      cli();
      beacons[i].setNextMessage(beaconMessages[i]);
      sei();
    }
  }
  delay(50);
}
