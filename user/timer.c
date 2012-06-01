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
            Reply(tid, (char *) &ticks, sizeof(int));
            ++ticks;
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
                delays[i] = delays[i - triggered];
            }
            ndelays -= triggered;
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
