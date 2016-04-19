#ifndef SENSORS_H_
#define SENSORS_H_

// Physical limitations
#define NUM_ANALOG_CHANNELS 16
#define NUM_TEMPERATURE_CHANNELS 8

void sensorsInit();
void sensorsTick();
  
int maxAnalogStrSize(int channel);
void readAnalogSensor(char *dest, int channel);

int maxTemperatureStrSize(int channel);
void readTemperatureSensor(char *dest, int channel);

#endif
