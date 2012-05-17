#include "user_task.h"
#include "bwio.h"
#include "syscalls.h"

void userModeTask(){
    bwputstr(COM2, "Start\r\n");
    int tid = MyTid();
    bwprintf(COM2, "Start %d\r\n", tid);
    int p_tid = MyParentTid();
    bwprintf(COM2, "Start %d, parent %d\r\n", tid, p_tid);
    if(tid < 4){
        for(int j=0; j < 3; ++j){
            int p = tid % 3 + 1 + j;
            bwprintf(COM2, "%d creating ? (%d)\r\n", tid, p);
            int c = Create(p, &userModeTask);
            bwprintf(COM2, "%d created %d (%d)\r\n", tid, c, p);
        }
        for(int i = 0; i < 2; ++i) {
            bwprintf(COM2, "%d: Pass\r\n", tid);
            Pass();
        }
    }
    bwprintf(COM2, "%d: Exit\r\n", tid);
    Exit();
}

