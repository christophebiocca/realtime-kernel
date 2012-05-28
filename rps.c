#include <bwio.h>
#include <syscalls.h>
#include <stdbool.h>
#include <user_task.h>
#include <nameserver.h>

typedef enum {
    SIGN_UP,
    PLAY_ROCK,
    PLAY_PAPER,
    PLAY_SCISSORS,
    QUIT
} rpsRequest;

typedef enum {
    WIN,
    LOSS,
    DRAW,
    ROUND_BEGIN,
    OPPONENT_QUIT,
    ARENA_FULL,
    NOT_PLAYING
} rpsResponse;

#define SIMULTANEOUS_GAMES  16
#define LOG(...)            bwprintf(COM2, "[rps-server] " __VA_ARGS__)
static void rpsServer(void) {
    static int awaiting_tid = 0; // client awaiting a challenger
    static struct {
        int tid;
        rpsRequest move;
        int opponent;
    } state[SIMULTANEOUS_GAMES];

    for (int i = 0; i < SIMULTANEOUS_GAMES; ++i) {
        state[i].tid = 0;
    }

    LOG("Reticulating splines...");
    int ret = RegisterAs("rps");

    if (!ret) {
        LOG("Error registering with name server: %d\r\n", ret);
        Exit();
    }

    int tid, request, response;
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

                int index1 = -1, index2 = -1;
                for (int i = 0; i < SIMULTANEOUS_GAMES; ++i) {
                    if (!state[i].tid) {
                        if (index1 == -1) {
                            index1 = i;
                        } else if (index2 == -1) {
                            index2 = i;
                        } else {
                            // found two free indices!
                            break;
                        }
                    }
                }

                // if index1 == -1 then index2 == -1
                if (index2 == -1) {
                    LOG("Unfortunately, the arena is currently booked. "
                        "Dropping player %d silently... \r\n", tid);
                    response = ARENA_FULL;
                    Reply(tid, (char*) &response, sizeof(int));
                    break;
                }

                state[index1].tid = awaiting_tid;
                state[index1].move = SIGN_UP;
                state[index1].opponent = index2;

                state[index2].tid = tid;
                state[index2].move = SIGN_UP;
                state[index2].opponent = index1;

                awaiting_tid = 0;

                response = ROUND_BEGIN;
                Reply(awaiting_tid, (char*) &response, sizeof(int));
                Reply(tid, (char*) &response, sizeof(int));
            }

            break;

        case PLAY_ROCK:
        case PLAY_PAPER:
        case PLAY_SCISSORS: {
            bool valid = false;

            for (int i = 0; i < SIMULTANEOUS_GAMES; ++i) {
                if (state[i].tid == tid) {
                    state[i].move = request;

                    if (state[i].move != SIGN_UP &&
                            state[state[i].opponent].move != SIGN_UP) {
                        // FIXME: decide the win / loss / draw state here and reply
                        response = WIN;
                        Reply(state[i].tid, (char*) &response, sizeof(int));
                        Reply(state[state[i].opponent].tid, (char*) &response, sizeof(int));

                        state[i].move = SIGN_UP;
                        state[state[i].opponent].move = SIGN_UP;

                        LOG("Round over!\r\n");
                        // FIXME: log win / loss /draw

                        valid = true;
                        break;
                    }
                }
            }


            if (!valid) {
                LOG("player %d issued a play request but is not in a "
                    "match... ignoring\r\n");
                response = NOT_PLAYING;
                Reply(tid, (char*) &response, sizeof(int));
            }

            break;
        }

        case QUIT: {
            bool valid = false;

            for (int i = 0; i < SIMULTANEOUS_GAMES; ++i) {
                if (state[i].tid == tid) {
                    int op = state[i].opponent;

                    response = OPPONENT_QUIT;
                    Reply(state[op].tid, (char*) &response, sizeof(int));
                    Reply(state[op].tid, (char*) &response, sizeof(int));

                    state[i].tid = 0;
                    state[op].tid = 0;

                    valid = true;
                    break;
                }
            }


            if (!valid) {
                LOG("player %d issued a quit request but is not in a "
                    "match... ignoring\r\n");
                response = NOT_PLAYING;
                Reply(tid, (char*) &response, sizeof(int));
            }

            break;
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
