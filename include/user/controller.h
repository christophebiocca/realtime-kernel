#ifndef USER_CONTROLLER_H
#define USER_CONTROLLER_H   1

#include <user/track_node.h>

void controllerInit(void);
void controllerQuit(void);

// this will block until the engineer is ready to receive commands
void controllerPrepareTrain(int train_id);

// meant to be called only by the respective engineer
void controllerUpdatePosition(int train_id, struct TrackNode *node, int mm);

// tells the controller to send the next sensor update to a specific engineer
void controllerSetExpectation(int train_id, int sensor, int number);

// primary way to interact with a train, send it to a particular distance from a
// node
void controllerSendTrain(int train_id, struct TrackNode *node, int mm);

// meant to be called only by the sensor server
void controllerSensorTriggered(int sensor, int number);

// returns non-zero if the request could not be fulfilled
// meant to be called by engineers
int controllerTurnoutCurve(int address);
int controllerTurnoutStraight(int address);

#endif
