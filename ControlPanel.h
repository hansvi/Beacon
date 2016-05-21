#ifndef CONTROLPANEL_H_
#define CONTROLPANEL_H_

#define BEACON_H00MSG 0
#define BEACON_H15MSG 1
#define BEACON_H30MSG 2
#define BEACON_H45MSG 3
#define BEACON_DEFMSG 4

void controlPanelInit();

bool isBeaconRunning(int beacon_nr);
void setBeaconRunning(int beacon_nr, bool state);

/*
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
*/

bool getBeaconMessage(int beacon_nr, int msg_index, char *dest, int bufsz);
void setBeaconMessage(int beacon_nr, int msg_index, char *text);

bool isBeaconMessageEnabled(int beacon_nr, int msg_index);
void setBeaconMessageEnabled(int beacon_nr, int msg_index, bool enabled);

bool getCurrentMessage(int index, char *dest, int bufsz);

#endif
