#ifndef USER_ENGINEER_H
#define USER_ENGINEER_H 1

#include <stdbool.h>
#include <user/track_data.h>

extern volatile int engineerID;
int engineerCreate(int trainID);
void engineerQuit(int engineer_tid);

// send an engineer to dest + mm
void engineerSend(int engineer_tid, struct TrackNode *dest, int mm);

// tell an engineer that a sensor they expected was triggered
// 0 <= sensor <= 4
// 0 <= number <= 15
void engineerSensorTriggered(int engineer_tid, int sensor, int number);

#endif
