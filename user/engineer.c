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

HEAP(PathHeap, struct PathNode*, getTotal, 16, <)

int heuristic(struct TrackNode *here, struct TrackNode *goal){
    // Gotta define something here. Use the dumbest one for now.
    (void) here; (void) goal;
    return 0;
}

int planPath(struct TrackNode *start, struct TrackNode *goal, struct TrackNode **output){
    struct PathNode nodes[TRACK_MAX * 2];
    struct PathHeap heap;
    heap.count = 0;
    // Placeholder goal node.
    nodes[0].node = goal;
    nodes[0].cost = 0x7FFFFFFF;
    nodes[0].total = 0x7FFFFFFF;
    nodes[0].from = 0;
    // Start Node
    nodes[1].node = start;
    nodes[1].cost = 0;
    nodes[1].total = heuristic(start, goal);
    nodes[1].from = 0;
    int count = 2;
    PathHeapPush(&heap, &nodes[1]);
    struct PathNode *goalNode = &nodes[0];
    while(heap.heap[1]->total <= goalNode->cost){
        struct PathNode *next = PathHeapPop(&heap);
        {
            // The reverse, considered to have no cost.
            nodes[count].node = next->node->reverse;
            nodes[count].cost = next->cost + 1000;
            nodes[count].total = nodes[count].cost + heuristic(nodes[count].node, goal);
            nodes[count].from = next;
            if(nodes[count].node == goal && nodes[count].cost < goalNode->cost){
                goalNode = &nodes[count];
            } else {
                PathHeapPush(&heap,&nodes[count]);
            }
            ++count;
        }
        if(next->node->type != NODE_EXIT){
            // The straight/ahead segment, almost always exits.
            nodes[count].node = next->node->edge[0].dest;
            nodes[count].cost = next->cost + next->node->edge[0].dist;
            nodes[count].total = nodes[count].cost + heuristic(nodes[count].node, goal);
            nodes[count].from = next;
            if(nodes[count].node == goal && nodes[count].cost < goalNode->cost){
                goalNode = &nodes[count];
            } else {
                PathHeapPush(&heap,&nodes[count]);
            }
            ++count;
        }
        if(next->node->type == NODE_BRANCH){
            // The curved branch segment, only available on branches.
            nodes[count].node = next->node->edge[1].dest;
            nodes[count].cost = next->cost + next->node->edge[1].dist;
            nodes[count].total = nodes[count].cost + heuristic(nodes[count].node, goal);
            nodes[count].from = next;
            if(nodes[count].node == goal && nodes[count].cost < goalNode->cost){
                goalNode = &nodes[count];
            } else {
                PathHeapPush(&heap,&nodes[count]);
            }
            ++count;
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

void engineer(int trainID){
    (void) trainID;
}

static struct TrackHashNode hashtbl[MAX_HASHNODES];
static struct TrackNode trackNodes[TRACK_MAX];

void initEngineer(int trainID){
    (void) trainID;
    initTrackA(trackNodes, hashtbl);
}

int engineerCreate(int trainID){
    return CreateArgs(TASK_PRIORITY, engineer, 1, trainID);
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

    struct TrackNode *srcNode = 0;
    for(int i = 0; i < TRACK_MAX; ++i){
        if(!strcmp(trackNodes[i].name, srcName)){
            srcNode = &trackNodes[i];
            break;
        }
    }

    struct TrackNode *destNode = 0;
    for(int i = 0; i < TRACK_MAX; ++i){
        if(!strcmp(trackNodes[i].name, destName)){
            destNode = &trackNodes[i];
            break;
        }
    }

    assert(srcNode);
    assert(destNode);

    struct TrackNode *route[50];
    int count = planPath(srcNode, destNode, route);
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
