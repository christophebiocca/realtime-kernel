#ifndef USER_SENSOR_H
#define USER_SENSOR_H   1

#include <user/track_data.h>

typedef char Sensor;

#define SENSOR_ENCODE(box, offset)  (((box) << 4) | offset)
#define SENSOR_DECODE_BOX(sensor)   (((sensor) & 0xf0) >> 4)
#define SENSOR_DECODE_OFFSET(off)   ((off) & 0x0f)

#define SENSOR_INVALID              0xff
#define SENSOR_BOX_MAX              5
#define SENSOR_OFFSET_MAX           16

void sensorInit(void);
void sensorQuit(void);

static inline struct TrackNode *nodeForSensor(Sensor sensor){
    char lookup[4];
    lookup[2] = lookup[3] = 0;
    lookup[0] = 'A' + SENSOR_DECODE_BOX(sensor);
    int number = 1 + SENSOR_DECODE_OFFSET(sensor);
    if(number >= 10){
        lookup[1] = '0' + number / 10;
        lookup[2] = '0' + number % 10;
    } else {
        lookup[1] = '0' + number;
    }
    return lookupTrackNode(hashtbl, lookup);
}

#endif /* USER_SENSOR_H */
