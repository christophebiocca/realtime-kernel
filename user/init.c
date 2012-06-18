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

/* Train IO test */
static void idlerTask(void) {
    while (1)
        ;
}

static void trainTask(void) {
    vtInit();
    turnoutInit();

    struct String out;
    sinit(&out);
    sputc(&out, 133);
    tioPrint(&out);
    
    int i = 0;
    while (1) {
        struct String in;
        struct String str;

        tioRead(&in);

        sinit(&str);
        char *buf = sbuffer(&in);
        for (unsigned int j = 0; j < slen(&in); ++j) {
            sputuint(&str, buf[j], 16);
        }
        mioPrint(&str);

        i = (i + slen(&in)) % 10;
        if (i == 0) {
            sinit(&str);
            sputstr(&str, "\r\n");
            mioPrint(&str);

            tioPrint(&out);
        }
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

