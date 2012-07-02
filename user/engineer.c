#include <user/engineer.h>
#include <user/mio.h>
#include <user/tio.h>
#include <user/string.h>
#include <user/clock.h>
#include <user/priorities.h>
#include <user/syscall.h>
#include <stdbool.h>
#include <user/heap.h>
#include <user/track_data.h>
#include <user/controller.h>
#include <user/log.h>
#include <lib.h>

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
    {
        nodes[start->reverse->idx].cost = 0;
        nodes[start->reverse->idx].total = heuristic(start->reverse, goal);
        nodes[start->reverse->idx].from = &nodes[start->idx];
    }
    PathHeapPush(&heap, &nodes[start->idx]);
    PathHeapPush(&heap, &nodes[start->reverse->idx]);
    struct PathNode *goalNode = &nodes[goal->idx];
    while(heap.count && heap.heap[1]->total <= goalNode->cost){
        struct PathNode *next = PathHeapPop(&heap);
        struct TrackNode *seq[3];
        int seqCost[3];
        int seqCount = 0;
        if(next->node->type == NODE_BRANCH || next->node->type == NODE_MERGE || next->node->reverse == goal){
            seq[seqCount] = next->node->reverse;
            seqCost[seqCount++] = 1000;
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
                    {
                        struct String s;
                        sinit(&s);
                        sputstr(&s, "Push ");
                        sputstr(&s, node->name);
                        sputc(&s, ' ');
                        sputuint(&s, nodes[node->idx].total,10);
                        sputstr(&s, "\r\n");
                        mioPrint(&s);
                    }
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
        SENSOR
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

void engineer(int trainID){
    logAssoc("en");
    (void) trainID;
}

int engineerCreate(int trainID){
    return CreateArgs(TASK_PRIORITY+2, engineer, 1, trainID);
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

// FIXME: stubs
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
    Send(engineer_tid, (char *)&msg, sizeof(struct EngineerMessage), 0, 0);
}

void engineerQuit(int engineer_tid) {
    (void) engineer_tid;
}
