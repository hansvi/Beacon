#ifndef CONTROLPANEL_H_
#define CONTROLPANEL_H_

void controlPanelInit();

bool isBeaconRunning(int index);
void setBeaconRunning(int index, bool state);

bool getDefaultMessage(int index, char *dest, int bufsz);
bool getMessage00(int index, char *dest, int bufsz);
bool getMessage15(int index, char *dest, int bufsz);
bool getMessage30(int index, char *dest, int bufsz);
bool getMessage45(int index, char *dest, int bufsz);

void setDefaultMessage(int index, char *msg);
void setMessage00(int index, char *msg);
void setMessage15(int index, char *msg);
void setMessage30(int index, char *msg);
void setMessage45(int index, char *msg);

bool getCurrentMessage(int index, char *dest, int bufsz);

#endif
