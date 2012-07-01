#include <bwio.h>
#include <ts7200.h>

#include <user/init.h>
#include <user/priorities.h>
#include <user/string.h>
#include <user/syscall.h>
#include <user/vt100.h>

#include <user/mio.h>
#include <user/nameserver.h>
#include <user/tio.h>

#include <user/clock.h>
#include <user/clock_drawer.h>
#include <user/controller.h>
#include <user/engineer.h>
#include <user/parser.h>
#include <user/sensor.h>
#include <user/train.h>
#include <user/turnout.h>

/* Train IO test */
static void idlerTask(void) {
    while (1)
        ;
}

static void trainTask(void) {
    vtInit();
    turnoutInit();
    sensorInit();
    trainInit();
    parserInit();
    clockInitTask();
    clockDrawerInit();
    controllerInit();
    initEngineer(39);
    Exit();
}

void trainTaskInit(void) {
    mioInit();
    tioInit();

    Create(TASK_PRIORITY, trainTask);
    Create(31, idlerTask);
    Exit();
}
