#ifndef USER_ENGINEER_H
#define USER_ENGINEER_H 1

#include <stdbool.h>
#include <user/track_data.h>

extern volatile int engineerID;
int engineerCreate(int trainID);
void engineerQuit(int engineer_tid);

void engineerCircle(int engineer_tid, int circle_mode);
void engineerRandom(int engineer_tid);

// send an engineer to dest + mm
void engineerSend(int engineer_tid, struct TrackNode *dest, int mm);

// tell an engineer that a sensor they expected was triggered
void engineerSensorTriggered(int engineer_tid, Sensor sensor);

void engineerDumpReservations(int engineer_tid);

#endif
