#include <lib.h>
#include <stdbool.h>
#include <ts7200.h>

#include <user/priorities.h>
#include <user/string.h>
#include <user/syscall.h>
#include <user/tio.h>
#include <user/track_data.h>
#include <user/pathfinding.h>
#include <user/kinematics.h>
#include <user/sensor.h>

#include <user/clock.h>
#include <user/controller.h>
#include <user/engineer.h>
#include <user/log.h>

struct Train;

struct EngineerMessage {
    enum {
        GOTO,
        SENSOR,
        QUIT,
        NUM_MESSAGE_TYPES
    } messageType;
    union {
        char padding_DO_NOT_USE[12];

        struct Position destination;
        struct {
            Sensor sensor;
        } sensorTriggered;
    } content;
};

struct Messaging {
    int courier;
    bool courierReady;
    bool notifyPosition;
    bool notifyExpectation;
};

struct Timing {
    // What the timer is doing
    int timer;
    int nextFire;
    bool timerReady;

    // What we need to be woken up for (timestamps)
    int positionUpdate;
};

struct TrackControl {
    // Our understanding of the track/position.
    TurnoutTable turnouts;
    struct Position position;
    struct TrackNode *expectedSensor;

    // Only applicable when pathing.
    bool pathing;
    struct Position goal;
    struct TrackNode *path[50];
    struct TrackNode **pathCurrent;
};

struct Train {
    int id;
    struct Kinematics kinematics;
    struct Messaging messaging;
    struct Timing timing;
    struct TrackControl track;
};

static inline void recoverCourier(struct Train *train, int sender){
    assert(sender == train->messaging.courier);
    assert(!train->messaging.courierReady);
    train->messaging.courierReady = true;
}

static inline void courierUsed(struct Train *train){
    assert(train->messaging.courier);
    train->messaging.courierReady = false;
}

#define UPDATE_INTERVAL 10

static inline void reverse(struct Train *train){
    struct String s;
    sinit(&s);
    sputc(&s, 15);
    sputc(&s, train->id);
    tioPrint(&s);
}

static inline void setSpeed(struct Train *train, int speed){
    logC("SetSpeed");
    assert(0 <= train->id && train->id <= 80);
    assert(0 <= speed && speed <= 14);
    train->kinematics.target_speed = train->kinematics.ideal_speed[speed];
    {
        struct String s;
        sinit(&s);
        sputstr(&s, "ts:");
        sputuint(&s, train->kinematics.target_speed,10);
        logS(&s);
    }
    struct String s;
    sinit(&s);
    sputc(&s, speed);
    sputc(&s, train->id);
    tioPrint(&s);
}

static inline void updateExpectation(struct Train *train){
    // Find the next sensor along the path.
    struct Position end;
    struct TrackNode *path[50];
    int len = alongTrack(train->track.turnouts, &train->track.position,
        train->kinematics.stop/1000, &end, path, false);
    assert(len <= 50);
    struct TrackNode *nextSensor = 0;

    // Skip the first node, since that's where we are right now.
    for(int i = 1; i < len; ++i){
        if(path[i]->type == NODE_SENSOR){
            nextSensor = path[i];
            break;
        }
    }
    if(nextSensor && nextSensor != train->track.expectedSensor){
        train->track.expectedSensor = nextSensor;
        train->messaging.notifyExpectation = true;
    }
}

static inline void notifyExpectation(struct Train *train){
    if(train->messaging.notifyExpectation && train->messaging.courierReady){
        {
            struct String s;
            sinit(&s);
            sputstr(&s, "Expect:");
            sputstr(&s, train->track.expectedSensor->name);
            logS(&s);
        }
        controllerSetExpectation(
            train->messaging.courier,
            train->id,
            SENSOR_ENCODE(
                train->track.expectedSensor->num / 16,
                train->track.expectedSensor->num % 16
            ),
            // FIXME: set secondary and alternate expectations
            SENSOR_INVALID,
            SENSOR_INVALID
        );
        courierUsed(train);
        train->messaging.notifyExpectation = false;
    }
}

static inline void calculateStop(struct Train *train){
    if(train->track.pathing){
        int dist = distance(train->track.turnouts,
            &train->track.position,
            &train->track.goal);
        if(dist <= train->kinematics.stop/1000){
            {
                struct String s;
                sinit(&s);
                sputstr(&s, "Dist: ");
                sputint(&s, dist, 10);
                sputstr(&s, " Stop: ");
                sputint(&s, train->kinematics.stop, 10);
                logS(&s);
            }
            setSpeed(train,0);
        }
    }
}

static inline void updateTurnouts(struct Train *train){
    struct Position end;
    struct TrackNode *path[50];
    struct TrackNode **sweep = path;
    int len = alongTrack(train->track.turnouts, &train->track.position,
        train->kinematics.stop/1000, &end, path, false);
    assert(len <= 50);

    struct TrackNode **t = train->track.pathCurrent;
    while(*sweep != end.node){
        if((*sweep)->type == NODE_MERGE && sweep != path){
            // No conflict the merge nodes.
            struct TrackNode *prev = (*(sweep-1));
            if((*sweep)->reverse->edge[DIR_STRAIGHT].reverse->src == prev &&
                isTurnoutCurved(train->track.turnouts, (*sweep)->num)){
                turnoutStraight((*sweep)->num, &train->track.turnouts);
            } else if((*sweep)->reverse->edge[DIR_CURVED].reverse->src == prev &&
                isTurnoutStraight(train->track.turnouts, (*sweep)->num)){
                turnoutCurve((*sweep)->num, &train->track.turnouts);
            }
        } else if((*sweep)->type == NODE_BRANCH && train->track.pathing){
            // Find the first time we hit this branch
            for(; *t != *sweep; ++t);

            // How should it be set?
            // Make the branch match expectations
            if((*sweep)->edge[DIR_STRAIGHT].dest == t[1] &&
                isTurnoutCurved(train->track.turnouts, (*sweep)->num)){
                turnoutStraight((*sweep)->num, &train->track.turnouts);
            } else if((*sweep)->edge[DIR_CURVED].dest == t[1] &&
                isTurnoutStraight(train->track.turnouts, (*sweep)->num)){
                turnoutCurve((*sweep)->num, &train->track.turnouts);
            }
        }
        sweep++;
    }
}

static inline void updatePosition(struct Train *train, struct Position *pos){
    train->track.position.node = pos->node;
    train->track.position.offset = pos->offset;
    train->messaging.notifyPosition = true;
    if(train->track.pathing){
        while(*(train->track.pathCurrent) != pos->node &&
            *(train->track.pathCurrent) != train->track.goal.node){
            train->track.pathCurrent++;
        }
    }
    updateExpectation(train);
    updateTurnouts(train);
    calculateStop(train);
}

static inline void sensorUpdate(struct Train *train, Sensor sensor){
    struct Position pos;
    pos.node = nodeForSensor(sensor);
    pos.offset = 0;

    int now = Time();
    tick(&train->kinematics, now);
    train->kinematics.distance = 0;

    updatePosition(train, &pos);

    train->timing.positionUpdate = now + UPDATE_INTERVAL;
}

static inline void timerPositionUpdate(struct Train *train, int time){
    tick(&train->kinematics, time);
    struct Position pos;
    struct TrackNode *path[50];
    int len = alongTrack(train->track.turnouts, &train->track.position,
        train->kinematics.distance/1000, &pos, path, false);
    assert(len <= 50);
    // Always skip the start point.
    for(int i = 1; i < len; ++i){
        if(path[i]->type == NODE_SENSOR){
            // We can't possibly have reached here yet. We're still before this point.
            pos.node = path[i-1];
            pos.offset = 0;
            // For lack of anything better.
            pos.offset = train->kinematics.distance/1000 -
                distance(train->track.turnouts, &train->track.position, &pos);
            break;
        }
    }
    updatePosition(train, &pos);
    train->kinematics.distance = 0;
    train->timing.positionUpdate = time + UPDATE_INTERVAL;
}

static inline void notifyPosition(struct Train *train){
    if(train->messaging.notifyPosition && train->messaging.courierReady){
        controllerUpdatePosition(train->messaging.courier,
            train->id, train->track.position.node, train->track.position.offset, 0);
        courierUsed(train);
        train->messaging.notifyPosition = false;
    }
}

static inline void scheduleTimer(struct Train *train){
    int closestEvent = train->timing.positionUpdate;
    // Schedule a timer for the next event
    if(train->timing.timerReady){
        Reply(train->timing.timer, (char *)&closestEvent, sizeof(closestEvent));
        train->timing.timerReady = false;
        train->timing.nextFire = closestEvent;
    } else if(train->timing.nextFire > closestEvent){
        // TODO: Need to nuke our timer, set it up correctly.
        logC("Badtimer");
    }
}

static inline void trainNavigate(struct Train *train, struct Position *dest){
    // Starting point is here + stopping distance.
    struct Position pathStart;
    int i = alongTrack(train->track.turnouts,
        &train->track.position,
        train->kinematics.stop/1000,
        &pathStart, train->track.path, true);
    // Now plan a path from there to here.
    planPath(nodes, pathStart.node, dest->node, train->track.path + (i-1));
    train->track.pathing = true;
    train->track.pathCurrent = train->track.path;
    train->track.goal.node = dest->node;
    train->track.goal.offset = dest->offset;

    for(struct TrackNode **i = train->track.pathCurrent;; i++){
        logC((*i)->name);
        if(train->track.goal.node == *i){
            break;
        }
    }
}

void engineer(int trainID){
    logAssoc("en");
    {
        struct String s;
        sinit(&s);
        sputuint(&s, MyTid(), 10);
        logS(&s);
    }

    /* SET UP */

    struct Train train;
    train.id = trainID;
    train.track.pathing = false;

    // Spawn our helpers and claim them.
    {
        train.timing.timer = CreateArgs(TASK_PRIORITY, clockWaiter, 1, MyTid());
        assert(train.timing.timer > 0);
        int tid;
        int len = Receive(&tid, (char *)&train.kinematics.time, sizeof(int));
        assert(tid == train.timing.timer);
        assert(len == sizeof(int));
        assert(train.kinematics.time >= 0);
        train.timing.timerReady = true;
    }

    {
        train.messaging.courier = controllerCourier(MyTid());
        assert(train.messaging.courier > 0);
        int tid;
        int len = Receive(&tid, 0, 0);
        assert(tid == train.messaging.courier);
        assert(len == 0);
        train.messaging.courierReady = true;
    }

    // speeds in um / cs
    memset16(train.kinematics.ideal_speed, 0, sizeof(train.kinematics.ideal_speed));
    //computeSpeeds(trainID, ideal_speed, 8, 14);
    
    for(int i = 0; i < 15; ++i){
        train.kinematics.ideal_speed[i] = 0;
    }
    train.kinematics.ideal_speed[14] = 5480;
    train.kinematics.target_speed = 0;
    train.kinematics.current_speed = 0;
    train.kinematics.stop = 0;
    train.kinematics.acceleration = 0;
    // Set the time.
    train.kinematics.time = Time();
    train.kinematics.distance = 0;

    setSpeed(&train,14);

    // Go forth and hit a sensor.
    {
        // Wait for a sensor message update.
        int tid;
        struct EngineerMessage msg;

        int len = Receive(&tid, (char *)&msg, sizeof(struct EngineerMessage));
        assert(len == sizeof(struct EngineerMessage));
        assert(msg.messageType == SENSOR);

        int ret = Reply(tid, 0, 0);
        assert(ret == 0);

        // TODO: Get the actual turnout state from someone.
        train.track.turnouts = (1<<23)-1;

        // Set the track position
        sensorUpdate(&train, msg.content.sensorTriggered.sensor);

        switch (msg.content.sensorTriggered.sensor) {
            case 0x02:  // A03
            case 0x2a:  // C11
            case 0x4f:  // E16
            case 0x40:  // E01
            case 0x20:  // C01
            case 0x13:  // B04
            case 0x28:  // C09
            case 0x1e:  // B15
            case 0x1d:  // B14
            case 0x3f:  // D16
            case 0x4d:  // E14
            case 0x48:  // E09
            case 0x34:  // D05
            case 0x45:  // E06
            case 0x42:  // E03
            case 0x30:  // D01
                train.kinematics.orientation = FORWARD;
                logC("Pickup at front");
                break;

            case 0x1f:  // B16
            case 0x29:  // C10
            case 0x12:  // B03
            case 0x21:  // C02
            case 0x41:  // E02
            case 0x4e:  // E15
            case 0x3b:  // C12
            case 0x03:  // A04
            case 0x31:  // D02
            case 0x43:  // E04
            case 0x44:  // E05
            case 0x35:  // D06
            case 0x49:  // E10
            case 0x4c:  // E13
            case 0x3e:  // D15
            case 0x1c:  // B13
                train.kinematics.orientation = BACKWARD;
                logC("Pickup at back");
                break;

            default:
                logC("Don't know orientation, assuming front");
                train.kinematics.orientation = FORWARD;
                break;
        }
    }

    setSpeed(&train, 14);

    while(true) {
        // Act on the outside world.
        notifyPosition(&train);
        notifyExpectation(&train);
        scheduleTimer(&train);

        struct EngineerMessage mesg;
        int tid;
        Receive(&tid, (char *)&mesg, sizeof(mesg));

        // React to the outside world.
        if(tid == train.messaging.courier){
            recoverCourier(&train, tid);
        } else if(tid == train.timing.timer){
            int time = *((int *)&mesg);
            if(time >= train.timing.positionUpdate){
                timerPositionUpdate(&train, time);
            }
            train.timing.timerReady = true;
        } else {
            Reply(tid, 0, 0);
            switch(mesg.messageType){
                case SENSOR: {
                    sensorUpdate(&train, mesg.content.sensorTriggered.sensor);
                    break;
                }
                case GOTO: {
                    trainNavigate(&train, &mesg.content.destination);
                    break;
                }
                case QUIT:
                    assert(false);
                    break;
                default:
                    assert(false);
                    break;
            }
        }
    }

    Exit();
}

int engineerCreate(int trainID){
    int ret = CreateArgs(TASK_PRIORITY, engineer, 1, trainID);
    assert(ret > 0);
    return ret;
}

void engineerSend(int engineer_tid, struct TrackNode *dest, int mm) {
    struct EngineerMessage msg = {
        .messageType = GOTO,
        .content.destination = {
            .node = dest,
            .offset = mm
        }
    };
    Send(engineer_tid, (char *)&msg, sizeof(struct EngineerMessage), 0, 0);
}

void engineerSensorTriggered(int engineer_tid, Sensor sensor) {
    struct EngineerMessage msg = {
        .messageType = SENSOR,
        .content.sensorTriggered.sensor = sensor,
    };
    int len = Send(engineer_tid, (char *)&msg, sizeof(struct EngineerMessage), 0, 0);
    assert(len == 0);
}

void engineerQuit(int engineer_tid) {
    struct EngineerMessage msg = {
        .messageType = QUIT,
    };
    Send(engineer_tid, (char *)&msg, sizeof(struct EngineerMessage), 0, 0);
}
