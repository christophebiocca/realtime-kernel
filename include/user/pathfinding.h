#ifndef PATHFINDING_H
#define PATHFINDING_H 1

#include <stdbool.h>
#include <user/track_data.h>
#include <user/turnout.h>

struct Position {
    struct TrackNode *node;
    int offset;
};

void planRoute(char *start, char *end);

int planPath(
    struct TrackNode *list,
    int train_id,
    struct TrackNode *start,
    struct TrackNode *goal,
    struct TrackNode **output
);

// Puts in ret the new position after traveling dist from start,
// given that the switches are in the state given by turnouts.
int alongTrack(
    TurnoutTable turnouts,
    struct Position *start,
    int dist,
    struct Position *end,
    struct TrackNode **path,
    struct TrackNode **follow,
    struct TrackEdge **edges,
    bool beyond
);

// Returns a count of nodes along the path until either dist < 0 or end is reached.
struct TrackNode **alongPath(
    struct TrackNode **path,
    int distance,
    struct TrackNode *end, bool beyond);

// Returns the first node involved in a reverse, or the end if there isn't one.
struct TrackNode **nextReverse(struct TrackNode **path, struct TrackNode *end);

int distance(TurnoutTable turnouts, struct Position *from, struct Position *to);

#endif
