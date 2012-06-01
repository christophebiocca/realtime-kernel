#include <bwio.h>
#include <ts7200.h>

#include <user/init.h>
#include <user/syscall.h>

// timer 1 underflow interrupt
#define INT_TIMER1  4

// raise interrupt every 500ms
#define NTICKS      1000

static void timerNotifier(void) {
    int serverTid = MyParentTid(), tmp;

    *((unsigned int *) (TIMER1_BASE + LDR_OFFSET)) = NTICKS;
    // enable a 2kHz periodic timer
    *((unsigned int *) (TIMER1_BASE + CRTL_OFFSET)) = ENABLE_MASK | MODE_MASK;

    while (1) {
        AwaitEvent(INT_TIMER1);
        *((unsigned int *) (TIMER1_BASE + CLR_OFFSET)) = 0;
        Send(serverTid, (char *) &tmp, sizeof(int), (char *) &tmp, sizeof(int));
    }

    Exit();
}

static void timerServer(void) {
    int ticks = 0, tid, tmp;

    Create(1, timerNotifier);

    while (1) {
        Receive(&tid, (char *) &tmp, sizeof(int));
        Reply(tid, (char *) &tmp, sizeof(int));

        ++ticks;
        bwprintf(COM2, "Got %d ticks\r\n", ticks);
    }

    Exit();
}

static void idleTask(void) {
    while(1) {
        Pass();
    }
}

void timerInitTask(void) {
    Create(2, timerServer);
    Create(31, idleTask);
    Exit();
}

