#include "user_task.h"
#include "bwio.h"
#include "syscalls.h"

static void childTask() {
    int tid = MyTid();
    int p_tid = MyParentTid();
    char* message = "Yo";
    bwprintf(COM2, "Task: %d, Parent: %d sending message.\r\n", tid, p_tid);
    char reply[3];
    int len = Send(p_tid, message, 3, reply, 3);
    bwprintf(COM2, "Task: %d got a reply of length %d (%s)\r\n", tid, len, reply);
    Exit();
}

void userModeTask(){
    for (int i = 0; i < 4; ++i) {
        bwprintf(COM2, "Created: %d\r\n", Create(
            (i < 2) ? 2 : 0,
            childTask
        ));
    }

    for (int i = 0; i < 4; ++i) {
        char msg[3];
        int tid = -3;
        int len = Receive(&tid, msg, 3);
        bwprintf(COM2, "First got a message of length %d (%s) from %d\r\n", len, msg, tid);
        Reply(tid, "Oy", 3);
    }

    bwputstr(COM2, "First: exiting\r\n");
    Exit();
}

