#include <bwio.h>

#include <ts7200.h>
#include <user/nameserver.h>
#include <user/syscall.h>
#include <user/init.h>

#include <user/string.h>
#include <user/mio.h>
#include <user/tio.h>
#include <user/priorities.h>
#include <user/vt100.h>
#include <user/turnout.h>
#include <user/sensor.h>

/* Train IO test */
static void idlerTask(void) {
    while (1)
        ;
}

static void trainTask(void) {
    vtInit();
    turnoutInit();
    sensorInit();

    while (1) {
        Pass();
    }

    mioQuit();
    tioQuit();

    Exit();
}

void trainTaskInit(void) {
    mioInit();
    tioInit();

    Create(TASK_PRIORITY, trainTask);
    Create(31, idlerTask);
    Exit();
}

