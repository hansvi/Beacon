#include "Sensors.h"
#include "Config.h"
#include <Arduino.h>

#include <OneWire.h>
#include <DallasTemperature.h>

OneWire oneWire(2);
DallasTemperature sensors(&oneWire);

static int analogInputs[NUM_ANALOG_CHANNELS]; // 0-1023
static int temperatureInputs[NUM_TEMPERATURE_CHANNELS]; // in 1 degrees C
static int temperatureDeviceCount = 0;

/* sensorsTick state */
static unsigned long lastConversion = 0;
static const int conversionInterval = 1000; // every second
static int currentAnalogChannel = 0;
/* end sensorTick state */

void sensorsInit()
{
  sensors.begin();
  // initial request is synchronous
  temperatureDeviceCount = sensors.getDeviceCount();
  if(temperatureDeviceCount > NUM_TEMPERATURE_CHANNELS)
  {
    temperatureDeviceCount = NUM_TEMPERATURE_CHANNELS;
  }
  sensors.requestTemperatures();
  sensors.setWaitForConversion(false);  // during looping we don't want to wait
  int i;
  for(i=0; i<NUM_ANALOG_CHANNELS; i++)
  {
    analogInputs[i] = analogRead(i);
  }
  for(i=0; i<temperatureDeviceCount; i++)
  {
    temperatureInputs[i] = sensors.getTempCByIndex(i);
  }
  lastConversion = millis();
}

/*!
 * Periodically read analog inputs and temperature sensors
 */
void sensorsTick()
{
  unsigned long now = millis();
  if((now - lastConversion) > conversionInterval)
  {
    // the interval is more than long enough for a conversion to complete
    // Read in the values, and start a new conversion
    for(int i=0; i<temperatureDeviceCount; i++)
    {
      temperatureInputs[i] = sensors.getTempCByIndex(i);
    }
    // Get a new conversion started
    sensors.requestTemperatures();
    lastConversion = millis();
  }
  analogInputs[currentAnalogChannel]=analogRead(currentAnalogChannel);
  currentAnalogChannel++;
  if(currentAnalogChannel==NUM_ANALOG_CHANNELS)
  {
    currentAnalogChannel=0;
  }
}

/*!
 * Give the worst case string size of a temperature channel 
 *
 * \param channel  Number of the sensor
 *
 * \return The length of the string just written (not including the terminating zero)
 */
int maxAnalogStrSize(int channel)
{
  if((channel>=0) && (channel<NUM_ANALOG_CHANNELS))
  {
    // Format will be eg. "4V2"
    return 3;
  }
  else
  {
    // write "ERR"
    return 3;
  }
}

/*!
 * Read the voltage of an analog input as a null-terminated string value. Returns the length of the string.
 *
 * \param dest     String where to write the result to.
 * \param channel  Number of the sensor
 *
 * \return The length of the string just written (not including the terminating zero)
 */
int readAnalogSensor(char *dest, int channel)
{
  if((channel>=0) && (channel<NUM_ANALOG_CHANNELS))
  {
    int value = analogInputs[channel]  + (1023/50/2);
    // will have to reach 50*1023, an int will not suffice. Use long int, to be safe
    long int bigvalue = value;
    bigvalue *= 50;
    bigvalue /= 1023;
    value = bigvalue;
    dest[0] = '0' + value/10;
    dest[1] = 'V';
    dest[2] = '0' + (value%10);
    dest[3]=0;
    return 3;
  }
  else
  {
    dest[0]='E';
    dest[1]='R';
    dest[2]='R';
    dest[3]=0;
    return 3;
  }
}

/*!
 * Give the worst case string size of a temperature channel 
 *
 * \param channel  Number of the sensor
 *
 * \return The length of the string just written (not including the terminating zero)
 */
int maxTemperatureStrSize(int channel)
{
  if((channel>=0) && (channel<temperatureDeviceCount))
  {
    // Example: "-11C"
    return 4;
  }
  else
  {
    // write "ERR"
    return 3;
  }
}

/*!
 * Read the temperature of a sensor as a null-terminated string value. Returns the length of the string.
 *
 * \param dest     String where to write the result to.
 * \param channel  Number of the sensor
 *
 * \return The length of the string just written (not including the terminating zero)
 */
int readTemperatureSensor(char *dest, int channel)
{
  if((channel>=0) && (channel<temperatureDeviceCount))
  {
    int t = temperatureInputs[channel];
    if((t<-55) || (t>125))
    {
      dest[0] = 'E';
      dest[1] = 'R';
      dest[2] = 'N';
      dest[3] = 'G';
      dest[4] = 0;
      return 4;
    }
    else
    {
      int i=0;
      if(t<0)
      {
        dest[i++] = '-';
        t = -t;
      }
      if(t>100)
      {
        dest[i++]='1';
        t-=100;
      }
      if(t>=10)
      {
        int tens = t/10;
        dest[i++] = '0' + tens;
        t -= tens * 10;
      }
      dest[i++] = '0' + t;
      dest[i++] = 'C';
      dest[i] = 0;
      return i;
    }
  }
  else
  {
    dest[0]='E';
    dest[1]='R';
    dest[2]='R';
    dest[3]=0;
    return 3;
  }
}

