#include "user_task.h"
#include "bwio.h"
#include "syscalls.h"

static void childTask() {
    int tid = MyTid();
    int p_tid = MyParentTid();

    bwprintf(COM2, "Task: %d, Parent: %d\r\n", tid, p_tid);
    Pass();
    bwprintf(COM2, "Task: %d, Parent: %d\r\n", tid, p_tid);
    Exit();
}

void userModeTask(){
    for (int i = 0; i < 4; ++i) {
        bwprintf(COM2, "Created: %d\r\n", Create(
            (i < 2) ? 0 : 2,
            childTask
        ));
    }

    bwputstr(COM2, "First: exiting\r\n");
    Exit();
}

