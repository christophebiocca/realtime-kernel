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
int alongTrack(TurnoutTable turnouts, struct Position *start,
    int dist, struct Position *end, struct TrackNode **path, bool beyond);

int distance(TurnoutTable turnouts, struct Position *from, struct Position *to);

#endif
