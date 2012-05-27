#include "user_task.h"
#include "bwio.h"
#include "syscalls.h"

#include <nameserver.h>

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
            (i < 2) ? 2 : 0,
            childTask
        ));
    }

    bwputstr(COM2, "First: exiting\r\n");
    Exit();
}

/* Nameserver testing init tasks */

static void nsChildTask() {
    bwprintf(COM2, "Server ID: %d\r\n", WhoIs("server"));
    Exit();
}

static void nsServerTask() {
    bwprintf(COM2, "Registering \"server\": %d\r\n", RegisterAs("server"));
    Exit();
}

void nsUserModeTask(){
    bwprintf(COM2, "Nameserver: %d\r\n", Create(1, task_nameserver));
    bwprintf(COM2, "Server: %d\r\n", Create(2, nsServerTask));
    bwprintf(COM2, "Child: %d\r\n", Create(3, nsChildTask));

    Exit();
}
