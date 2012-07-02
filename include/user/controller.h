#ifndef USER_CONTROLLER_H
#define USER_CONTROLLER_H   1

#include <user/track_data.h>

void controllerInit(void);
void controllerQuit(void);

// this will block until the engineer is ready to receive commands
void controllerPrepareTrain(int train_id);

// meant to be called only by the respective engineer
void controllerUpdatePosition(int couriertid, int train_id, struct TrackNode *node, int mm);

// tells the controller to send the next sensor update to a specific engineer
void controllerSetExpectation(int couriertid, int train_id, int sensor, int number);

// primary way to interact with a train, send it to a particular distance from a
// node
void controllerSendTrain(int train_id, struct TrackNode *node, int mm);

// meant to be called only by the sensor server
void controllerSensorTriggered(int sensor, int number);

// meant to be called by engineers
// FIXME: this could possibly fail when we implement reservations
void controllerTurnoutCurve(int couriertid, int address);
void controllerTurnoutStraight(int couriertid, int address);

int controllerCourier(int sendertid);

#endif
