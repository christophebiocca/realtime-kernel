#include <lib.h>
#include <stdbool.h>
#include <ts7200.h>

#include <user/kinematics.h>
#include <user/pathfinding.h>
#include <user/priorities.h>
#include <user/sensor.h>
#include <user/string.h>
#include <user/syscall.h>
#include <user/tio.h>
#include <user/track_data.h>
#include <user/vt100.h>

#include <user/clock.h>
#include <user/controller.h>
#include <user/engineer.h>
#include <user/log.h>
#include <debug.h>

// mm for how long the train is behind the pickup
#define TRAIN_TAIL_PICKUP_FRONT 190
#define TRAIN_PICKUP_LENGTH     50
#define TRAIN_TAIL_PICKUP_BACK  75

struct Train;

struct EngineerMessage {
    enum {
        GOTO,
        SENSOR,
        CIRCLE_MODE,
        RANDOM_MODE,
        DUMP_RESERVATIONS,
        QUIT,
        NUM_MESSAGE_TYPES
    } messageType;
    union {
        char padding_DO_NOT_USE[12];

        struct Position destination;
        struct {
            Sensor sensor;
        } sensorTriggered;

        int circle_mode;
    } content;
};

struct Messaging {
    int courier;
    bool courierReady;
    bool notifyPosition;
    bool notifyExpectation;
    bool notifyNeededReservations;
    bool notifyDoNotWantReservations;
};

struct Timing {
    // What the timer is doing
    int timer;
    int nextFire;
    bool timerReady;

    // What we need to be woken up for (timestamps)
    int positionUpdate;
    int replan;
};

struct TrackControl {
    // Our understanding of the track/position.
    TurnoutTable turnouts;
    struct Position position;
    int last_error;
    struct TrackNode *expectedSensor;
    struct TrackNode *secondarySensor;

    bool fullyReserved;

    // Only applicable when pathing.
    bool pathing;
    struct Position next_stop;
    struct Position goal;
    struct TrackNode *path[50];
    struct TrackNode **pathCurrent;
};

#define TRACK_RESERVATION_EDGES 50
struct TrackReservations {
    int needed_count;
    struct TrackEdge *needed[TRACK_RESERVATION_EDGES];

    int granted_count;
    struct TrackEdge *granted[TRACK_RESERVATION_EDGES];

    // according to thesaurus.com "do not want" is the antonym of "need"
    int donotwant_count;
    struct TrackEdge *donotwant[TRACK_RESERVATION_EDGES];
};

struct Train {
    int id;
    enum EngineerMode mode;
    struct Kinematics kinematics;
    struct Messaging messaging;
    struct Timing timing;
    struct TrackControl track;
    struct TrackReservations reservations;

#ifndef PRODUCTION
    struct Perf wholeLoop;
    struct Perf plan;
    struct Perf timerCallback;
    struct Perf updatePosition;
    struct Perf updateExpectation;
    struct Perf updateTurnouts;
    struct Perf calculateStop;
    struct Perf handleReversals;
    struct Perf findNextStop;
    struct Perf kinematicsFuckery;
    struct Perf tio;

    struct Perf reservationsNeeded;
    struct Perf reservationsGranted;
    struct Perf reservationsDoNotWant;
#endif
};

static inline void recoverCourier(struct Train *train, int sender){
    assert(sender == train->messaging.courier);
    (void)(sender);
    assert(!train->messaging.courierReady);
    train->messaging.courierReady = true;
}

static inline void courierUsed(struct Train *train){
    assert(train->messaging.courier);
    train->messaging.courierReady = false;
}

#define UPDATE_INTERVAL 5

static inline void reverse(struct Train *train){
    TIMER_START(train->tio);
    struct String s;
    sinit(&s);
    sputc(&s, 15);
    sputc(&s, train->id);
    tioPrint(&s);
    train->kinematics.orientation = (train->kinematics.orientation == FORWARD)
        ? BACKWARD : FORWARD;
    TIMER_WORST(train->tio);
}

static inline void setSpeed(struct Train *train, int speed){
    logC("SetSpeed");
    assert(0 <= train->id && train->id <= 80);
    assert(0 <= speed && speed <= 14);
    if(train->kinematics.target_speed == train->kinematics.ideal_speed[speed]){
        return;
    }
    train->kinematics.target_speed = train->kinematics.ideal_speed[speed];
    TIMER_START(train->kinematicsFuckery);
    computeAcceleration(&train->kinematics);
    TIMER_WORST(train->kinematicsFuckery);
    {
        struct String s;
        sinit(&s);
        sputstr(&s, "ts:");
        sputuint(&s, train->kinematics.target_speed,10);
        logS(&s);
    }
    TIMER_START(train->tio);
    struct String s;
    sinit(&s);
    sputc(&s, speed);
    sputc(&s, train->id);
    tioPrint(&s);
    TIMER_WORST(train->tio);
}

// FIXME: horrible O(n) performance
static inline int edgeInArray(struct TrackEdge **array,
        int size, struct TrackEdge *edge) {

    for (int i = 0; i < size; ++i) {
        if (array[i] == edge || array[i] == edge->reverse) {
            return true;
        }
    }

    return false;
}

static inline void updateNeededReservations(struct Train *train) {
    struct TrackReservations *r = &train->reservations;

    int back = (train->kinematics.orientation == FORWARD)
        ? TRAIN_TAIL_PICKUP_FRONT
        : TRAIN_TAIL_PICKUP_BACK;

    // entrances have no nodes before them
    if (train->track.position.node->type != NODE_ENTER &&
            train->track.position.offset < back) {

        struct TrackNode *back = train->track.position.node->reverse;

        int dir = 0;
        if (back->type == NODE_BRANCH) {
            dir = isTurnoutCurved(train->track.turnouts, back->num);
        } else if (back->type == NODE_BRANCH_CURVED) {
            dir = 1;
        }

        int already_needed = edgeInArray(
            r->needed,
            r->needed_count,
            &back->edge[dir]
        );

        if (!already_needed) {
            r->needed[r->needed_count++] = &back->edge[dir];
            assert(r->needed_count <= TRACK_RESERVATION_EDGES);
            int already_granted = edgeInArray(
                r->granted,
                r->granted_count,
                &back->edge[dir]
            );
            if(!already_granted){
                train->messaging.notifyNeededReservations = true;
                train->track.fullyReserved = false;
            }
        }
    }

    struct Position end;
    struct TrackNode *path[50];
    struct TrackEdge *edges[50];

    int len = alongTrack(
        train->track.turnouts,
        &train->track.position,
        train->kinematics.stop / 1000 + 200,
        &end,
        path,
        train->track.pathing ? train->track.pathCurrent : 0,
        edges,
        false
    );

    for (int i = 0; i < (len - 1); ++i) {
        int already_needed = edgeInArray(
            r->needed,
            r->needed_count,
            edges[i]
        );

        if (!already_needed) {
            r->needed[r->needed_count++] = edges[i];
            assert(r->needed_count <= TRACK_RESERVATION_EDGES);
            int already_granted = edgeInArray(
                r->granted,
                r->granted_count,
                edges[i]
            );
            if(!already_granted){
                train->messaging.notifyNeededReservations = true;
                train->track.fullyReserved = false;
            }
        }
    }
}

static inline void updateGrantedReservations(struct Train *train) {
    struct TrackReservations *r = &train->reservations;

    for (int i = 0; i < r->needed_count; ++i) {

        if (r->needed[i]->reserved != train->id) {
            continue;
        }

        struct TrackEdge *edge = r->needed[i];

        // shift everything to the left
        for(int j = i+1; j < r->needed_count; ++j){
            r->needed[j-1] = r->needed[j];
        }
        --r->needed_count;
        --i;

        if(r->needed_count == 0) {
            train->messaging.notifyNeededReservations = false;
            train->track.fullyReserved = true;
        }

        int edge_exists = edgeInArray(
            r->granted,
            r->granted_count,
            edge
        );
        if (!edge_exists) {
            r->granted[r->granted_count++] = edge;
            assert(r->granted_count <= TRACK_RESERVATION_EDGES);
        }
    }
}

static inline void updateDoNotWantReservations(struct Train *train) {
    struct TrackReservations *r = &train->reservations;

    for (int i = 0; i < r->granted_count; ++i) {

        int edge_exists = edgeInArray(
            r->needed,
            r->needed_count,
            r->granted[i]
        );

        if (!edge_exists) {
            // edge no longer needed, DO NOT WANT
            struct TrackEdge *edge = r->granted[i];

            for(int j = i + 1; j < r->granted_count; ++j){
                r->granted[j-1] = r->granted[j];
            }
            --r->granted_count;
            --i;

            r->donotwant[r->donotwant_count++] = edge;
            assert(r->donotwant_count <= TRACK_RESERVATION_EDGES);

            train->messaging.notifyDoNotWantReservations = true;
        }
    }
}

static inline void notifyNeededReservations(struct Train *train) {
    if (train->messaging.notifyNeededReservations &&
            train->messaging.courierReady) {

        struct TrackReservations *r = &train->reservations;
        struct TrackEdge *edges[5];

        int j;
        for (int i = j = 0; j < 5 && i < r->needed_count; ++i) {

            int already_granted = edgeInArray(
                r->granted,
                r->granted_count,
                r->needed[i]
            );

            if (!already_granted) {
                edges[j++] = r->needed[i];
            }
        }

        if (j > 0) {
            for (; j < 5; ++j) {
                edges[j] = NULLPTR;
            }

            controllerReserve(
                train->messaging.courier,
                train->id,
                edges[0],
                edges[1],
                edges[2],
                edges[3],
                edges[4]
            );
            courierUsed(train);
        }
    }
}

static inline void notifyDoNotWantReservations(struct Train *train) {
    if (train->messaging.notifyDoNotWantReservations &&
            train->messaging.courierReady) {

        struct TrackReservations *r = &train->reservations;
        struct TrackEdge *edges[5];

        int j;
        for (int i = j = 0; j < 5 && i < r->donotwant_count; ++i) {

            int still_needed = edgeInArray(
                r->needed,
                r->needed_count,
                r->donotwant[i]
            );

            if (!still_needed) {
                edges[j++] = r->donotwant[i];
                for(int k = i + 1; k < r->donotwant_count; ++k){
                    r->donotwant[k-1] = r->donotwant[k];
                }
                --r->donotwant_count;
            }
        }

        if (j > 0) {
            for (; j < 5; ++j) {
                edges[j] = NULLPTR;
            }

            controllerRelease(
                train->messaging.courier,
                train->id,
                edges[0],
                edges[1],
                edges[2],
                edges[3],
                edges[4]
            );
            courierUsed(train);
        }
    }
}

static inline void updateExpectation(struct Train *train) {
    // Find the next sensor along the path.
    struct Position end;
    struct TrackNode *path[50];
    int len = alongTrack(train->track.turnouts, &train->track.position,
        train->kinematics.stop/1000 + 100, &end, path,
        (train->track.pathing ? train->track.pathCurrent : 0), NULLPTR, false);
    assert(len <= 50);
    struct TrackNode *nextSensor = 0;

    // Skip the first node, since that's where we are right now.
    for(int i = 1; i < len; ++i){
        if(path[i]->type == NODE_SENSOR){
            if(!nextSensor){
                nextSensor = path[i];
                break;
            }
        }
    }

    if(nextSensor && nextSensor != train->track.expectedSensor &&
        nextSensor->edge[DIR_STRAIGHT].reserved == train->id &&
        nextSensor->reverse->edge[DIR_STRAIGHT].reserved == train->id){
        train->track.expectedSensor = nextSensor;
        train->messaging.notifyExpectation = true;

        // If we have a primary sensor, we should also seek the next sensor on the track.
        struct TrackNode *node = train->track.expectedSensor;

        do {
            int dir = 0;
            if(node->type == NODE_BRANCH &&
                    isTurnoutCurved(train->track.turnouts, node->num)) {
                dir = 1;
            }
            node = node->edge[dir].dest;
        } while (!(node->type & (NODE_SENSOR | NODE_EXIT)));

        if (node->type == NODE_SENSOR) {
            train->track.secondarySensor = node;
        } else {
            train->track.secondarySensor = 0;
        }
    }
}

static inline void notifyExpectation(struct Train *train){
    if(train->messaging.notifyExpectation && train->messaging.courierReady){
        {
            struct String s;
            sinit(&s);
            sputstr(&s, "Expect:");
            sputstr(&s, train->track.expectedSensor->name);
            if(train->track.secondarySensor){
                sputstr(&s, ",");
                sputstr(&s, train->track.secondarySensor->name);
            }
            logS(&s);
        }
        controllerSetExpectation(
            train->messaging.courier,
            train->id,
            SENSOR_ENCODE(
                train->track.expectedSensor->num / 16,
                train->track.expectedSensor->num % 16
            ),
            // FIXME: set alternate expectation
            train->track.secondarySensor ? SENSOR_ENCODE(
                train->track.secondarySensor->num / 16,
                train->track.secondarySensor->num % 16
            ) : SENSOR_INVALID,
            SENSOR_INVALID
        );
        courierUsed(train);
        train->messaging.notifyExpectation = false;
    }
}

static inline void adjustTargetSpeed(struct Train *train){
    if(!train->track.pathing){
        return;
    }
    bool stopping = train->kinematics.target_speed == 0;
    int dist = distance(train->track.turnouts,
        &train->track.position,
        &train->track.next_stop);
    int invdist = distance(train->track.turnouts,
        &train->track.next_stop,
        &train->track.position);
    int stop = train->kinematics.stop/1000;
    if(!stopping && ((stop > dist && dist != 0x7FFFFFFF) || !train->track.fullyReserved)){
        if(stop > dist && dist != 0x7FFFFFFF){
            logC("Stopping (over)");
        } else if(!train->track.fullyReserved){
            logC("Stopping (reserve)");
        }
        setSpeed(train,0);
        {
            struct String s;
            sinit(&s);
            sputstr(&s, "Dist: ");
            sputint(&s, dist, 10);
            sputstr(&s, " Stop: ");
            sputint(&s, train->kinematics.stop, 10);
            sputstr(&s, " Pos:");
            sputstr(&s, train->track.position.node->name);
            sputc(&s, '@');
            sputint(&s, train->track.position.offset, 10);
            logS(&s);
        }
        if(dist <= stop && train->track.next_stop.node == train->track.goal.node &&
            train->track.next_stop.offset == train->track.goal.offset){
            train->track.pathing = false;
        }
    } else if(stopping && (((invdist >= dist) && (stop <= dist)) ||
        (invdist >= 200 && dist == 0x7FFFFFFF)) && train->track.fullyReserved){
        setSpeed(train,14);
        train->timing.replan = 0x7FFFFFFF;
        {
            struct String s;
            sinit(&s);
            sputstr(&s, "Dist: ");
            sputint(&s, dist, 10);
            sputstr(&s, " Stop: ");
            sputint(&s, train->kinematics.stop, 10);
            sputstr(&s, " Pos:");
            sputstr(&s, train->track.position.node->name);
            sputc(&s, '@');
            sputint(&s, train->track.position.offset, 10);
            logS(&s);
        }
    }
    if(!train->track.fullyReserved && train->timing.replan == 0x7FFFFFFF){
        train->timing.replan = Time() + 800 + ((train->id - 35) * 50);
        {
            struct String s;
            sinit(&s);
            sputstr(&s, "Replan @");
            sputint(&s, train->timing.replan, 10);
            logS(&s);
        }
    }
    if(stopping && !train->kinematics.target_speed){
        if(invdist < dist){
            logC("Overshot");
        }
        if(stop > dist){
            logC("Slowing Down");
        }
        if(!train->track.fullyReserved){
            logC("Waiting on reservations");
        }
    }
}

static inline void updateTurnouts(struct Train *train){
    if(train->track.pathing){
        struct TrackNode **t = train->track.pathCurrent;
        struct TrackNode **end = alongPath(t, train->kinematics.stop/1000 + train->track.position.offset,
            train->track.next_stop.node, true);
        for(; t <= end && *t != train->track.next_stop.node; ++t){
            if((*t)->type == NODE_BRANCH){
                // Find the first time we hit this branch
                // How should it be set?
                // Make the branch match expectations
                if((*t)->edge[DIR_STRAIGHT].dest == t[1]){
                    turnoutStraight((*t)->num, &train->track.turnouts);
                } else if((*t)->edge[DIR_CURVED].dest == t[1]){
                    turnoutCurve((*t)->num, &train->track.turnouts);
                }
            }
        }
    }

    struct Position end;
    struct TrackNode *path[50];
    struct TrackNode **sweep = path;
    int len = alongTrack(train->track.turnouts, &train->track.position,
        train->kinematics.stop/1000, &end, path,
        (train->track.pathing ? train->track.pathCurrent : 0), NULLPTR, false);
    (void) len;
    assert(len <= 50);
    while(*sweep != end.node){
        if((*sweep)->type == NODE_MERGE && sweep != path){
            // No conflict the merge nodes.
            struct TrackNode *prev = (*(sweep-1));
            if((*sweep)->reverse->edge[DIR_STRAIGHT].reverse->src == prev){
                turnoutStraight((*sweep)->num, &train->track.turnouts);
            } else if((*sweep)->reverse->edge[DIR_CURVED].reverse->src == prev){
                turnoutCurve((*sweep)->num, &train->track.turnouts);
            }
        }
        sweep++;
    }
}

static inline void findNextStop(struct Train *train){
    struct TrackNode **next = nextReverse(train->track.pathCurrent, train->track.goal.node);
    train->track.next_stop.node = *next;
    if(*next == train->track.goal.node){
        train->track.next_stop.offset = train->track.goal.offset;
    } else if((*(next+1))->reverse == train->track.goal.node){
        train->track.next_stop.offset = -train->track.next_stop.offset;
    } else {
        train->track.next_stop.offset = 200 +
            ((train->kinematics.orientation == FORWARD) ?
            TRAIN_TAIL_PICKUP_FRONT : TRAIN_TAIL_PICKUP_BACK);
    }
    {
        struct String s;
        sinit(&s);
        sputstr(&s, "Next Stop:[");
        sputint(&s, next - train->track.path, 10);
        sputc(&s, ']');
        sputstr(&s, train->track.next_stop.node->name);
        sputc(&s, '@');
        sputint(&s, train->track.next_stop.offset, 10);
        logS(&s);
    }
}

static inline void updatePosition(struct Train *train, struct Position *pos);

static inline void handleReversals(struct Train *train){
    if(train->kinematics.target_speed == 0 && train->track.pathing){
        bool a = (*(train->track.pathCurrent) == train->track.next_stop.node);
        bool b = train->track.next_stop.node != train->track.goal.node;
        bool c = train->kinematics.acceleration == 0;
        if(a && b && c){
            logC("Reversing");
            // We need to turn the train around.
            reverse(train);
            train->track.pathCurrent++;
            {
                // Project, then reverse.
                struct Position pos;
                alongTrack(train->track.turnouts, &train->track.position,
                    TRAIN_PICKUP_LENGTH - (train->kinematics.orientation == FORWARD ? TRAIN_TAIL_PICKUP_BACK : TRAIN_TAIL_PICKUP_FRONT),
                    &pos, 0, 0, 0, true);
                train->track.position.node = pos.node->reverse;
                train->track.position.offset = -pos.offset;
                {
                    struct String s;
                    sinit(&s);
                    sputstr(&s, "NP:");
                    sputstr(&s, train->track.position.node->name);
                    sputc(&s, '@');
                    sputint(&s, train->track.position.offset, 10);
                    logS(&s);
                }
            }
            TIMER_START(train->findNextStop);
            findNextStop(train);
            TIMER_WORST(train->findNextStop);
            updatePosition(train, &train->track.position);
        }
    }
}

static inline void trainNavigate(struct Train *train, struct Position *dest);

static inline void updatePosition(struct Train *train, struct Position *pos){
    train->track.position.node = pos->node;
    train->track.position.offset = pos->offset;
    train->messaging.notifyPosition = true;

    if(train->track.pathing){
        bool found = false;
        for(int i = 0; i < 5 && train->track.pathCurrent[i-1] != train->track.goal.node; ++i){
            if(train->track.pathCurrent[i] == pos->node){
                train->track.pathCurrent += i;
                found = true;
                break;
            }
        }
        if(!found){
            logC("Messed up, recalc");
            trainNavigate(train, &train->track.goal);
        }
    }

    // do not fuck with the ordering of these
    TIMER_START(train->reservationsGranted);
    updateGrantedReservations(train);
    TIMER_WORST(train->reservationsGranted);

    TIMER_START(train->reservationsNeeded);
    updateNeededReservations(train);
    TIMER_WORST(train->reservationsNeeded);

    TIMER_START(train->reservationsDoNotWant);
    updateDoNotWantReservations(train);
    TIMER_WORST(train->reservationsDoNotWant);

    TIMER_START(train->updateExpectation);
    updateExpectation(train);
    TIMER_WORST(train->updateExpectation);
    TIMER_START(train->updateTurnouts);
    updateTurnouts(train);
    TIMER_WORST(train->updateTurnouts);
    TIMER_START(train->calculateStop);
    adjustTargetSpeed(train);
    TIMER_WORST(train->calculateStop);
    TIMER_START(train->handleReversals);
    handleReversals(train);
    TIMER_WORST(train->handleReversals);
}

static inline void sensorUpdate(struct Train *train, Sensor sensor){
    struct Position pos;
    pos.node = nodeForSensor(sensor);
    pos.offset = 0;

    struct Position oldpos;
    oldpos.node = train->track.position.node;
    oldpos.offset = 0;

    if (oldpos.node != NULLPTR) {
        int realdist = distance(train->track.turnouts, &oldpos, &pos);
        int sign = (train->track.position.offset > realdist) ? 1 : -1;
        int error = (train->track.position.offset - realdist) * sign;

        train->track.last_error = error;
    }

    int now = Time();
    tick(&train->kinematics, now);
    train->kinematics.distance = 0;

    TIMER_START(train->updatePosition);
    updatePosition(train, &pos);
    TIMER_WORST(train->updatePosition);

    train->timing.positionUpdate = now + UPDATE_INTERVAL;
}

static inline void timerPositionUpdate(struct Train *train, int time){
    Pass();
    tick(&train->kinematics, time);
    struct Position pos;
    struct TrackNode *path[50];
    int len = alongTrack(train->track.turnouts, &train->track.position,
        train->kinematics.distance/1000, &pos, path, 0, NULLPTR, false);
    assert(len >= 0 && len <= 50);
    // Always skip the start point.
    for(int i = 1; i < len; ++i){
        if(path[i]->type == NODE_SENSOR){
            // We can't possibly have reached here yet. We're still before this point.
            pos.node = path[i-1];
            pos.offset = 0;
            // For lack of anything better.
            pos.offset = train->kinematics.distance/1000 -
                distance(train->track.turnouts, &train->track.position, &pos);
            {
                struct Position next;
                next.node = path[i];
                next.offset = 0;
                int nextDist = distance(train->track.turnouts, &pos, &next);
                if(pos.offset >= nextDist){
                    pos.offset = nextDist - 1;
                }
            }
            break;
        }
    }
    TIMER_START(train->updatePosition);
    updatePosition(train, &pos);
    TIMER_WORST(train->updatePosition);
    train->kinematics.distance = 0;
    train->timing.positionUpdate = time + UPDATE_INTERVAL;
}

static inline void notifyPosition(struct Train *train){
    if(train->messaging.notifyPosition && train->messaging.courierReady){
        struct TrackNode *next_stop;
        int next_stop_offset;
        struct TrackNode *goal;

        if (train->track.pathing) {
            next_stop = train->track.next_stop.node;
            next_stop_offset = train->track.next_stop.offset;
            goal = train->track.goal.node;
        } else {
            next_stop = goal = NULLPTR;
            next_stop_offset = 0;
        }

        controllerUpdatePosition(
            train->messaging.courier,
            train->id,
            train->track.position.node,
            train->track.position.offset,
            train->track.last_error,
            next_stop,
            goal,
            train->mode
        );
        courierUsed(train);
        train->messaging.notifyPosition = false;
    }
}

static inline void scheduleTimer(struct Train *train){
    int closestEvent = train->timing.positionUpdate;
    if(closestEvent > train->timing.replan){
        closestEvent = train->timing.replan;
    }
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
        train->kinematics.stop/1000 + (train->kinematics.orientation == FORWARD) ?
        TRAIN_TAIL_PICKUP_BACK : TRAIN_TAIL_PICKUP_FRONT,
        &pathStart, train->track.path, 0, NULLPTR, (train->kinematics.stop == 0 &&
        train->track.position.node->type == NODE_BRANCH));
    // Now plan a path from there to here.
    planPath(nodes, train->id, pathStart.node, dest->node, train->track.path + (i-1));
    train->track.pathing = true;
    train->track.pathCurrent = train->track.path;
    train->track.goal.node = dest->node;
    train->track.goal.offset = dest->offset;
    TIMER_START(train->findNextStop);
    findNextStop(train);
    TIMER_WORST(train->findNextStop);

    for(struct TrackNode **i = train->track.pathCurrent;; i++){
        if(train->track.goal.node == *i){
            break;
        }
    }
}

void engineer(int trainID){
    {
        char name[5];
        name[0] = 'e';
        name[1] = 'n';
        name[2] = '0' + (trainID % 10);
        name[3] = '0' + (trainID / 10);
        name[4] = 0;

        logAssoc(name);
    }
    {
        struct String s;
        sinit(&s);
        sputuint(&s, MyTid(), 10);
        logS(&s);
    }

    /* SET UP */
    struct Train train;
    train.id = trainID;
    train.mode = ENGINEER_MODE_USER;
    train.track.pathing = false;

    TIMER_INIT(train.wholeLoop);
    TIMER_INIT(train.plan);
    TIMER_INIT(train.timerCallback);
    TIMER_INIT(train.updatePosition);
    TIMER_INIT(train.updateExpectation);
    TIMER_INIT(train.updateTurnouts);
    TIMER_INIT(train.calculateStop);
    TIMER_INIT(train.handleReversals);
    TIMER_INIT(train.findNextStop);
    TIMER_INIT(train.kinematicsFuckery);
    TIMER_INIT(train.tio);
    TIMER_INIT(train.reservationsNeeded);
    TIMER_INIT(train.reservationsGranted);
    TIMER_INIT(train.reservationsDoNotWant);

    // Spawn our helpers and claim them.
    {
        train.timing.timer = CreateArgs(TASK_PRIORITY, clockWaiter, 1, MyTid());
        assert(train.timing.timer > 0);
        int tid;
        int len = Receive(&tid, (char *)&train.kinematics.time, sizeof(int));
        (void)(len);
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
        (void)(len);
        assert(tid == train.messaging.courier);
        assert(len == 0);
        train.messaging.courierReady = true;
    }

    kinematicsInit(&train.kinematics, train.id);
    train.kinematics.time = Time();

    train.reservations.needed_count = 0;
    train.reservations.granted_count = 0;
    train.reservations.donotwant_count = 0;

    train.track.expectedSensor = 0;
    train.track.secondarySensor = 0;
    train.messaging.notifyExpectation = false;

    train.timing.replan = 0x7FFFFFFF;

    setSpeed(&train, 14);

    // Go forth and hit a sensor.
    {
        // Wait for a sensor message update.
        int tid;
        struct EngineerMessage msg;

        int len = Receive(&tid, (char *)&msg, sizeof(struct EngineerMessage));
        (void)(len);
        assert(len == sizeof(struct EngineerMessage));
        assert(msg.messageType == SENSOR);

        int ret = Reply(tid, 0, 0);
        (void)(ret);
        assert(ret == 0);

        // TODO: Get the actual turnout state from someone.
        train.track.turnouts = (1<<23)-1;
        train.track.last_error = 0;
        train.track.position.node = NULLPTR;

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

    setSpeed(&train, 0);

    int last_circle = 0;
    char *circles[][3] = {
        {
            "E14",
            "D6",
            "D1"
        }, {
            "C10",
            "E2",
            "A4"
        }
    };

    bool quit = false;
    while(!quit || !train.messaging.courierReady || !train.timing.timerReady) {
        if(!quit){
            // Act on the outside world.
            notifyPosition(&train);
            notifyExpectation(&train);
            notifyNeededReservations(&train);
            notifyDoNotWantReservations(&train);

            if (!train.track.pathing) {
                struct TrackNode *dest = NULLPTR;
                char *log_type = NULLPTR;

                switch(train.mode) {
                    case ENGINEER_MODE_CIRCLE1:
                    case ENGINEER_MODE_CIRCLE2: {
                        dest = lookupTrackNode(
                            hashtbl,
                            circles[
                                (train.mode == ENGINEER_MODE_CIRCLE1) ? 0 : 1
                            ][last_circle++]
                        );
                        last_circle %= 3;

                        assert(dest != NULLPTR);
                        log_type = "Circle";

                        break;
                    }

                    case ENGINEER_MODE_RANDOM: {
                        dest = randomTrackNode(nodes);
                        log_type = "Random";
                        break;
                    }

                    default:
                        break;
                }

                if (dest != NULLPTR) {
                    struct Position pos;
                    pos.node = dest;
                    pos.offset = 0;

                    TIMER_START(train.plan);
                    trainNavigate(&train, &pos);
                    TIMER_WORST(train.plan);

                    struct String s;
                    sinit(&s);
                    sputstr(&s, CYAN);
                    sputstr(&s, log_type);
                    sputstr(&s, ": ");
                    sputstr(&s, dest->name);
                    sputstr(&s, RESET);
                    logS(&s);
                }
            }

            scheduleTimer(&train);
        }

        struct EngineerMessage mesg;
        int tid;

        TIMER_WORST(train.wholeLoop);
        Receive(&tid, (char *)&mesg, sizeof(mesg));
        TIMER_START(train.wholeLoop);

        // React to the outside world.
        if(tid == train.messaging.courier){
            recoverCourier(&train, tid);
        } else if(tid == train.timing.timer){
            int time = *((int *)&mesg);
            if(time >= train.timing.positionUpdate){
                TIMER_START(train.timerCallback);
                timerPositionUpdate(&train, time);
                TIMER_WORST(train.timerCallback);
            }
            if(time >= train.timing.replan){
                if(train.track.pathing){
                    TIMER_START(train.plan);
                    logC("Replanning");
                    trainNavigate(&train, &train.track.goal);
                    TIMER_WORST(train.plan);
                }
                train.timing.replan = 0x7FFFFFFF;
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
                    TIMER_START(train.plan);
                    train.mode = ENGINEER_MODE_USER;
                    trainNavigate(&train, &mesg.content.destination);
                    TIMER_WORST(train.plan);
                    break;
                }

                case CIRCLE_MODE: {
                    switch (mesg.content.circle_mode) {
                        case 0:
                        case 1:
                            train.mode = ENGINEER_MODE_CIRCLE1;
                            break;

                        case 2:
                            train.mode = ENGINEER_MODE_CIRCLE2;
                            break;

                        default:
                            logC("unknown circle mode");
                    }
                    break;
                }

                case RANDOM_MODE: {
                    train.mode = ENGINEER_MODE_RANDOM;
                    break;
                }

                case DUMP_RESERVATIONS: {
                    struct String s;
                    struct TrackReservations *r = &train.reservations;

                    sinit(&s);
                    sputstr(&s, "Needed (");
                    sputint(&s, r->needed_count, 10);
                    sputstr(&s, ")");
                    logS(&s);

                    for (int i = 0; i < r->needed_count; ++i) {
                        sinit(&s);

                        sputstr(&s, "    ");
                        sputstr(&s, r->needed[i]->src->name);
                        sputstr(&s, " to ");
                        sputstr(&s, r->needed[i]->dest->name);
                        logS(&s);
                    }

                    sinit(&s);
                    sputstr(&s, "Granted (");
                    sputint(&s, r->granted_count, 10);
                    sputstr(&s, ")");
                    logS(&s);

                    for (int i = 0; i < r->granted_count; ++i) {
                        sinit(&s);

                        sputstr(&s, "    ");
                        sputstr(&s, r->granted[i]->src->name);
                        sputstr(&s, " to ");
                        sputstr(&s, r->granted[i]->dest->name);
                        logS(&s);
                    }

                    sinit(&s);
                    sputstr(&s, "Do Not Want (");
                    sputint(&s, r->donotwant_count, 10);
                    sputstr(&s, ")");
                    logS(&s);

                    for (int i = 0; i < r->donotwant_count; ++i) {
                        sinit(&s);

                        sputstr(&s, "    ");
                        sputstr(&s, r->donotwant[i]->src->name);
                        sputstr(&s, " to ");
                        sputstr(&s, r->donotwant[i]->dest->name);
                        logS(&s);
                    }

                    break;
                }

                case QUIT:
                    // Dump all the timings to the log.
                    TIMER_PRINT(train.wholeLoop);
                    TIMER_PRINT(train.timerCallback);
                    TIMER_PRINT(train.plan);
                    TIMER_PRINT(train.updatePosition);
                    TIMER_PRINT(train.updateExpectation);
                    TIMER_PRINT(train.updateTurnouts);
                    TIMER_PRINT(train.calculateStop);
                    TIMER_PRINT(train.handleReversals);
                    TIMER_PRINT(train.findNextStop);
                    TIMER_PRINT(train.kinematicsFuckery);
                    TIMER_PRINT(train.tio);
                    TIMER_PRINT(train.reservationsNeeded);
                    TIMER_PRINT(train.reservationsGranted);
                    TIMER_PRINT(train.reservationsDoNotWant);
                    quit = true;
                    break;
                default:
                    assert(false);
                    break;
            }
        }
    }

    setSpeed(&train, 0);
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

void engineerCircle(int engineer_tid, int circle_mode) {
    struct EngineerMessage msg = {
        .messageType = CIRCLE_MODE,
        .content.circle_mode = circle_mode,
    };
    Send(engineer_tid, (char *)&msg, sizeof(struct EngineerMessage), 0, 0);
}

void engineerRandom(int engineer_tid) {
    struct EngineerMessage msg = {
        .messageType = RANDOM_MODE
    };
    Send(engineer_tid, (char *)&msg, sizeof(struct EngineerMessage), 0, 0);
}

void engineerDumpReservations(int engineer_tid) {
    struct EngineerMessage msg = {
        .messageType = DUMP_RESERVATIONS,
    };
    Send(engineer_tid, (char *)&msg, sizeof(struct EngineerMessage), 0, 0);
}

void engineerSensorTriggered(int engineer_tid, Sensor sensor) {
    struct EngineerMessage msg = {
        .messageType = SENSOR,
        .content.sensorTriggered.sensor = sensor,
    };
    int len = Send(engineer_tid, (char *)&msg, sizeof(struct EngineerMessage), 0, 0);
    (void)(len);
    assert(len == 0);
}

void engineerQuit(int engineer_tid) {
    struct EngineerMessage msg = {
        .messageType = QUIT,
    };
    Send(engineer_tid, (char *)&msg, sizeof(struct EngineerMessage), 0, 0);
}
