#include "ControlPanel.h"
#include "Config.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <TimeLib.h>

static bool runningState[BEACON_COUNT]; // true -> on, false -> off
static byte lastSpecialMessage[BEACON_COUNT]; // 0=H00, 1=H15, 2=H30, 3=H45

void controlPanelInit()
{
  File f;
  char filename[20];
  byte firstMessage;
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);

  if (!SD.begin(4)) {
    Serial.println("SDCard initialization failed!");
    // assert(0);
    while(1) {} // No point continuing after just having stated that
  }
  Serial.println("SCDCard initialization done.");

  firstMessage = minute()/15;
  for(int i=0; i<BEACON_COUNT; i++)
  {
    lastSpecialMessage[i]=firstMessage;
    sprintf(filename, "%d", i);
    if(!SD.exists(filename))
    {
      SD.mkdir(filename);
      sprintf(filename, "%d/ON", i);
      f = SD.open(filename, FILE_WRITE);
      f.close();
    }
    sprintf(filename, "%d/ON", i);
    if(SD.exists(filename))
    {
      runningState[i] = true;
    }
    else
    {
      runningState[i] = false;
    }
  }
}

bool isBeaconRunning(int index)
{
  if((index>=0) && (index<BEACON_COUNT))
  {
    return runningState[index];
  }
  return false;
}

void setBeaconRunning(int index, bool state)
{
  File f;
  char filename[20];
  
  if((index>=0) && (index<BEACON_COUNT))
  {
    sprintf(filename, "%d/ON", index);
    runningState[index] = state;
    if(state)
    {
      f = SD.open(filename, FILE_WRITE);
      f.close();
    }
    else
    {
      SD.remove(filename);
    }
  }
}

static bool getMessageHelper(int index, char *dest, int bufsz, const char* file)
{
  char filename[20];
  File f;
  if((index>=0) && (index<BEACON_COUNT) && (bufsz > 0))
  {
    sprintf(filename, "%d/%s", index, file);
    f = SD.open(filename, FILE_READ);
    if(f)
    {
      char c = f.read();
      while((c != -1) && (c != '\r') && (c != '\n') && (bufsz > 1))
      {
        *dest++=c;
        bufsz--;
        c=f.read();
      }
      *dest = 0;
      f.close();
      return true;
    }
  }
  return false;
}

static bool setMessageHelper(int index, char *msg, const char* file)
{
  char filename[20];
  File f;
  if((index>=0) && (index<BEACON_COUNT))
  {
    sprintf(filename, "%d/%s", index, file);
    SD.remove(filename);
    f = SD.open(filename, FILE_WRITE);
    if(f)
    {
      f.println(msg);
      f.close();
    }
  }
}

bool getDefaultMessage(int index, char *dest, int bufsz)
{
  return getMessageHelper(index, dest, bufsz, "DEFAULT.TXT");
}
bool getMessage00(int index, char *dest, int bufsz)
{
  return getMessageHelper(index, dest, bufsz, "H00.TXT");
}
bool getMessage15(int index, char *dest, int bufsz)
{
  return getMessageHelper(index, dest, bufsz, "H15.TXT");
}
bool getMessage30(int index, char *dest, int bufsz)
{
  return getMessageHelper(index, dest, bufsz, "H30.TXT");
}
bool getMessage45(int index, char *dest, int bufsz)
{
  return getMessageHelper(index, dest, bufsz, "H45.TXT");
}

void setDefaultMessage(int index, char *msg)
{
  setMessageHelper(index, msg, "DEFAULT.TXT");
}
void setMessage00(int index, char *msg)
{
  setMessageHelper(index, msg, "H00.TXT");
}
void setMessage15(int index, char *msg)
{
  setMessageHelper(index, msg, "H15.TXT");
}
void setMessage30(int index, char *msg)
{
  setMessageHelper(index, msg, "H30.TXT");
}
void setMessage45(int index, char *msg)
{
  setMessageHelper(index, msg, "H45.TXT");
}

bool getCurrentMessage(int index, char *dest, int bufsz)
{
  if((index>=0) && (index<BEACON_COUNT))
  {
    byte specialMessage = minute()/15;
    if(specialMessage == lastSpecialMessage[index])
    {
      return getDefaultMessage(index, dest, bufsz);
    }
    else
    {
      lastSpecialMessage[index] = specialMessage;
      switch(specialMessage)
      {
        case 0:
          return getMessage00(index, dest, bufsz);
        case 1:
          return getMessage15(index, dest, bufsz);
        case 2:
          return getMessage30(index, dest, bufsz);
        case 3:
          return getMessage45(index, dest, bufsz);
        default:
          //assert(0);
          return false;
      }
    }
  }
  return false;
}

