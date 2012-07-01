#include <debug.h>
#include <lib.h>
#include <stdbool.h>

#include <user/controller.h>
#include <user/engineer.h>
#include <user/priorities.h>
#include <user/turnout.h>
#include <user/syscall.h>
#include <user/vt100.h>
#include <user/mio.h>

#define MAX_TRAINS          80
#define MAX_SENSORS         5
#define MAX_SENSOR_NUMBER   16

#define VALIDATE_TRAIN_ID(id)       assert((id) >= 0 && (id) < MAX_TRAINS)
#define VALIDATE_SENSOR(id)         assert((id) >= 0 && (id) < MAX_SENSORS)
#define VALIDATE_SENSOR_NUMBER(n)   assert((n) >= 0 && (n) < MAX_SENSOR_NUMBER)

enum {
    PREPARE_TRAIN,
    UPDATE_POSITION,
    SET_EXPECTATION,
    SEND_TRAIN,
    SENSOR_TRIGGERED,
    TURNOUT_REQUEST,
    QUIT
};

struct ControllerMessage {
    // should be the above enum
    // using an int here to explicitly specify size
    int type;

    union {
        struct PrepareTrainMessage {
            int train_id;
        } prepareTrain;

        struct UpdatePositionMessage {
            int train_id;
            struct TrackNode *node;
            int mm;
        } updatePosition;

        struct SetExpectationMessage {
            int train_id;
            int sensor;
            int number;
        } setExpectation;

        struct SendTrainMessage {
            int train_id;
            struct TrackNode *node;
            int mm;
        } sendTrain;

        struct SensorTriggeredMessage {
            int sensor;
            int number;
        } sensorTriggered;

        struct TurnoutRequest {
            enum {
                CURVE,
                STRAIGHT
            } orientation;

            int address;
        } turnoutRequest;
    };
};

#define MAX_AWAITING_TRAINS 16
static void controllerServer(void) {
    struct {
        int engineer_tid;
        struct TrackNode *node;
        int mm;
    } train_status[MAX_TRAINS];
    int expectations[MAX_SENSORS][MAX_SENSOR_NUMBER];

    for (int i = 0; i < MAX_TRAINS; ++i) {
        train_status[i].engineer_tid = -1;
    }

    static_assert(sizeof(expectations) % 16 == 0);
    memset16(expectations, -1, sizeof(expectations));

    struct {
        int tid_buffer[MAX_AWAITING_TRAINS];
        int head;
        int tail;
    } awaiting_trains;
    awaiting_trains.head = 0;
    awaiting_trains.tail = 0;

    bool quitting = false;
    struct ControllerMessage request;
    int sender_tid;
    while (!quitting) {
        Receive(
            &sender_tid,
            (char *) &request,
            sizeof(struct ControllerMessage)
        );

        Reply(sender_tid, (char*) 0, 0);

        switch (request.type) {
            case PREPARE_TRAIN: {
                int train_id = request.prepareTrain.train_id;

                VALIDATE_TRAIN_ID(train_id);
                assert(train_status[train_id].engineer_tid == -1);

                train_status[train_id].engineer_tid =
                    engineerCreate(train_id);
                train_status[train_id].node = NULLPTR;

                awaiting_trains.tid_buffer[awaiting_trains.tail++] =
                    train_status[train_id].engineer_tid;
                awaiting_trains.tail %= MAX_AWAITING_TRAINS;

                break;
            }

            case UPDATE_POSITION: {
                int train_id = request.updatePosition.train_id;

                VALIDATE_TRAIN_ID(train_id);

                int engineer_tid = train_status[train_id].engineer_tid;
                assert(engineer_tid >= 0);

                if (train_status[train_id].node == NULLPTR) {
                    // engineer is checking in for the first time
                    // remove them from awaiting queue
                    int i;

                    for (i = awaiting_trains.head;
                            i != awaiting_trains.tail;
                            i = (i + 1) % MAX_AWAITING_TRAINS) {
                        if (awaiting_trains.tid_buffer[i] == engineer_tid) {
                            // swap current pos with head
                            int tmp = awaiting_trains.tid_buffer[i];
                            awaiting_trains.tid_buffer[i] =
                                awaiting_trains.tid_buffer[awaiting_trains.head];
                            awaiting_trains.tid_buffer[awaiting_trains.head] = tmp;

                            // throw away what's at head
                            awaiting_trains.head =
                                (awaiting_trains.head + 1) % MAX_AWAITING_TRAINS;

                            break;
                        }
                    }

                    // make sure we found an awaiting train
                    assert(i != awaiting_trains.tail);
                }

                train_status[train_id].node = request.updatePosition.node;
                train_status[train_id].mm = request.updatePosition.mm;

                // FIXME: multiple trains?
                struct String s;
                sinit(&s);
                sputstr(&s, CURSOR_SAVE);
                sputstr(&s, CURSOR_HIDE);
                vtPos(&s, TRAIN_ROW, 1);

                sputstr(&s, "Train: ");
                sputstr(&s, request.updatePosition.node->name);
                sputc(&s, ' ');
                sputint(&s, request.updatePosition.mm, 10);
                sputstr(&s, "mm");

                sputstr(&s, CURSOR_SHOW);
                sputstr(&s, CURSOR_RESTORE);
                mioPrint(&s);

                break;
            }

            case SET_EXPECTATION: {
                VALIDATE_TRAIN_ID(request.setExpectation.train_id);
                VALIDATE_SENSOR(request.setExpectation.sensor);
                VALIDATE_SENSOR_NUMBER(request.setExpectation.number);

                int *fill = &expectations[request.setExpectation.sensor]
                    [request.setExpectation.number];

                assert(*fill == -1);
                *fill = request.setExpectation.train_id;

                break;
            }

            case SEND_TRAIN: {
                int train_id = request.sendTrain.train_id;
                VALIDATE_TRAIN_ID(train_id);

                int engineer_tid = train_status[train_id].engineer_tid;
                assert(engineer_tid >= 0);

                if (train_status[train_id].node != NULLPTR) {
                    engineerSend(
                        engineer_tid,
                        request.sendTrain.node,
                        request.sendTrain.mm
                    );
                }

                // FIXME: log an error about engineer not being ready to be
                // routed

                break;
            }

            case SENSOR_TRIGGERED: {
                VALIDATE_SENSOR(request.sensorTriggered.sensor);
                VALIDATE_SENSOR_NUMBER(request.sensorTriggered.number);

                int train_id = expectations[request.sensorTriggered.sensor]
                    [request.sensorTriggered.number];

                if (train_id >= 0 && train_id < MAX_TRAINS) {
                    engineerSensorTriggered(
                        train_status[train_id].engineer_tid,
                        request.sensorTriggered.sensor,
                        request.sensorTriggered.number
                    );
                } else {
                    // no one expected this sensor
                    // send it to everyone awaiting

                    // make sure someone is awaiting
                    assert(awaiting_trains.head != awaiting_trains.tail);

                    for (int i = awaiting_trains.head;
                            i != awaiting_trains.tail;
                            i = (i + 1) % MAX_AWAITING_TRAINS) {
                        engineerSensorTriggered(
                            awaiting_trains.tid_buffer[i],
                            request.sensorTriggered.sensor,
                            request.sensorTriggered.number
                        );
                    }
                }

                break;
            }

            case TURNOUT_REQUEST: {
                switch (request.turnoutRequest.orientation) {
                    case CURVE:
                        turnoutCurve(request.turnoutRequest.address);
                        break;
                    case STRAIGHT:
                        turnoutStraight(request.turnoutRequest.address);
                        break;
                    default:
                        assert(0);
                }

                break;
            }

            case QUIT: {
                for (int i = 0; i < MAX_TRAINS; ++i) {
                    if (train_status[i].engineer_tid >= 0) {
                        engineerQuit(train_status[i].engineer_tid);
                    }
                }

                quitting = true;

                break;
            }
        }
    }

    Exit();
}

static int g_controller_server_tid;
void controllerInit(void) {
    static_assert(sizeof(struct ControllerMessage) == 16);
    g_controller_server_tid = Create(TASK_SERVER_PRIORITY, controllerServer);
    assert(g_controller_server_tid >= 0);
}

void controllerPrepareTrain(int train_id) {
    struct ControllerMessage msg;

    msg.type = PREPARE_TRAIN;
    msg.prepareTrain.train_id = train_id;

    Send(
        g_controller_server_tid,
        (char *) &msg, sizeof(struct ControllerMessage),
        (char *) 0, 0
    );
}

void controllerUpdatePosition(int train_id, struct TrackNode *node, int mm) {
    struct ControllerMessage msg;

    msg.type = UPDATE_POSITION;
    msg.updatePosition.train_id = train_id;
    msg.updatePosition.node = node;
    msg.updatePosition.mm = mm;

    Send(
        g_controller_server_tid,
        (char *) &msg, sizeof(struct ControllerMessage),
        (char *) 0, 0
    );
}

void controllerSetExpectation(int train_id, int sensor, int number) {
    struct ControllerMessage msg;

    msg.type = SET_EXPECTATION;
    msg.setExpectation.train_id = train_id;
    msg.setExpectation.sensor = sensor;
    msg.setExpectation.number = number;

    Send(
        g_controller_server_tid,
        (char *) &msg, sizeof(struct ControllerMessage),
        (char *) 0, 0
    );
}

void controllerSendTrain(int train_id, struct TrackNode *node, int mm) {
    struct ControllerMessage msg;

    msg.type = SEND_TRAIN;
    msg.sendTrain.train_id = train_id;
    msg.sendTrain.node = node;
    msg.sendTrain.mm = mm;

    Send(
        g_controller_server_tid,
        (char *) &msg, sizeof(struct ControllerMessage),
        (char *) 0, 0
    );
}

void controllerSensorTriggered(int sensor, int number) {
    struct ControllerMessage msg;

    msg.type = SENSOR_TRIGGERED;
    msg.sensorTriggered.sensor = sensor;
    msg.sensorTriggered.number = number;

    Send(
        g_controller_server_tid,
        (char *) &msg, sizeof(struct ControllerMessage),
        (char *) 0, 0
    );
}

void controllerTurnoutCurve(int address) {
    struct ControllerMessage msg;

    msg.type = TURNOUT_REQUEST;
    msg.turnoutRequest.orientation = CURVE;
    msg.turnoutRequest.address = address;

    Send(
        g_controller_server_tid,
        (char *) &msg, sizeof(struct ControllerMessage),
        (char *) 0, 0
    );
}

void controllerTurnoutStraight(int address) {
    struct ControllerMessage msg;

    msg.type = TURNOUT_REQUEST;
    msg.turnoutRequest.orientation = STRAIGHT;
    msg.turnoutRequest.address = address;

    Send(
        g_controller_server_tid,
        (char *) &msg, sizeof(struct ControllerMessage),
        (char *) 0, 0
    );
}

void controllerQuit(void) {
    struct ControllerMessage msg;

    msg.type = QUIT;

    Send(
        g_controller_server_tid,
        (char *) &msg, sizeof(struct ControllerMessage),
        (char *) 0, 0
    );
}
