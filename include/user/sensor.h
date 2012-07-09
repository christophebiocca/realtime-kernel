#ifndef USER_SENSOR_H
#define USER_SENSOR_H   1

#include <user/track_data.h>

void sensorInit(void);
void sensorQuit(void);

void sensorInterrupt(int train_number, int sensor, int sensor_number);
void sensorTimer(int sensor1, int sensor1_num, int sensor2, int sensor2_num);

static inline struct TrackNode *nodeForSensor(int sensor, int number){
    char lookup[4];
    lookup[2] = lookup[3] = 0;
    lookup[0] = 'A' + sensor;
    number += 1;
    if(number >= 10){
        lookup[1] = '0' + number / 10;
        lookup[2] = '0' + number % 10;
    } else {
        lookup[1] = '0' + number;
    }
    return lookupTrackNode(hashtbl, lookup);
}

#endif /* USER_SENSOR_H */
