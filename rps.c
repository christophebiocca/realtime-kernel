#include <bwio.h>
#include <syscalls.h>
#include <user_task.h>
#include <nameserver.h>

typedef enum {
    SIGN_UP,
    PLAY_ROCK,
    PLAY_PAPER,
    PLAY_SCISSORS,
    QUIT
} rpsRequest;

// we support 32 matches going at once
#define CONCURRENT_MATCHES  16
#define LOG(...)    bwprintf(COM2, "[rps-server] " __VA_ARGS__)
static void rpsServer(void) {
    static int awaiting_tid = 0; // client awaiting a challenger
    static int current_tid1 = 0, current_tid2 = 0;
    static rpsRequest current_tid1_state, current_tid2_state;

    LOG("Reticulating splines...");
    int ret = RegisterAs("rps");

    if (!ret) {
        LOG("Error registering with name server: %d\r\n", ret);
        Exit();
    }

    int tid, request;
    Receive(&tid, (char *) &request, sizeof(int));

    switch (request) {
        case SIGN_UP:
            LOG("Sign up request by %d\r\n", tid);

            // FIXME: we should probably scan to see if tid is already involved
            // in a match. Consider the case where a client issues SIGN_UP, is
            // matched, and then issues SIGN_UP again instead of PLAY_*.
            if (!awaiting_tid) {
                LOG("We need to wait for a challenger...\r\n");
                awaiting_tid = tid;
            } else {
                LOG("Found match: %d vs %d\r\n", awaiting_tid, tid);

                if (current_tid1 != 0 || current_tid2 != 0) {
                    LOG("Unfortunately, the arena is currently booked. "
                        "Dropping player %d silently... \r\n", tid);
                    // FIXME: tell client why!
                    Reply(tid, (char*) &request, sizeof(int));
                }

                current_tid1 = awaiting_tid;
                current_tid2 = tid;
                current_tid1_state = current_tid2_state = SIGN_UP;

                awaiting_tid = 0;

                // FIXME: figure out void replies
                Reply(awaiting_tid, (char*) &request, sizeof(int));
                Reply(tid, (char*) &request, sizeof(int));
            }

            break;

        case PLAY_ROCK:
        case PLAY_PAPER:
        case PLAY_SCISSORS: {
            if (current_tid1 == tid) {
                current_tid1_state = request;
            } else if (current_tid2 == tid) {
                current_tid2_state = request;
            } else {
                LOG("player %d issued a play request but is not in a "
                    "match... ignoring\r\n");
                Reply(tid, (char*) &request, sizeof(int));
                break;
            }

            if (current_tid1_state != SIGN_UP && current_tid2_state != SIGN_UP) {
                // FIXME: decide the win / loss / draw state here and reply
                int response = WIN;
                Reply(current_tid1, &request, sizeof(int));
                Reply(current_tid2, &request, sizeof(int));

                LOG("Round over!");
                // FIXME: log win / loss /draw
                current_tid1 = current_tid2 = 0;
            }
        }

        case QUIT:
            int *swap =
                (current_tid1 == tid) ? &current_tid1 :
                (current_tid2 == tid) ? &current_tid2 :
                NULL;

            if (swap) {
                LOG("Player %d is quitting!\r\n", tid);

                if (awaiting_tid) {
                    *swap = awaiting_tid;
                    Reply(awaiting_tid, (char*) &request, sizeof(int));
                    awaiting_tid = 0;
                } else {
                }
            }
    }

    Exit();
}

void rpsUserModeTask(void) {
    Create(1, task_nameserver);
    Create(2, rpsServer);
    //Create(2, rpsClient);
    //Create(2, rpsClient);
    Exit();
}
