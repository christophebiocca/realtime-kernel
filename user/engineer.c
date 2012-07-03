#include <lib.h>
#include <stdbool.h>
#include <ts7200.h>

#include <user/heap.h>
#include <user/mio.h>
#include <user/priorities.h>
#include <user/string.h>
#include <user/syscall.h>
#include <user/tio.h>
#include <user/track_data.h>

#include <user/clock.h>
#include <user/controller.h>
#include <user/engineer.h>
#include <user/log.h>

struct PathNode {
    struct TrackNode *node;
    int cost;
    int total;
    struct PathNode *from;
};

inline static int getTotal(struct PathNode *node){
    return node->total;
}

HEAP(PathHeap, struct PathNode*, getTotal, 9, <)

int heuristic(struct TrackNode *here, struct TrackNode *goal){
    // Gotta define something here. Use the dumbest one for now.
    (void) here; (void) goal;
    return 0;
}

int planPath(struct TrackNode *list, struct TrackNode *start, struct TrackNode *goal, struct TrackNode **output){
    struct PathNode nodes[TRACK_MAX];
    for(int i = 0; i < TRACK_MAX; ++i){
        nodes[i].node = &list[i];
        nodes[i].cost = 0x7FFFFFFF;
        nodes[i].total = 0x7FFFFFFF;
        nodes[i].from = 0;
    }
    struct PathHeap heap;
    heap.count = 0;
    // Goal Node
    // Start Node
    {
        nodes[start->idx].cost = 0;
        nodes[start->idx].total = heuristic(start, goal);
        nodes[start->idx].from = 0;
    }
    // Start Node, reverse direction
    // {
    //     nodes[start->reverse->idx].cost = 0;
    //     nodes[start->reverse->idx].total = heuristic(start->reverse, goal);
    //     nodes[start->reverse->idx].from = &nodes[start->idx];
    // }
    PathHeapPush(&heap, &nodes[start->idx]);
    //PathHeapPush(&heap, &nodes[start->reverse->idx]);
    struct PathNode *goalNode = &nodes[goal->idx];
    while(heap.count && heap.heap[1]->total <= goalNode->cost){
        struct PathNode *next = PathHeapPop(&heap);
        struct TrackNode *seq[3];
        int seqCost[3];
        int seqCount = 0;
        if(/*next->node->type == NODE_BRANCH || next->node->type == NODE_MERGE || */next->node->reverse == goal){
            seq[seqCount] = next->node->reverse;
            seqCost[seqCount++] = 0;
        }
        if(next->node->type != NODE_EXIT){
            seq[seqCount] = next->node->edge[0].dest;
            seqCost[seqCount++] = next->node->edge[0].dist;
        }
        if(next->node->type == NODE_BRANCH){
            seq[seqCount] = next->node->edge[1].dest;
            seqCost[seqCount++] = next->node->edge[1].dist;
        }
        for(int i = 0; i < seqCount; ++i){
            struct TrackNode *node = seq[i];
            int cost = seqCost[i];
            if(next->cost + cost < nodes[node->idx].cost){
                nodes[node->idx].cost = next->cost + cost;
                nodes[node->idx].total = nodes[node->idx].cost + heuristic(nodes[node->idx].node, goal);
                nodes[node->idx].from = next;
                if(nodes[node->idx].total < goalNode->total && node != goal){
                    PathHeapPush(&heap,&nodes[node->idx]);
                }
            }
        }
    }
    // At this point, the current goal node has the shortest path made out of back-pointers.
    int solutionCount = 0;
    for(struct PathNode *n = goalNode; n != 0; n=n->from){
        ++solutionCount;
    }
    int i = solutionCount;
    for(struct PathNode *n = goalNode; n != 0; n=n->from){
        output[--i] = n->node;
    }
    assert(i == 0);
    return solutionCount;
}

struct EngineerMessage {
    enum {
        GOTO,
        SENSOR,
        QUIT
    } messageType;
    union {
        struct {
            struct TrackNode *dest;
            int mm;
        } destination;
        struct {
            int sensor;
            int number;
        } sensor;
    } content;
    int dummy;
};

static inline struct TrackNode *find(int sensor, int number){
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

#define UPDATE_INTERVAL 10

static inline void setSpeed(int trainID, int speed){
    assert(0 <= trainID && trainID <= 80);
    assert(0 <= speed && speed <= 14);
    struct String s;
    sinit(&s);
    sputc(&s, speed);
    sputc(&s, trainID);
    tioPrint(&s);
}

static inline int alongPath(struct TrackNode **path, int dist, int last){
    int i = 0;
    while(dist > 0 && i < last){
        struct TrackNode *next = path[i+1];
        if(path[i]->edge[0].dest == next){
            dist -= path[i]->edge[0].dist;
        } else if(path[i]->edge[1].dest == next){
            dist -= path[i]->edge[1].dist;
        } else {
            assert(path[i]->reverse == next);
        }
        ++i;
    }
    return i;
}

// computes the length of a path in mm
static inline int distance(struct TrackNode *from, struct TrackNode *to) {
    // only allow for really small path's
    struct TrackNode *path[5];
    // subtract 1 because we access the next element in the loop
    int len = planPath(nodes, from, to, path) - 1;

    struct TrackNode *next;
    int dist = 0;

    for (int i = 0; i < len; ++i) {
        next = path[i + 1];

        if (path[i]->edge[0].dest == next) {
            dist += path[i]->edge[0].dist;
        } else if (path[i]->edge[1].dest == next) {
            dist += path[i]->edge[1].dist;
        } else {
            assert(path[i]->reverse == next);
        }
    }

    return dist;
}

// number of readings to throw away (while the train accelerates to the desired
// speed)
#define THROWAWAY_FIRST 15
#define THROWAWAY_OTHER 3

// number of readings to average for speed
#define NUM_READINGS    8

static inline void computeSpeeds(const int train_id,
        int *speeds, const int min, const int max) {

    struct EngineerMessage msg;
    int tid, len, ret;

    for (int i = min; i <= max; ++i) {
        setSpeed(train_id, i);

        // throw away a lot of initial readings for first speed since train
        // needs to accelerate from stopped state.
        int throwaway = (i == min) ? THROWAWAY_FIRST : THROWAWAY_OTHER;

        for (int j = 0; j < throwaway; ++j) {
            // throw away some readings while we get up to speed
            len = Receive(&tid, (char *) &msg, sizeof(struct EngineerMessage));
            assert(len == sizeof(struct EngineerMessage));
            assert(msg.messageType == SENSOR);

            ret = Reply(tid, 0, 0);
            assert(ret == 0);
        }

        struct TrackNode *last_position = NULLPTR;
        unsigned int distance_travelled; // in mm

        for (int j = 0; j < NUM_READINGS; ++j) {
            len = Receive(&tid, (char *) &msg, sizeof(struct EngineerMessage));
            assert(len == sizeof(struct EngineerMessage));
            assert(msg.messageType == SENSOR);

            ret = Reply(tid, 0, 0);
            assert(ret == 0);

            struct TrackNode *position = find(
                msg.content.sensor.sensor,
                msg.content.sensor.number
            );
            assert(position);

            if (last_position == NULLPTR) {
                distance_travelled = 0;
                *(TIMER4_CRTL) = TIMER4_ENABLE;
            } else {
                distance_travelled += distance(last_position, position);
            }

            last_position = position;
        }

        unsigned int time_taken = *(TIMER4_VAL);
        *(TIMER4_CRTL) = 0;

        // in mm / ms
        float time_in_cs = time_taken / 9083.0;
        float speed = (1000 * distance_travelled) / time_in_cs;
        speeds[i] = (int) speed;

        struct String s;
        sinit(&s);
        sputstr(&s, "Speed at ");
        sputint(&s, i, 10);
        sputstr(&s, " is ");
        sputuint(&s, speeds[i], 10);
        logS(&s);
    }
}


// FIXME: consider using the floating point values?
#define FORWARD_STOPPING_COEFFICIENT        (149)
#define BACKWARD_STOPPING_COEFFICIENT       (176)

// FIXME: this is the negation of the average of the computed deceleration
// constant... this is probably a very bad approximation!
#define FORWARD_ACCELERATION_COEFFICIENT    (15)
#define BACKWARD_ACCELERATION_COEFFICIENT   (13)

// if (abs(computed_speed - expected_speed) < threshold) acceleration = 0
#define SPEED_THRESHOLD                     (100)
void engineer(int trainID){
    logAssoc("en");
    {
        struct String s;
        sinit(&s);
        sputuint(&s, MyTid(), 10);
        logS(&s);
    }

    /* SET UP */

    // Spawn our helpers and claim them.
    bool timerReady = false;
    int timer;
    int time;
    {
        timer = CreateArgs(TASK_PRIORITY, clockWaiter, 1, MyTid());
        assert(timer > 0);
        int tid;
        int len = Receive(&tid, (char *)&time, sizeof(int));
        assert(tid == timer);
        assert(len == sizeof(int));
        assert(time >= 0);
        timerReady = true;
    }

    bool courierReady = false;
    int courier;
    {
        courier = controllerCourier(MyTid());
        assert(courier > 0);
        int tid;
        int len = Receive(&tid, 0, 0);
        assert(tid == courier);
        assert(len == 0);
        courierReady = true;
    }

    struct TrackNode *path[50];

    // speeds in um / cs
    int ideal_speed[15];
    memset16(ideal_speed, 0, sizeof(ideal_speed));
    computeSpeeds(trainID, ideal_speed, 8, 14);

    // computeSpeeds leaves the train running at speed 14
    int target_speed = ideal_speed[14];
    int computed_speed = ideal_speed[14];
    int acceleration = 0;

    enum {
        FORWARD,
        BACKWARD
    } orientation;

    // Go forth and hit a sensor.
    struct TrackNode *position;
    int posTime;
    bool needPosUpdate;
    bool needExpect;
    {
        // Wait for a sensor message update.
        int tid;
        struct EngineerMessage msg;

        int len = Receive(&tid, (char *)&msg, sizeof(struct EngineerMessage));
        assert(len == sizeof(struct EngineerMessage));
        assert(msg.messageType == SENSOR);

        int ret = Reply(tid, 0, 0);
        assert(ret == 0);

        position = find(msg.content.sensor.sensor, msg.content.sensor.number);
        time = posTime = Time();
        needPosUpdate = true;
        needExpect = true;

        int flag = (msg.content.sensor.sensor << 4) | msg.content.sensor.number;
        switch (flag) {
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
                orientation = FORWARD;
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
                orientation = BACKWARD;
                logC("Pickup at back");
                break;

            default:
                logC("Don't know orientation, assuming front");
                orientation = FORWARD;
                break;
        }
    }

    bool quitting = false;
    bool courierQuit = false;
    bool timerQuit = false;
    int current = 0;
    int set = 0;
    int toSet = 0;
    int target = 0;
    int dist = 0;

    while (!quitting || !courierQuit || !timerQuit) {
        if(target && (target == toSet) && target_speed){
            target_speed = 0;
            setSpeed(trainID, 0);
        } else if(target && set <= toSet && courierReady){
            do {
                set++;
            } while (path[set]->type != NODE_BRANCH && set <= toSet);
            if(set <= toSet){
                if(path[set]->edge[0].dest == path[set+1]){
                    logC("Straight");
                    controllerTurnoutStraight(courier, path[set]->num);
                } else if(path[set]->edge[1].dest == path[set+1]) {
                    logC("Curve");
                    controllerTurnoutCurve(courier, path[set]->num);
                } else {
                    logC("Reverse");
                    {
                        struct String s;
                        sinit(&s);
                        sputstr(&s, path[set]->name);
                        sputstr(&s, " -> ");
                        sputstr(&s, path[set+1]->name);
                        logS(&s);
                    }
                    assert(path[set]->reverse == path[set+1]);
                }
                courierReady = false;
            }
        } else if(needPosUpdate && courierReady) {
            int sign = (target_speed < computed_speed) ? -1 : 1;
            int diffspeed = (target_speed - computed_speed) * sign;

            if (diffspeed < SPEED_THRESHOLD) {
                acceleration = 0;
            } else {
                int coeff = (orientation == FORWARD)
                    ? FORWARD_ACCELERATION_COEFFICIENT
                    : BACKWARD_ACCELERATION_COEFFICIENT;

                acceleration = sign * coeff;
            }

            int diff = time - posTime;
            // in um
            dist = computed_speed * diff + (acceleration * diff * diff) / 2;

            controllerUpdatePosition(
                courier,
                trainID,
                position,
                dist / 1000
            );
            needPosUpdate = false;
            courierReady = false;
        } else if(needExpect && courierReady) {
            struct TrackNode *n;

            if(target){
                int i = current;
                do {
                    ++i;
                } while (path[i]->type != NODE_SENSOR);
                n = path[i];
            } else {
                n = position;
                do {
                    if(n->type == NODE_BRANCH){
                        n = n->edge[1].dest;
                    } else {
                        n = n->edge[0].dest;
                    }
                } while(n->type != NODE_SENSOR);
            }
            {
                struct String s;
                sinit(&s);
                sputstr(&s, "Expect ");
                sputstr(&s, n->name);
                logS(&s);
            }
            controllerSetExpectation(courier, trainID, n->num / 16, n->num % 16);
            courierReady = false;
            needExpect = false;
        } else if (!needPosUpdate && timerReady){
            int nextTimer = time + 25;
            int ret = Reply(timer, (char *)&nextTimer, sizeof(int));
            assert(ret == 0);
            timerReady = false;
        } else {
            int tid;
            struct EngineerMessage msg;
            int len = Receive(&tid, (char *)&msg, sizeof(msg));

            if(tid == courier) {
                assert(len == 0);

                if (!quitting) {
                    courierReady = true;
                } else {
                    Reply(courier, (char *) 0, 0);
                    courierQuit = true;
                    courierReady = false;
                }
            } else if(tid == timer){
                assert(len == sizeof(int));

                if (!quitting) {
                    timerReady = true;
                    time = *((int*)&msg);
                    needPosUpdate = true;
                } else {
                    int next = -1;
                    Reply(timer, (char *) &next, sizeof(int));
                    timerQuit = true;
                    timerReady = false;
                }
            } else {
                assert(len == sizeof(struct EngineerMessage));
                {
                    int ret = Reply(tid, 0, 0);
                    assert(ret == 0);
                }

                switch (msg.messageType) {
                    case SENSOR: {
                        time = Time();
                        struct TrackNode *new_position =
                            find(msg.content.sensor.sensor, msg.content.sensor.number);
                        int travelleddist = distance(position, new_position);

                        computed_speed = (travelleddist  * 1000) / (time - posTime);

                        posTime = time;
                        position = new_position;

                        if (target) {
                            while(path[current] != position) ++current;
                        }
                        needPosUpdate = true;
                        needExpect = true;

                        break;
                    }

                    case GOTO: {
                        int len = planPath(nodes, position, msg.content.destination.dest, path);
                        set = current = 0;
                        target = (len - 1);
                        {
                            struct String s;
                            sinit(&s);
                            for(int i = 0; i < len; i++){
                                sputstr(&s, path[i]->name);
                                sputc(&s, ' ');
                                if(!((i+1) % 10)){
                                    logS(&s);
                                    sinit(&s);
                                }
                            }
                            logS(&s);
                        }
                        break;
                    }

                    case QUIT:
                        setSpeed(trainID, 0);
                        // FIXME: still set expectations for what we expect to
                        // trigger, and update to final resting position
                        quitting = true;
                        break;

                    default:
                        assert(0);
                }
                
                if (target) {
                    int stop = (orientation == FORWARD)
                        ? FORWARD_STOPPING_COEFFICIENT
                        : BACKWARD_STOPPING_COEFFICIENT;

                    toSet = current + alongPath(path+current, (dist + computed_speed * stop)/1000, target-current);
                    {
                        struct String s;
                        sinit(&s);
                        sputstr(&s, "c:");
                        sputstr(&s, path[current]->name);
                        sputstr(&s, "s:");
                        sputstr(&s, path[set]->name);
                        sputstr(&s, "ts:");
                        sputstr(&s, path[toSet]->name);
                        sputstr(&s, "t:");
                        sputstr(&s, path[target]->name);
                        logS(&s);
                    }
                }
            }
        }
    }

    Exit();
}

int engineerCreate(int trainID){
    int ret = CreateArgs(TASK_PRIORITY+2, engineer, 1, trainID);
    assert(ret > 0);
    return ret;
}

void planRoute(char *src, char *dest){
    char srcName[5];
    char destName[5];

    for(int i = 0; i < 5; ++i){
        {
char c = src[i];
            if(0x61 <= c && c <= 0x7A) c &= ~0x20;
            srcName[i] = c;
        }
        {
            char c = dest[i];
            if(0x61 <= c && c <= 0x7A) c &= ~0x20;
            destName[i] = c;
        }
    }

    {
        struct String s;
        sinit(&s);
        sputstr(&s,srcName);
        sputstr(&s," to ");
        sputstr(&s,destName);
        sputstr(&s,"\r\n");
        mioPrint(&s);
    }

    struct TrackNode *srcNode = lookupTrackNode(hashtbl, srcName);
    struct TrackNode *destNode = lookupTrackNode(hashtbl, destName);

    assert(srcNode);
    assert(destNode);

    struct TrackNode *route[50];
    int count = planPath(nodes, srcNode, destNode, route);
    for(int i = 0; i < count; ++i){
        struct String s;
        sinit(&s);
        sputstr(&s,route[i]->name);
        sputstr(&s,", ");
        mioPrint(&s);
    }
    {
        struct String s;
        sinit(&s);
        sputstr(&s,"\r\n");
        mioPrint(&s);
    }
}

void engineerSend(int engineer_tid, struct TrackNode *dest, int mm) {
    struct EngineerMessage msg = {
        .messageType = GOTO,
        .content.destination = {
            .dest = dest,
            .mm = mm
        }
    };
    Send(engineer_tid, (char *)&msg, sizeof(struct EngineerMessage), 0, 0);
}

void engineerSensorTriggered(int engineer_tid, int sensor, int number) {
    struct EngineerMessage msg = {
        .messageType = SENSOR,
        .content.sensor = {
            .sensor = sensor,
            .number = number
        }
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
