#include <stdbool.h>

#include <debug.h>
#include <bwio.h>
#include <ts7200.h>

#include <user/clock.h>
#include <user/syscall.h>

// timer 1 underflow interrupt
#define INT_TIMER1  4

// raise interrupt every 10ms
#define NTICKS      20

#define TIME_REQUEST    -1
#define TICK_REQUEST    -2
#define QUIT            -3

static void clockNotifier(void) {
    int serverTid = MyParentTid();
    int request = TICK_REQUEST;
    int response;

    *((unsigned int *) (TIMER1_BASE + LDR_OFFSET)) = NTICKS;
    // enable a 2kHz periodic timer
    *((unsigned int *) (TIMER1_BASE + CRTL_OFFSET)) = ENABLE_MASK | MODE_MASK;

    while (1) {
        AwaitEvent(INT_TIMER1);
        *((unsigned int *) (TIMER1_BASE + CLR_OFFSET)) = 0;
        Send(
            serverTid,
            (char *) &request, sizeof(int),
            (char *) &response, sizeof(int)
        );

        if (!response) {
            trace("quit");
            break;
        }
    }

    Exit();
}

// max number of tasks we support delaying
#define MAX_DELAYS  64

// Request: int
//      >= 0                Delay n ticks before replying with current ticks
//      == TIME_REQUEST     Reply with current number of ticks
//      == TICK_REQUEST     Increment ticks
//      == QUIT             Quit notifier on next TICK_REQUEST and then quit
// Reply: int
//      >= 0                Number of ticks that have elapsed
//      == -1               Too many tasks are delaying
//      == -2               Unknown request
static void clockServer(void) {
    int tid;
    int request;
    int ticks = 0;

    struct {
        int trigger;
        int tid;
    } delays[MAX_DELAYS];
    int ndelays = 0;

    int should_quit = -1;
    bool has_quit;

    Create(1, clockNotifier);

    while (1) {
        assert(ticks >= 0);
        assert(ndelays >= 0);

        Receive(&tid, (char *) &request, sizeof(int));

        if (request >= 0) {
            // Delay
            if (ndelays == MAX_DELAYS) {
                trace("cannot handle delay request by %d, dropping", tid);
                int response = -1;
                Reply(tid, (char *) &response, sizeof(int));
            } else {
                int i;

                // FIXME: binary search?
                for (i = 0; i < ndelays; ++i) {
                    if (delays[i].trigger > request) {
                        break;
                    }
                }

                // shift everything down
                // FIXME: memmove?
                for (int j = ndelays; j > i; --j) {
                    delays[j] = delays[j - 1];
                }

                delays[i].trigger = request;
                delays[i].tid = tid;
                ++ndelays;
            }
        } else if (request == TIME_REQUEST) {
            Reply(tid, (char *) &ticks, sizeof(int));
        } else if (request == TICK_REQUEST) {
            int response;

            if (should_quit >= 0) {
                response = 0;
                has_quit = true;
            } else {
                response = 1;
                ++ticks;
            }

            Reply(tid, (char *) &response, sizeof(int));
        } else if (request == QUIT) {
            should_quit = tid;
            has_quit = false;
        } else {
            trace("invalid request: %d", request);

            int response = -2;
            Reply(tid, (char *) &response, sizeof(int));
        }

        int triggered = 0;
        for (int i = 0; i < ndelays && ticks >= delays[i].trigger; ++i) {
            Reply(delays[i].tid, (char *) &ticks, sizeof(int));
            ++triggered;
        }

        // FIXME: memmove
        if (triggered != 0) {
            for (int i = triggered; i < ndelays; ++i) {
                delays[i - triggered] = delays[i];
            }
            ndelays -= triggered;
        }

        if (should_quit >= 0 && has_quit) {
            // clear out everyone blocked on us
            for (int i = 0; i < ndelays; ++i) {
                Reply(delays[i].tid, (char *) &ticks, sizeof(int));
            }

            trace("quit");
            Reply(should_quit, (char *) &ticks, sizeof(int));
            break;
        }
    }

    Exit();
}

static int g_clock_server_tid;
void clockInitTask(void) {
    g_clock_server_tid = Create(2, clockServer);
    if (g_clock_server_tid < 0) {
        trace("error creating timer server: %d", g_clock_server_tid);
    }
}

int Delay(int ticks) {
    return DelayUntil(Time() + ticks);
}

int Time(void) {
    int request = TIME_REQUEST, response;

    Send(
        g_clock_server_tid,
        (char *) &request, sizeof(int),
        (char *) &response, sizeof(int)
    );

    return response;
}

int DelayUntil(int nticks) {
    int response;

    Send(
        g_clock_server_tid,
        (char *) &nticks, sizeof(int),
        (char *) &response, sizeof(int)
    );

    return response;
}

int ClockQuit(void) {
    int request = QUIT, response;

    Send(
        g_clock_server_tid,
        (char *) &request, sizeof(int),
        (char *) &response, sizeof(int)
    );

    return response;
}
