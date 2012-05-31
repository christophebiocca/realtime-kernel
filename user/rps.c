#include <bwio.h>
#include <stdbool.h>

#include <user/nameserver.h>
#include <user/syscall.h>

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

static void safeReply(int tid, rpsResponse resp){
    int response = resp;
    int ret = Reply(tid, (char*) &response, sizeof(int));
    if(ret < 0){
        LOG("Got a negative response to reply! (%d)\r\n");
        Exit();
    }
}

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

    int tid, request;
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
                        safeReply(tid, ARENA_FULL);
                        break;
                    }

                    state[index1].tid = awaiting_tid;
                    state[index1].move = SIGN_UP;
                    state[index1].opponent = index2;

                    state[index2].tid = tid;
                    state[index2].move = SIGN_UP;
                    state[index2].opponent = index1;

                    awaiting_tid = 0;

                    safeReply(state[index1].tid, ROUND_BEGIN);
                    safeReply(state[index2].tid, ROUND_BEGIN);
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

                        if(state[op].move == QUIT){
                            LOG("%d already quit, notifying\r\n", state[op].tid);
                            safeReply(state[i].tid, OPPONENT_QUIT);
                            state[i].tid = state[op].tid = 0;
                        } else if (state[i].move != SIGN_UP && state[op].move != SIGN_UP) {
                            int response = (state[i].move - state[op].move + 3) % 3;
                            safeReply(state[i].tid, response);
                            response = (state[op].move - state[i].move + 3) % 3;
                            safeReply(state[op].tid, response);

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
                    safeReply(tid, NOT_PLAYING);
                }

                break;
            }

            case QUIT: {
                LOG("Got quit request from %d\r\n", tid);
                bool valid = false;

                for (int i = 0; i < SIMULTANEOUS_GAMES; ++i) {
                    if (state[i].tid == tid) {
                        int op = state[i].opponent;
                        state[i].move = QUIT;
                        safeReply(state[i].tid, OPPONENT_QUIT);
                        if(state[op].move == PLAY_ROCK || state[op].move == PLAY_PAPER ||
                            state[op].move == PLAY_SCISSORS){
                            LOG("Notifying %d\r\n", state[op].tid);
                            safeReply(state[op].tid, OPPONENT_QUIT);
                            state[i].tid = 0;
                            state[op].tid = 0;
                        }

                        valid = true;
                        break;
                    }
                }


                if (!valid) {
                    LOG("player %d issued a quit request but is not in a "
                        "match... ignoring\r\n", tid);
                    safeReply(tid, NOT_PLAYING);
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

#define CLIENT_LOG(msg, ...) bwprintf(COM2, "[client %d] " msg, client_id , ##  __VA_ARGS__)

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
    for(int i = next(4) + 1; i >= 0;){
        CLIENT_LOG("Signing up!\r\n");
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
            CLIENT_LOG("%d games left to play\r\n", i);
            // Now let's play for some time.
            int play;
            while((play = next(2)) == 3); // To eliminate bias in picking a move.
            rpsRequest moves[3] = {PLAY_ROCK, PLAY_PAPER, PLAY_SCISSORS};
            char *mvNames[3] = {"Rock", "Paper", "Scissors"};
            request = moves[play];
            CLIENT_LOG("Will play %s\r\n", mvNames[play]);
            len = Send(server, (char*) &request, 4, (char*) &response, 4);
            if(len < 0){
                CLIENT_LOG("Got a bad response when playing! %d\r\n", len);
                Exit();
            }
            if(response == OPPONENT_QUIT){
                // We need to sign up again.
                CLIENT_LOG("Other player dropped.\r\n");
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
    CLIENT_LOG("Going to quit!\r\n");
    len = Send(server, (char*) &request, 4, (char*) &response, 4);
    if(len < 0){
        CLIENT_LOG("Got a bad response when quitting! %d\r\n", len);
    }
    Exit();
}

static void playerSpawner(void){
    for(int i = 0; i < 6 ; ++i){
        int tid = Create(4, rpsClient);
        if(tid < 0){
            bwputstr(COM2, "Couldn't create player, quitting!\r\n");
            break;
        } else {
            bwprintf(COM2, "Created client #%d\r\n", tid);
        }
    }
    Exit();
}

void rpsUserModeTask(void) {
    seed = 0xb4561ab9;
    Create(1, task_nameserver);
    Create(2, rpsServer);
    Create(3, playerSpawner);
    Exit();
}
