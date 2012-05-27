#include "user_task.h"
#include "bwio.h"
#include "syscalls.h"

#include <nameserver.h>

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
