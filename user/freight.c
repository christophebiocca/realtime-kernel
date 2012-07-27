#include <debug.h>
#include <lib.h>
#include <stdbool.h>

#include <user/freight.h>
#include <user/log.h>
#include <user/priorities.h>
#include <user/syscall.h>
#include <user/track_data.h>

volatile struct Freight g_freights[NUM_FREIGHTS];

struct FreightCommand {
    enum {
        CLAIM,
        SET_NEXT,
        QUIT,
    } type;

    union {
        char padding_DO_NOT_USE[12];
        int claim_id;

        struct {
            struct TrackNode *src;
            struct TrackNode *dest;
        } setNext;
    };
};

static void freightServer(void) {
    logAssoc("fr");
    int next_freight_id = 0;

    // create initial freights
    for (int i = 0; i < NUM_FREIGHTS; ++i) {
        g_freights[i].id = next_freight_id++;
        g_freights[i].src = randomTrackNode(nodes);
        g_freights[i].dest = randomTrackNode(nodes);
    }

    struct Freight next;
    next.id = false;
    next.src = NULLPTR;
    next.dest = NULLPTR;

    int tid;
    struct FreightCommand cmd;

    bool quit = false;
    while (!quit) {
        int len = Receive(&tid, (char *) &cmd, sizeof(struct FreightCommand));
        assert(len == sizeof(struct FreightCommand));

        switch (cmd.type) {
            case CLAIM: {
                int reply = false;

                for (int i = 0; i < NUM_FREIGHTS; ++i) {
                    if (g_freights[i].id == cmd.claim_id) {
                        reply = true;

                        // replace this freight since its been claimed
                        g_freights[i].id = next_freight_id++;

                        if (next.id) {
                            logC("using user specified freight");

                            g_freights[i].src = next.src;
                            g_freights[i].dest = next.dest;

                            next.id = false;
                        } else {
                            logC("randomly generating freight");

                            g_freights[i].src = randomTrackNode(nodes);
                            g_freights[i].dest = randomTrackNode(nodes);
                        }

                        break;
                    }
                }

                Reply(tid, (char *) &reply, sizeof(int));
                break;
            }

            case SET_NEXT: {
                next.id = true;
                next.src = cmd.setNext.src;
                next.dest = cmd.setNext.dest;

                Reply(tid, NULLPTR, 0);

                break;
            }

            case QUIT:
                quit = true;
                Reply(tid, NULLPTR, 0);
                break;

            default:
                assert(0);
        }
    }

    Exit();
}

static int g_freight_server_tid;
void freightInit(void) {
    g_freight_server_tid = Create(FREIGHT_PRIORITY, freightServer);
    assert(g_freight_server_tid >= 0);
}

int freightClaim(int id) {
    struct FreightCommand cmd;

    cmd.type = CLAIM;
    cmd.claim_id = id;

    int response;
    Send(
        g_freight_server_tid,
        (char *) &cmd, sizeof(struct FreightCommand),
        (char *) &response, sizeof(int)
    );

    return response;
}

void freightSetNext(struct TrackNode *src, struct TrackNode *dest) {
    struct FreightCommand cmd;

    cmd.type = SET_NEXT;
    cmd.setNext.src = src;
    cmd.setNext.dest = dest;

    Send(
        g_freight_server_tid,
        (char *) &cmd, sizeof(struct FreightCommand),
        NULLPTR, 0
    );
}

void freightQuit(void) {
    struct FreightCommand cmd;
    cmd.type = QUIT;

    Send(
        g_freight_server_tid,
        (char *) &cmd, sizeof(struct FreightCommand),
        NULLPTR, 0
    );
}
