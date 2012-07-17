#include <debug.h>
#include <lib.h>
#include <stdbool.h>

#include <user/controller.h>
#include <user/engineer.h>
#include <user/log.h>
#include <user/mio.h>
#include <user/priorities.h>
#include <user/syscall.h>
#include <user/turnout.h>
#include <user/vt100.h>
#include <user/courier.h>

#define MAX_TRAINS          80

#define VALIDATE_TRAIN_ID(id)       assert((id) >= 0 && (id) < MAX_TRAINS)

enum {
    PREPARE_TRAIN,
    UPDATE_POSITION,
    SET_EXPECTATION,
    SEND_TRAIN,
    SENSOR_TRIGGERED,
    RESERVATION,
    QUIT
};

#define MAX_TRACK_EDGES 5
struct ControllerMessage {
    // should be the above enum
    // using an int here to explicitly specify size
    int type;

    union {
        // makes the size of this struct a multiple of 4 words
        char padding_DO_NOT_USE[28];

        struct PrepareTrainMessage {
            int train_id;
        } prepareTrain;

        struct UpdatePositionMessage {
            int train_id;
            struct TrackNode *node;
            int mm;

            int error;
        } updatePosition;

        struct SetExpectationMessage {
            int train_id;
            Sensor primary;
            Sensor secondary;
            Sensor alternative;
        } setExpectation;

        struct SendTrainMessage {
            int train_id;
            struct TrackNode *node;
            int mm;
        } sendTrain;

        struct SensorTriggeredMessage {
            Sensor sensor;
        } sensorTriggered;

        struct Reservation {
            enum {
                RESERVE,
                RELEASE
            } op;

            int train_id;
            struct TrackEdge *edges[MAX_TRACK_EDGES];
        } reservation;
    };
};

#define MAX_AWAITING_TRAINS 16
static void controllerServer(void) {
    logAssoc("tc");
    struct {
        int engineer_tid;
        struct TrackNode *node;
        int mm;
        int screen_row_offset;

        Sensor primary;
        Sensor secondary;
        Sensor alternative;
    } train_status[MAX_TRAINS];

    int current_screen_row_offset = 0;

    int expectations[SENSOR_BOX_MAX][SENSOR_OFFSET_MAX];

    for (int i = 0; i < MAX_TRAINS; ++i) {
        train_status[i].engineer_tid = -1;
        train_status[i].primary = SENSOR_INVALID;
        train_status[i].secondary = SENSOR_INVALID;
        train_status[i].alternative = SENSOR_INVALID;
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

                struct String s;
                sinit(&s);
                sputstr(&s, "prepare train ");
                sputint(&s, train_id, 10);
                logS(&s);

                VALIDATE_TRAIN_ID(train_id);
                assert(train_status[train_id].engineer_tid == -1);

                // curve all the turnouts in the inner loops
                turnoutCurve(8, NULLPTR);
                turnoutCurve(9, NULLPTR);
                turnoutCurve(10, NULLPTR);
                turnoutCurve(13, NULLPTR);
                turnoutCurve(14, NULLPTR);
                turnoutCurve(15, NULLPTR);
                turnoutCurve(16, NULLPTR);
                turnoutCurve(17, NULLPTR);
                turnoutCurve(153, NULLPTR);
                turnoutCurve(154, NULLPTR);
                turnoutCurve(155, NULLPTR);
                turnoutCurve(156, NULLPTR);

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

                    train_status[train_id].screen_row_offset =
                        current_screen_row_offset++;
                }

                train_status[train_id].node = request.updatePosition.node;
                train_status[train_id].mm = request.updatePosition.mm;

                struct String s;
                sinit(&s);
                sputstr(&s, CURSOR_SAVE);
                sputstr(&s, CURSOR_HIDE);
                vtPos(
                    &s,
                    TRAIN_ROW + train_status[train_id].screen_row_offset,
                    1
                );
                sputstr(&s, CLEAR_LINE);

                sputstr(&s, "T");
                sputint(&s, train_id, 10);
                sputstr(&s, " [");
                sputint(&s, train_status[train_id].engineer_tid, 10);
                sputstr(&s, "]: ");
                sputstr(&s, request.updatePosition.node->name);
                sputc(&s, ' ');
                sputint(&s, request.updatePosition.mm, 10);
                sputstr(&s, "mm - E: ");
                sputint(&s, request.updatePosition.error, 10);

                sputstr(&s, CURSOR_SHOW);
                sputstr(&s, CURSOR_RESTORE);
                mioPrint(&s);

                break;
            }

            case SET_EXPECTATION: {
                int train_id = request.setExpectation.train_id;
                VALIDATE_TRAIN_ID(train_id);

                // clear any previous expectation set by this engineer
                // FIXME: copy pasted from SENSOR_TRIGGERED
                {
                    Sensor sensor = train_status[train_id].primary;
                    if (sensor != SENSOR_INVALID) {
                        expectations[SENSOR_DECODE_BOX(sensor)]
                            [SENSOR_DECODE_OFFSET(sensor)] = -1;
                    }

                    sensor = train_status[train_id].secondary;
                    if (sensor != SENSOR_INVALID) {
                        expectations[SENSOR_DECODE_BOX(sensor)]
                            [SENSOR_DECODE_OFFSET(sensor)] = -1;
                    }

                    sensor = train_status[train_id].alternative;
                    if (sensor != SENSOR_INVALID) {
                        expectations[SENSOR_DECODE_BOX(sensor)]
                            [SENSOR_DECODE_OFFSET(sensor)] = -1;
                    }
                }

                // FIXME: Copy-paste *choke*
                Sensor sensor = request.setExpectation.primary;
                if (sensor != SENSOR_INVALID) {
                    int *fill = &expectations[SENSOR_DECODE_BOX(sensor)]
                        [SENSOR_DECODE_OFFSET(sensor)];

                    if (*fill != -1) {
                        struct String s;
                        sinit(&s);
                        sputstr(&s, "Train ");
                        sputint(&s, request.setExpectation.train_id, 10);
                        sputstr(&s, " tried to expect ");
                        sputc(&s, 'A' + SENSOR_DECODE_BOX(sensor));
                        sputint(&s, SENSOR_DECODE_OFFSET(sensor), 10);
                        sputstr(&s, ", owned by ");
                        sputint(&s, *fill, 10);
                        logS(&s);

                        assert(0);
                    }
                    *fill = request.setExpectation.train_id;
                }

                sensor = request.setExpectation.secondary;
                if (sensor != SENSOR_INVALID) {
                    int *fill = &expectations[SENSOR_DECODE_BOX(sensor)]
                        [SENSOR_DECODE_OFFSET(sensor)];

                    if (*fill != -1) {
                        struct String s;
                        sinit(&s);
                        sputstr(&s, "Train ");
                        sputint(&s, request.setExpectation.train_id, 10);
                        sputstr(&s, " tried to expect ");
                        sputc(&s, 'A' + SENSOR_DECODE_BOX(sensor));
                        sputint(&s, SENSOR_DECODE_OFFSET(sensor), 10);
                        sputstr(&s, ", owned by ");
                        sputint(&s, *fill, 10);
                        logS(&s);

                        assert(0);
                    }
                    *fill = request.setExpectation.train_id;
                }

                sensor = request.setExpectation.alternative;
                if (sensor != SENSOR_INVALID) {
                    int *fill = &expectations[SENSOR_DECODE_BOX(sensor)]
                        [SENSOR_DECODE_OFFSET(sensor)];

                    if (*fill != -1) {
                        struct String s;
                        sinit(&s);
                        sputstr(&s, "Train ");
                        sputint(&s, request.setExpectation.train_id, 10);
                        sputstr(&s, " tried to expect ");
                        sputc(&s, 'A' + SENSOR_DECODE_BOX(sensor));
                        sputint(&s, SENSOR_DECODE_OFFSET(sensor), 10);
                        sputstr(&s, ", owned by ");
                        sputint(&s, *fill, 10);
                        logS(&s);

                        assert(0);
                    }
                    *fill = request.setExpectation.train_id;
                }

                train_status[train_id].primary =
                    request.setExpectation.primary;
                train_status[train_id].secondary =
                    request.setExpectation.secondary;
                train_status[train_id].alternative =
                    request.setExpectation.alternative;

                break;
            }

            case SEND_TRAIN: {
                int train_id = request.sendTrain.train_id;
                VALIDATE_TRAIN_ID(train_id);

                int engineer_tid = train_status[train_id].engineer_tid;
                assert(engineer_tid >= 0);

                struct String s;
                sinit(&s);

                if (train_status[train_id].node != NULLPTR) {
                    sputstr(&s, "send train ");
                    sputint(&s, train_id, 10);
                    sputstr(&s, " around ");
                    sputstr(&s, request.sendTrain.node->name);
                    logS(&s);

                    engineerSend(
                        engineer_tid,
                        request.sendTrain.node,
                        request.sendTrain.mm
                    );
                } else {
                    sputstr(&s, "engineer for ");
                    sputint(&s, train_id, 10);
                    sputstr(&s, " not ready");
                    logS(&s);
                }

                break;
            }

            case SENSOR_TRIGGERED: {
                int box = SENSOR_DECODE_BOX(request.sensorTriggered.sensor);
                int offset = SENSOR_DECODE_OFFSET(
                    request.sensorTriggered.sensor
                );
                int train_id = expectations[box][offset];

                if (train_id >= 0 && train_id < MAX_TRAINS) {
                    engineerSensorTriggered(
                        train_status[train_id].engineer_tid,
                        request.sensorTriggered.sensor
                    );

                    // clear all expectations
                    // FIXME: moar copy-pasta
                    Sensor sensor = train_status[train_id].primary;
                    if (sensor != SENSOR_INVALID) {
                        expectations[SENSOR_DECODE_BOX(sensor)]
                            [SENSOR_DECODE_OFFSET(sensor)] = -1;
                    }

                    sensor = train_status[train_id].secondary;
                    if (sensor != SENSOR_INVALID) {
                        expectations[SENSOR_DECODE_BOX(sensor)]
                            [SENSOR_DECODE_OFFSET(sensor)] = -1;
                    }

                    sensor = train_status[train_id].alternative;
                    if (sensor != SENSOR_INVALID) {
                        expectations[SENSOR_DECODE_BOX(sensor)]
                            [SENSOR_DECODE_OFFSET(sensor)] = -1;
                    }

                    train_status[train_id].primary = SENSOR_INVALID;
                    train_status[train_id].secondary = SENSOR_INVALID;
                    train_status[train_id].alternative = SENSOR_INVALID;

                    // FIXME: do track modifications based on which expectation
                    // was triggered
                } else {
                    // no one expected this sensor
                    // send it to everyone awaiting

                    // make sure someone is awaiting
                    //assert(awaiting_trains.head != awaiting_trains.tail);
                    if (awaiting_trains.head == awaiting_trains.tail) {
                        struct String s;
                        sinit(&s);
                        sputstr(&s, "unexpected sensor: ");
                        sputint(&s, box, 10);
                        sputstr(&s, ", ");
                        sputint(&s, offset, 10);
                        logS(&s);
                    }

                    for (int i = awaiting_trains.head;
                            i != awaiting_trains.tail;
                            i = (i + 1) % MAX_AWAITING_TRAINS) {
                        engineerSensorTriggered(
                            awaiting_trains.tid_buffer[i],
                            request.sensorTriggered.sensor
                        );
                    }
                }

                break;
            }

            case RESERVATION: {
                if (request.reservation.op == RESERVE) {
                    bool will_succeed = true;

                    for (int i = 0; i < MAX_TRACK_EDGES; ++i) {
                        struct TrackEdge *edge = request.reservation.edges[i];

                        if (edge == NULLPTR) {
                            break;
                        }

                        // ensure consistency
                        if (edge->reserved != edge->reverse->reserved) {
                            struct String s;
                            sinit(&s);
                            sputstr(&s, "IR: [");
                            sputstr(&s, edge->src->name);
                            sputstr(&s, "->");
                            sputstr(&s, edge->dest->name);
                            sputstr(&s, "]: ");
                            sputint(&s, edge->reserved, 10);
                            sputstr(&s, ", [");
                            sputstr(&s, edge->reverse->src->name);
                            sputstr(&s, "->");
                            sputstr(&s, edge->reverse->dest->name);
                            sputstr(&s, "]: ");
                            sputint(&s, edge->reverse->reserved, 10);
                            logS(&s);

                            assert(0);
                        }

                        if (edge->reserved != request.reservation.train_id
                                && edge->reserved != -1) {
                            will_succeed = false;
                            break;
                        }
                    }

                    if (!will_succeed) {
                        // FIXME: log failures?
                        break;
                    }

                    for (int i = 0; i < MAX_TRACK_EDGES; ++i) {
                        struct TrackEdge *edge = request.reservation.edges[i];

                        if (edge == NULLPTR) {
                            break;
                        }

                        if (edge->reserved == -1) {
                            struct String s;
                            sinit(&s);
                            sputstr(&s, YELLOW);
                            sputstr(&s, "Train ");
                            sputint(&s, request.reservation.train_id, 10);
                            sputstr(&s, " <--- ");
                            sputstr(&s, edge->src->name);
                            sputstr(&s, " to ");
                            sputstr(&s, edge->dest->name);
                            sputstr(&s, RESET);
                            logS(&s);
                        }

                        edge->reserved = edge->reverse->reserved =
                            request.reservation.train_id;
                    }

                } else if (request.reservation.op == RELEASE) {
                    for (int i = 0; i < MAX_TRACK_EDGES; ++i) {
                        struct TrackEdge *edge = request.reservation.edges[i];

                        if (edge == NULLPTR) {
                            break;
                        }

                        if (edge->reserved == request.reservation.train_id) {
                            edge->reserved = edge->reverse->reserved = -1;

                            struct String s;
                            sinit(&s);
                            sputstr(&s, GREEN);
                            sputstr(&s, "Train ");
                            sputint(&s, request.reservation.train_id, 10);
                            sputstr(&s, " ---> ");
                            sputstr(&s, edge->src->name);
                            sputstr(&s, " to ");
                            sputstr(&s, edge->dest->name);
                            sputstr(&s, RESET);
                            logS(&s);
                        }
                    }
                }

                break;
            }

            case QUIT: {
                logC("quitting engineers");
                for (int i = 0; i < MAX_TRAINS; ++i) {
                    if (train_status[i].engineer_tid >= 0) {
                        engineerQuit(train_status[i].engineer_tid);
                    }
                }

                logC("quitting");
                quitting = true;

                break;
            }
        }
    }

    Exit();
}

static int g_controller_server_tid;
void controllerInit(void) {
    static_assert(sizeof(struct ControllerMessage) % 16 == 0);
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

void controllerUpdatePosition(int couriertid, int train_id,
        struct TrackNode *node, int mm, int error) {
    struct ControllerMessage msg;

    msg.type = UPDATE_POSITION;
    msg.updatePosition.train_id = train_id;
    msg.updatePosition.node = node;
    msg.updatePosition.mm = mm;
    msg.updatePosition.error = error;

    int ret = Reply(
        couriertid,
        (char *) &msg, sizeof(struct ControllerMessage)
    );
    (void)(ret);
    assert(ret == 0);
}

void controllerSetExpectation(int couriertid, int train_id,
        Sensor primary, Sensor secondary, Sensor alternative) {

    struct ControllerMessage msg;

    msg.type = SET_EXPECTATION;
    msg.setExpectation.train_id = train_id;
    msg.setExpectation.primary = primary;
    msg.setExpectation.secondary = secondary;
    msg.setExpectation.alternative = alternative;

    int ret = Reply(
        couriertid,
        (char *) &msg, sizeof(struct ControllerMessage)
    );
    (void)(ret);
    assert(ret == 0);
}

void controllerReserve(int courier_tid, int train_id,
        struct TrackEdge *edge1, struct TrackEdge *edge2,
        struct TrackEdge *edge3, struct TrackEdge *edge4,
        struct TrackEdge *edge5) {

    struct ControllerMessage msg;
    msg.type = RESERVATION;
    msg.reservation.op = RESERVE;
    msg.reservation.train_id = train_id;

    msg.reservation.edges[0] = edge1;
    msg.reservation.edges[1] = edge2;
    msg.reservation.edges[2] = edge3;
    msg.reservation.edges[3] = edge4;
    msg.reservation.edges[4] = edge5;

    int ret = Reply(
        courier_tid,
        (char *) &msg, sizeof(struct ControllerMessage)
    );
    (void) ret;
    assert(ret == 0);
}

void controllerRelease(int courier_tid, int train_id,
        struct TrackEdge *edge1, struct TrackEdge *edge2,
        struct TrackEdge *edge3, struct TrackEdge *edge4,
        struct TrackEdge *edge5) {

    struct ControllerMessage msg;

    msg.type = RESERVATION;
    msg.reservation.op = RELEASE;
    msg.reservation.train_id = train_id;

    msg.reservation.edges[0] = edge1;
    msg.reservation.edges[1] = edge2;
    msg.reservation.edges[2] = edge3;
    msg.reservation.edges[3] = edge4;
    msg.reservation.edges[4] = edge5;

    int ret = Reply(
        courier_tid,
        (char *) &msg, sizeof(struct ControllerMessage)
    );
    (void) ret;
    assert(ret == 0);
}

void controllerBlockingReserve(int train_id, struct TrackEdge *edge) {
    struct ControllerMessage msg;
    msg.type = RESERVATION;
    msg.reservation.op = RESERVE;
    msg.reservation.train_id = train_id;
    msg.reservation.edges[0] = edge;
    msg.reservation.edges[1] = NULLPTR;

    Send(
        g_controller_server_tid,
        (char *) &msg, sizeof(struct ControllerMessage),
        (char *) 0, 0
    );
}

void controllerBlockingRelease(int train_id, struct TrackEdge *edge) {
    struct ControllerMessage msg;
    msg.type = RESERVATION;
    msg.reservation.op = RELEASE;
    msg.reservation.train_id = train_id;
    msg.reservation.edges[0] = edge;
    msg.reservation.edges[1] = NULLPTR;

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

void controllerSensorTriggered(Sensor sensor) {
    struct ControllerMessage msg;

    msg.type = SENSOR_TRIGGERED;
    msg.sensorTriggered.sensor = sensor;

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

int controllerCourier(int sendertid){
    return CreateArgs(
        TASK_PRIORITY,
        courier,
        3,
        sendertid,
        sizeof(struct ControllerMessage),
        g_controller_server_tid
    );
}
