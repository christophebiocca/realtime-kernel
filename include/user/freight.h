#ifndef USER_FREIGHT_H
#define USER_FREIGHT_H  1

#include <user/track_data.h>

struct Freight {
    int id;
    struct TrackNode *src;
    struct TrackNode *dest;
};

#define NUM_FREIGHTS    3
extern volatile struct Freight g_freights[NUM_FREIGHTS];

void freightInit(void);
void freightQuit(void);

int freightClaim(int id);
void freightSetNext(struct TrackNode *src, struct TrackNode *dest);

#endif
