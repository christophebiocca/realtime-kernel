#include <user/engineer.h>
#include <user/mio.h>
#include <user/tio.h>
#include <user/string.h>
#include <user/clock.h>
#include <user/priorities.h>
#include <user/syscall.h>
#include <stdbool.h>

void engineer(int trainID){
    (void) trainID;
}

void initEngineer(int trainID){
    (void) trainID;
    //initTrackA(trackNodes, hashtbl);
}

int engineerCreate(int trainID){
    return CreateArgs(TASK_PRIORITY, engineer, 1, trainID);
}

void engineerSend(int engineer_tid, struct TrackNode *dest, int mm) {
    (void) engineer_tid;
    (void) dest;
    (void) mm;
}

void engineerSensorTriggered(int engineer_tid, int sensor, int number) {
    (void) engineer_tid;
    (void) sensor;
    (void) number;
}

void engineerQuit(int engineer_tid) {
    (void) engineer_tid;
}
