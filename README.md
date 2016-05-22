# Beacon
CW Beacon control software for Arduino

## Dependencies
This software depends on the Time, OneWire and dallas-temperature-control libraries. Install them beforehand in the libraries folder of your Arduino sketches folder (see links).

## Installation
Attach the Arduino ethernet shield to the Arduino Mega.
On an empty micro SD flash card, copy the www folder. Insert the micro SD card in the Arduino ethernet shield SD slot.
Put this directory in the Arduino folder and start the IDE. Open the Beacon sketch.
Select the Config.h file, and configure the MAC address to match your ethernet shield, and the static IP address to match your network.
Select the Arduino Mega board and upload the sketch.


## Links
* Github: https://github.com/hansvi/Beacon
* OneWire library: http://playground.arduino.cc/Learning/OneWire
* Dallas Temperature sensor library: http://milesburton.com/Main_Page?title=Dallas_Temperature_Control_Library
* Time library (tested with v1.5): http://www.pjrc.com/teensy/td_libs_Time.html

## For future reference:
* https://sites.google.com/site/astudyofentropy/project-definition/timer-jitter-entropy-sources/entropy-library - random numbers
