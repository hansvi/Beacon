#ifndef SENSORS_H_
#define SENSORS_H_

void sensorsInit();
void sensorsTick();
  
int maxAnalogStrSize(int channel);
int readAnalogSensor(char *dest, int channel);

int maxTemperatureStrSize(int channel);
int readTemperatureSensor(char *dest, int channel);

#endif
