#include <user/clock_drawer.h>
#include <user/clock.h>
#include <user/mio.h>
#include <user/string.h>
#include <stdbool.h>
#include <user/syscall.h>
#include <user/vt100.h>
#include <user/priorities.h>

// Time is in tenths of a second.
void printTime(int time){
    struct String s;
    sinit(&s);
    sputstr(&s, CURSOR_HIDE);
    sputstr(&s, CURSOR_SAVE);
    vtPos(&s, TIMER_ROW, TIMER_COL);
    sputc(&s, '0' + time / 3600000);
    sputc(&s, '0' + (time % 3600000) / 360000);
    sputc(&s, ':');
    sputc(&s, '0' + (time % 360000) / 60000);
    sputc(&s, '0' + (time % 60000) / 6000);
    sputc(&s, ':');
    sputc(&s, '0' + (time % 6000) / 1000);
    sputc(&s, '0' + (time % 1000) / 100);
    sputc(&s, '.');
    sputc(&s, '0' + (time % 100) / 10);
    sputstr(&s, CURSOR_RESTORE);
    sputstr(&s, CURSOR_SHOW);
    mioPrint(&s);
}

static volatile int loop;
void clockDrawer(void){
    // A tenth of a second is 10 ticks
    int now = Time();
    while(loop){
        printTime(now);
        now += 5;
        DelayUntil(now);
    }
    Exit();
}

void clockDrawerInit(void){
    loop = true;
    Create(TASK_PRIORITY, clockDrawer);
}

void clockDrawerQuit(void){
    loop = false;
}
