#include "ControlPanel.h"
#include "Config.h"
#include "BeaconController.h"
#include "Sensors.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <TimeLib.h>
#include <assert.h>

extern Beacon beacons[BEACON_COUNT];
static byte lastSpecialMessage[BEACON_COUNT]; // 0=H00, 1=H15, 2=H30, 3=H45

static const char* msg_filenames[5] = { "H00", "H15", "H30", "H45", "DEF" };

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
      beacons[i].setEnabled(true);
    }
    else
    {
      beacons[i].setEnabled(false);
    }
  }
  if(!SD.exists("/log"))
  {
    SD.mkdir("/log");
  }
}

bool isBeaconRunning(int beacon_nr)
{
  if((beacon_nr>=0) && (beacon_nr<BEACON_COUNT))
  {
    return beacons[beacon_nr].getEnabled();
  }
  return false;
}

void setBeaconRunning(int beacon_nr, bool state)
{
  File f;
  char filename[20];
  
  if((beacon_nr>=0) && (beacon_nr<BEACON_COUNT))
  {
    sprintf(filename, "%d/ON", beacon_nr);
    beacons[beacon_nr].setEnabled(state);
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

bool getBeaconMessage(int beacon_nr, int msg_index, char *dest, int bufsz)
{
  char filename[20];
  File f;
  if((beacon_nr>=0) && (beacon_nr<BEACON_COUNT) && (msg_index>=0) && (msg_index <= BEACON_DEFMSG) && (bufsz>0))
  {
    dest[0]=0;
    sprintf(filename, "%d/%s.TXT", beacon_nr, msg_filenames[msg_index]);
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
  else
  {
    assert(0);
  }
  return false;
}

void setBeaconMessage(int beacon_nr, int msg_index, char *text)
{
  char filename[20];
  File f;
  if((beacon_nr>=0) && (beacon_nr<BEACON_COUNT) && (msg_index>=0) && (msg_index <= BEACON_DEFMSG))
  {
    sprintf(filename, "%d/%s.TXT", beacon_nr, msg_filenames[msg_index]);
    SD.remove(filename);
    f = SD.open(filename, FILE_WRITE);
    if(f)
    {
      f.println(text);
      f.close();
    }
  }
  else
  {
    assert(0);
  }
}

bool isBeaconMessageEnabled(int beacon_nr, int msg_index)
{
  char filename[20];
  File f;
  if((beacon_nr>=0) && (beacon_nr<BEACON_COUNT) && (msg_index>=0) && (msg_index <= BEACON_DEFMSG))
  {
    sprintf(filename, "%d/%s.ON", beacon_nr, msg_filenames[msg_index]);
    return SD.exists(filename);
  }
  else
  {
    assert(0);
  }
  return false;
}

void setBeaconMessageEnabled(int beacon_nr, int msg_index, bool enabled)
{
  char filename[20];
  File f;
  if((beacon_nr>=0) && (beacon_nr<BEACON_COUNT) && (msg_index>=0) && (msg_index <= BEACON_DEFMSG))
  {
    sprintf(filename, "%d/%s.ON", beacon_nr, msg_filenames[msg_index]);
    if(enabled)
    {
      f = SD.open(filename, FILE_WRITE);
      f.close();
    }
    else
    {
      SD.remove(filename);
    }
  }
  else
  {
    assert(0);
  }
}

bool getCurrentMessage(int beacon_nr, char *dest, int bufsz)
{
  if((beacon_nr>=0) && (beacon_nr<BEACON_COUNT) && (bufsz>0))
  {
    dest[0]=0;
    byte specialMessage = minute()/15;
    if(specialMessage != lastSpecialMessage[beacon_nr])
    {
      lastSpecialMessage[beacon_nr] = specialMessage;
      if(isBeaconMessageEnabled(beacon_nr, specialMessage))
      {
        getBeaconMessage(beacon_nr, specialMessage, dest, bufsz);
      }
    }
    if(dest[0]==0)
    {
      if(isBeaconMessageEnabled(beacon_nr, BEACON_DEFMSG))
      {
        getBeaconMessage(beacon_nr, BEACON_DEFMSG, dest, bufsz);
      }
    }
  }
  else
  {
    assert(0);
  }
  return dest[0]!=0;
}

static void logToFile(File &f, time_t timestamp)
{
  char logline[LOGLINE_SIZE];
  char *ptr = logline;
  int i;
  
  sprintf(ptr, "%02d:%02d\t", hour(timestamp), minute(timestamp));
  ptr += strlen(ptr);
  
  for(i=0; i<NUM_ANALOG_CHANNELS; i++)
  {
    if(i)
    {
      *ptr++='\t';
    }
    ptr += readAnalogSensor(ptr, i);
  }
  for(i=0; i<NUM_TEMPERATURE_CHANNELS; i++)
  {
    *ptr++='\t';
    ptr += readTemperatureSensor(ptr, i);
  }
  *ptr++='\n';
  *ptr=0;
  f.print(logline);
}

void writeLog(time_t timestamp)
{
  if(timeStatus() == timeNotSet)
  {
    // Do not log if the time is unknown
    return;
  }
  // eg: /log/2016/12/31.log
  char filename[21];
  File f;
  int log_year = year(timestamp);
  int log_month = month(timestamp);
  int log_day = day(timestamp);
  int log_hour = hour(timestamp);
  int log_minute = minute(timestamp);
  sprintf(filename, "/log/%04d/%02d/%02d.csv", log_year, log_month, log_day);
  if(!SD.exists(filename))
  {
    sprintf(filename, "/log/%04d", log_year);
    if(!SD.exists(filename))
    {
      SD.mkdir(filename);
    }
    sprintf(filename, "/log/%04d/%02d", log_year, log_month);
    if(!SD.exists(filename))
    {
      SD.mkdir(filename);
    }
    sprintf(filename, "/log/%04d/%02d/%02d.csv", log_year, log_month, log_day);
  }
  f = SD.open(filename, FILE_WRITE);
  if(f)
  {
    logToFile(f, timestamp);
    f.close();
  }
}
