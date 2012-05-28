#include <bwio.h>
#include <syscalls.h>
#include <stdbool.h>
#include <user_task.h>
#include <nameserver.h>

typedef enum {
    PLAY_ROCK = 0,
    PLAY_PAPER = 1,
    PLAY_SCISSORS = 2,
    SIGN_UP,
    QUIT
} rpsRequest;

typedef enum {
    LOSS = 2,
    DRAW = 0,
    WIN = 1,
    ROUND_BEGIN,
    OPPONENT_QUIT,
    ARENA_FULL,
    NOT_PLAYING
} rpsResponse;

#define SIMULTANEOUS_GAMES  16
#define LOG(...)            bwprintf(COM2, "[rps-server] " __VA_ARGS__)
static void rpsServer(void) {
    static int awaiting_tid;
    static struct {
        int tid;
        rpsRequest move;
        int opponent;
    } state[SIMULTANEOUS_GAMES];

    awaiting_tid = 0;
    for (int i = 0; i < SIMULTANEOUS_GAMES; ++i) {
        state[i].tid = 0;
    }

    LOG("Reticulating splines...\r\n");
    int ret = RegisterAs("rps");

    if (ret < 0) {
        LOG("Error registering with name server: %d\r\n", ret);
        Exit();
    }

    int tid, request, response;
    while (true) {
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
                        if (state[i].tid == 0) {
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
                    Reply(state[index1].tid, (char*) &response, sizeof(int));
                    Reply(state[index2].tid, (char*) &response, sizeof(int));
                }

                break;

            case PLAY_ROCK:
            case PLAY_PAPER:
            case PLAY_SCISSORS: {
                LOG("Got play request from %d\r\n", tid);
                bool valid = false;

                for (int i = 0; i < SIMULTANEOUS_GAMES; ++i) {
                    if (state[i].tid == tid) {
                        state[i].move = request;
                        int op = state[i].opponent;

                        if (state[i].move != SIGN_UP && state[op].move != SIGN_UP) {
                            response = (state[i].move - state[op].move + 3) % 3;
                            Reply(state[i].tid, (char*) &response, sizeof(int));
                            response = (state[op].move - state[i].move + 3) % 3;
                            Reply(state[op].tid, (char*) &response, sizeof(int));

                            state[i].move = SIGN_UP;
                            state[op].move = SIGN_UP;

                            LOG("Round over! Press Enter to continue\r\n");
                            bwgetc(COM2);
                            // FIXME: log win / loss /draw

                        }

                        valid = true;
                        break;
                    }
                }


                if (!valid) {
                    LOG("player %d issued a play request but is not in a "
                        "match... ignoring\r\n", tid);
                    response = NOT_PLAYING;
                    Reply(tid, (char*) &response, sizeof(int));
                }

                break;
            }

            case QUIT: {
                LOG("Got quit request from %d\r\n", tid);
                bool valid = false;

                for (int i = 0; i < SIMULTANEOUS_GAMES; ++i) {
                    if (state[i].tid == tid) {
                        int op = state[i].opponent;

                        response = OPPONENT_QUIT;
                        LOG("Found %d\r\n", tid);
                        Reply(state[i].tid, (char*) &response, sizeof(int));
                        LOG("Letting %d know %d\r\n", state[op].tid);
                        Reply(state[op].tid, (char*) &response, sizeof(int));

                        state[i].tid = 0;
                        state[op].tid = 0;

                        valid = true;
                        break;
                    }
                }


                if (!valid) {
                    LOG("player %d issued a quit request but is not in a "
                        "match... ignoring\r\n", tid);
                    response = NOT_PLAYING;
                    Reply(tid, (char*) &response, sizeof(int));
                }

                break;
            }
        }
    }

    Exit();
}

static unsigned int seed;

static inline unsigned int next(int width){
    seed = (seed * 1664525U + 1013904223U);
    return seed >> (32-width);
}

#define CLIENT_LOG(msg, ...) bwprintf(COM2, "[client %d] " msg, client_id,  __VA_ARGS__)

static void rpsClient(void){
    int client_id = MyTid();
    int server = WhoIs("rps");
    if(server < 0){
        CLIENT_LOG("Got a bad response when doing name query! %d\r\n", server);
        Exit();
    }
    int request;
    int response;
    int len;
    for(int i = next(10); i >= 0;){
        CLIENT_LOG("Signing up!\r\n", "");
        // Let's sign up
        request = SIGN_UP;
        len = Send(server, (char*) &request, 4, (char*) &response, 4);
        if(len < 0){
            CLIENT_LOG("Got a bad response when signing up! %d\r\n", len);
        }
        if(response != ROUND_BEGIN){
            CLIENT_LOG("Was expecting ROUND_BEGIN, got %d\r\n", response);
            Exit();
        }
        for(; i >= 0; --i){
            CLIENT_LOG("Got %d games left to play\r\n", i);
            // Now let's play for some time.
            int play;
            while((play = next(2)) == 3); // To eliminate bias in picking a game.
            rpsRequest moves[3] = {PLAY_ROCK, PLAY_PAPER, PLAY_SCISSORS};
            request = moves[play];
            len = Send(server, (char*) &request, 4, (char*) &response, 4);
            if(len < 0){
                CLIENT_LOG("Got a bad response when playing! %d\r\n", len);
                Exit();
            }
            if(response == OPPONENT_QUIT){
                // We need to sign up again.
                CLIENT_LOG("Other player dropped.\r\n", 0);
                break;
            }
            if(response != WIN && response != DRAW && response != LOSS){
                CLIENT_LOG("Was expecting win/loss/draw, got %d\r\n", response);
                Exit();
            }
            CLIENT_LOG("We %s\r\n", (response == WIN ? "Won" : (response == DRAW) ? "Tied" : "Lost"));
        }
    }
    request = QUIT;
    CLIENT_LOG("Going to quit!\r\n", 0);
    len = Send(server, (char*) &request, 4, (char*) &response, 4);
    if(len < 0){
        CLIENT_LOG("Got a bad response when quitting! %d\r\n", len);
    }
    Exit();
}

static void playerSpawner(void){
    Create(2, rpsClient);
    Create(2, rpsClient);
    Exit();
    while(1){
        bwputstr(COM2, "No players are active at the moment, let's create one.\r\n");
        if(Create(2, rpsClient) < 0){
            bwputstr(COM2, "Couldn't create player, quitting!\r\n");
            break;
        }
    }
}

void rpsUserModeTask(void) {
    Create(1, task_nameserver);
    Create(2, rpsServer);
    Create(3, playerSpawner);
    Exit();
}
