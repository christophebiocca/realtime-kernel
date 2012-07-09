#ifndef USER_CONTROLLER_H
#define USER_CONTROLLER_H   1

#include <user/sensor.h>
#include <user/track_data.h>

void controllerInit(void);
void controllerQuit(void);

// this will block until the engineer is ready to receive commands
void controllerPrepareTrain(int train_id);

// meant to be called only by the respective engineer
void controllerUpdatePosition(
    int couriertid,
    int train_id,
    struct TrackNode *node,
    int mm,
    int error
);

// tells the controller to send the next sensor update to a specific engineer
void controllerSetExpectation(
    int couriertid,
    int train_id,
    Sensor primary,
    Sensor secondary,
    Sensor alternative
);

// primary way to interact with a train, send it to a particular distance from a
// node
void controllerSendTrain(int train_id, struct TrackNode *node, int mm);

// meant to be called only by the sensor server
void controllerSensorTriggered(Sensor sensor);

int controllerCourier(int sendertid);

#endif
