#include <lib.h>

#include <user/track_data.h>
#include <user/heap.h>
#include <user/pathfinding.h>
#include <user/mio.h>
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

// Travelling over a piece of reserved track is equivalent to 10 meters
#define RESERVED_COST(id, edge)                                             \
    (((edge).reserved == id || (edge).reserved < 0) ? 0 : 10000)

int planPath(struct TrackNode *list, int train_id,
        struct TrackNode *start, struct TrackNode *goal,
        struct TrackNode **output, int *output_cost) {

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
    PathHeapPush(&heap, &nodes[start->idx]);

    {
        nodes[start->reverse->idx].cost = 0;
        nodes[start->reverse->idx].total = heuristic(start->reverse, goal);
        nodes[start->reverse->idx].from = &nodes[start->idx];
        PathHeapPush(&heap, &nodes[start->reverse->idx]);
    }

    struct PathNode *goalNode = &nodes[goal->idx];

    while (heap.count && heap.heap[1]->total <= goalNode->cost) {
        struct PathNode *next = PathHeapPop(&heap);
        struct TrackNode *seq[3];
        int seqCost[3];
        int seqCount = 0;

        {
            seq[seqCount] = next->node->reverse;
            seqCost[seqCount++] = 0;
        }

        if (!(next->node->type & (NODE_EXIT | NODE_BRANCH_CURVED))) {
            seq[seqCount] = next->node->edge[0].dest;
            seqCost[seqCount++] = next->node->edge[0].dist +
                RESERVED_COST(train_id, next->node->edge[0]);
        }

        if ((next->node->type & (NODE_BRANCH | NODE_BRANCH_CURVED))) {
            seq[seqCount] = next->node->edge[1].dest;
            seqCost[seqCount++] = next->node->edge[1].dist +
                RESERVED_COST(train_id, next->node->edge[1]);
        }

        for (int i = 0; i < seqCount; ++i) {
            struct TrackNode *node = seq[i];
            int cost = seqCost[i];
            assert(cost >= 0);
            if(next->cost + cost < nodes[node->idx].cost){
                nodes[node->idx].cost = next->cost + cost;
                nodes[node->idx].total = nodes[node->idx].cost +
                    heuristic(nodes[node->idx].node, goal);

                nodes[node->idx].from = next;
                if(nodes[node->idx].total < goalNode->total && node != goal){
                    PathHeapPush(&heap,&nodes[node->idx]);
                }
            }
        }
    }

    // At this point, the current goal node has the shortest path made out of back-pointers.
    if (output_cost != NULLPTR) {
        *output_cost = goalNode->cost;
    }

    int solutionCount = 0;
    for(struct PathNode *n = goalNode; n != 0; n=n->from){
        ++solutionCount;
    }

    if (output != NULLPTR) {
        int i = solutionCount;
        for(struct PathNode *n = goalNode; n != 0; n=n->from){
            output[--i] = n->node;
        }
        assert(i == 0);
    }

    return solutionCount;
}

int alongTrack(TurnoutTable turnouts, struct Position *start,
    int dist, struct Position *end, struct TrackNode **path,
    struct TrackNode **follow,
    struct TrackEdge **edges, int *edgeCount, bool beyond){

    dist = dist + start->offset;

    struct TrackNode *pos = start->node;

    int last_dist = 0;
    struct TrackNode *last_pos = NULLPTR;

    int i = 0;
    int j = 0;

    while (dist >= 0 && pos->type != NODE_EXIT) {
        for(int k = 0; follow && (k < 5); ++k){
            if(follow[k] == pos){
                follow += k;
            }
        }
        last_dist = dist;
        last_pos = pos;
        /*{
            struct String s;
            sinit(&s);
            sputstr(&s,pos->name);
            sputc(&s,':');
            sputuint(&s, dist, 10);
            logS(&s);
        }*/

        if (path) {
            path[i++] = pos;
        }

        int dir = 0;
        if(pos->type == NODE_BRANCH){
            if(follow && *follow == pos){
                dir = (pos->edge[0].dest == follow[1]) ? 0 : 1;
            } else {
                dir = isTurnoutCurved(turnouts, pos->num);
            }
        } else if(pos->type == NODE_BRANCH_CURVED){
            dir = 1;
        }

        if (edges) {
            edges[j++] = &pos->edge[dir];

            if (pos->type == NODE_BRANCH) {
                // project up to the next sensor on the wrong branch
                edges[j++] = &pos->edge[(dir == 0) ? 1 : 0];
                struct TrackNode *node = pos->edge[(dir == 0) ? 1 : 0].dest;

                while (node->type & ~(NODE_SENSOR | NODE_EXIT)) {
                    int dirprime = 0;

                    if (node->type == NODE_BRANCH) {
                        dirprime = isTurnoutCurved(turnouts, node->num);
                    } else if (node->type == NODE_BRANCH_CURVED) {
                        dirprime = 1;
                    }

                    edges[j++] = &node->edge[dirprime];
                    node = node->edge[dirprime].dest;
                }
            }
        }

        dist -= pos->edge[dir].dist;
        pos = pos->edge[dir].dest;
    }

    if (path && (dist >= 0 || beyond)) {
        path[i++] = pos;
    }

    if(edgeCount){
        *edgeCount = j;
    }

    if(dist >= 0 || beyond){
        end->node = pos;
        end->offset = dist;
    } else {
        end->node = last_pos;
        end->offset = last_dist;
    }

    return i;
}

#define MAX_SEARCH 40

int distance(TurnoutTable turnouts, struct Position *from, struct Position *to) {
    int dist = -from->offset;
    struct TrackNode *pos = from->node;

    int i = 0;
    while(pos != to->node){
        if(pos->type == NODE_EXIT){
            // Can't get there.
            return 0x7FFFFFFF;
        }
        int dir = 0;
        if(pos->type == NODE_BRANCH){
            dir = isTurnoutCurved(turnouts, pos->num);
        } else if(pos->type == NODE_BRANCH_CURVED){
            dir = 1;
        }
        dist += pos->edge[dir].dist;
        pos = pos->edge[dir].dest;
        ++i;
        if(i > MAX_SEARCH){
            // Can't get there from here with the current turnouts.
            return 0x7FFFFFFF;
        }
    }
    return dist + to->offset;
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
        sputstr(&s, "Plan ");
        sputstr(&s,srcName);
        sputstr(&s," to ");
        sputstr(&s,destName);
        logS(&s);
    }

    struct TrackNode *srcNode = lookupTrackNode(hashtbl, srcName);
    struct TrackNode *destNode = lookupTrackNode(hashtbl, destName);

    assert(srcNode);
    assert(destNode);

    struct TrackNode *route[50];
    int cost;
    // use an invalid train id for planning purposes
    int count = planPath(nodes, 82, srcNode, destNode, route, &cost);
    struct String s;

    for(int i = 0; i < count; ++i){
        sinit(&s);
        sputstr(&s, "-> ");
        sputstr(&s,route[i]->name);
        logS(&s);
    }

    sinit(&s);
    sputstr(&s, "Cost: ");
    sputint(&s, cost, 10);
    logS(&s);

}

struct TrackNode **nextReverse(struct TrackNode **path, struct TrackNode *end){
    while(*path != end &&
        (*path)->reverse != *(path+1)){
        ++path;
    }
    return path;
}
