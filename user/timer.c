#include <stdbool.h>

#include <bwio.h>
#include <ts7200.h>

#include <user/timer.h>
#include <user/syscall.h>

// timer 1 underflow interrupt
#define INT_TIMER1  4

// raise interrupt every 500ms
#define NTICKS      1000

#define TIME_REQUEST    -1
#define TICK_REQUEST    -2
#define QUIT            -3

static void timerNotifier(void) {
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
            bwputstr(COM2, "[time-notifier] quitting...\r\n");
            break;
        }
    }

    Exit();
}

// max number of tasks we support delaying
#define MAX_DELAYS  64
static void timerServer(void) {
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

    Create(1, timerNotifier);

    while (1) {
        Receive(&tid, (char *) &request, sizeof(int));

        if (request >= 0) {
            // Delay
            if (ndelays == MAX_DELAYS) {
                int response = -1;
                Reply(tid, (char *) &response, sizeof(int));
            } else {
                int i;

                // FIXME: binary search?
                for (i = 0; i < (ndelays - 1); ++i) {
                    if (delays[i].trigger > request) {
                        break;
                    }
                }

                // shift everything down
                // FIXME: memmove?
                for (int j = i + 1; j <= ndelays; ++j) {
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
            bwprintf(COM2, "[time-server]: Invalid Request: %d\r\n", request);
            // reply to unblock sender
            Reply(tid, (char *) &ticks, sizeof(int));
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

            bwputstr(COM2, "[time-server] quitting...\r\n");
            Reply(should_quit, (char *) &ticks, sizeof(int));
            break;
        }
    }

    Exit();
}

static void idleTask(void) {
    while (1) {
        Pass();
    }

    Exit();
}

static int g_timer_server_tid;
void timerInitTask(void) {
    g_timer_server_tid = Create(2, timerServer);
    if (g_timer_server_tid < 0) {
        bwputstr(COM2, "Error creating timer server\r\n");
    }

    Create(31, idleTask);
    Exit();
}

int Delay(int ticks) {
    return DelayUntil(Time() + ticks);
}

int Time(void) {
    int request = TIME_REQUEST, response;

    Send(
        g_timer_server_tid,
        (char *) &request, sizeof(int),
        (char *) &response, sizeof(int)
    );

    return response;
}

int DelayUntil(int nticks) {
    int response;

    Send(
        g_timer_server_tid,
        (char *) &nticks, sizeof(int),
        (char *) &response, sizeof(int)
    );

    return response;
}

int TimeQuit(void) {
    int request = QUIT, response;

    Send(
        g_timer_server_tid,
        (char *) &request, sizeof(int),
        (char *) &response, sizeof(int)
    );

    return response;
}
