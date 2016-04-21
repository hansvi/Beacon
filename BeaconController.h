#ifndef BEACONCONTROLLER_H_
#define BEACONCONTROLLER_H_

#include <Arduino.h>

byte morseEncodeChar(char c);
boolean morseEncodeMessage(byte *codedMessage, const char *str, int maxBytes);
const char* morseGetError();

class Beacon
{
  int normalPin;
  int invPin;
  int modePin0;
  int modePin1;
  
  byte *nextMsg;

  byte *msg;
  int msgPos;
  byte bitPos;  // 0-7
  byte code;    // contains a copy of the current byte being sent, current bit is MSB
  
  // State while transmitting is determined by these 3 variables:
  // cmdPause for when a pause command byte is being processed
  // onPause when a dit or a dah is being transmitted
  // offPause when a intra- or inter-symbol delay is being transmitted
  unsigned cmdPause;
  byte onPause;
  byte offPause;
  
  boolean done;
  boolean enabled;
  
  void keyOn();                // Sets the normal output
  void keyOff();               // Sets the inverted output
  void carrierOff();           // Sets both outputs to off
  void powerMode(byte mode);   // Sets the 2 mode pins to a specific state

  public:
  Beacon(int normalPin, int invPin, int modePin0, int modePin1);
  Beacon();
  void begin(int normalPin, int invPin, int modePin0, int modePin1);
  void end();
  void tick();
  boolean isDone();
  void setNextMessage(byte *codedMessage);
  void setEnabled(bool on); // Start/stop the beacon.
  bool getEnabled();
};

#endif
