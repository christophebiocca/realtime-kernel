#include <debug.h>
#include <user/timer.h>
#include <user/k3task.h>
#include <user/syscall.h>
#include <bwio.h>

struct ClientInstructions {
    short int delayTime;
    short int numDelays;
};


void clientTask(void){
    static_assert(sizeof(struct ClientInstructions) == 4);
    int tid = MyTid();
    int pid = MyParentTid();
    struct ClientInstructions instr;
    int ret = Send(pid, (char *) 0, 0, (char *) &instr, sizeof(struct ClientInstructions));
    if(ret != 4){
        // Start bitching
        trace("Expected send to return 4, got %d", ret);
        Exit();
    }
    trace("%d: going to wait %d times for %d ticks", tid, instr.numDelays, instr.delayTime);
    for(int i = 0; i < instr.numDelays; ++i){
        // Timed wait.
        ret = Delay(instr.delayTime);
        bwprintf(COM2, "Task: %d, interval: %d, delay #%d\r\n", tid, instr.delayTime, i);
    }
    Exit();
    return;
}

void idlearound(void){
    while(1){
        for(int i = 0; i < 10000; ++i);
        bwputc(COM2, '.');
    }
}

void timerInitialTask(void){
    timerInitTask();

    int priorities[4] = {3,4,5,6};
    int tids[4];
    int delayTimes[4] = {10,23,33,71};
    int delayNums[4] = {20,9,6,3};

    Create(31,idlearound);

    for(int i = 0; i < 4; ++i){
        tids[i] = Create(priorities[i], clientTask);
        if(tids[i] < 1){
            trace("Expected create to return an id, got %d.", tids[i]);
            Exit();
        }
        bwprintf(COM2, "Created %d\r\n", tids[i]);
    }
    for(int i = 0; i < 4; ++i){
        int tid;
        int ret = Receive(&tid, (char*) 0, 0);
        if(ret != 0){
            trace("Expected receive to return 0, got %d.", ret);
            Exit();
        }
        struct ClientInstructions instr;
        for(int j = 0; j < 4; ++j){
            if(tids[j] == tid){
                instr.delayTime = delayTimes[j];
                instr.numDelays = delayNums[j];
                trace("Set speed for %d.", tids[j]);
                break;
            }
        }
        ret = Reply(tid, (char*) &instr, sizeof(struct ClientInstructions));
        if(ret != 0){
            trace("Expected reply to return 0, got %d.", ret);
        }
    }
    Exit();
}
