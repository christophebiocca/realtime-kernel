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
#include <user/log.h>
#include <user/parser.h>
#include <user/sensor.h>
#include <user/turnout.h>
#include <user/track_data.h>

/* Train IO test */
static void idlerTask(void) {
    while (1)
        ;
}

void trainTaskInit(void) {
    clockInitTask();
    mioInit();
    tioInit();
    logInit();

    Create(31, idlerTask);

    Delay(10);
    vtInit();
    Delay(10);

    turnoutInit();
    Delay(10);
    sensorInit();
    Delay(10);
    parserInit();
    Delay(10);
    clockDrawerInit();
    Delay(10);
    controllerInit();
    Delay(10);
    initTrackA(nodes, hashtbl);
}
