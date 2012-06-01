#include <kernel/interrupts.h>
#include "task_internal.h"
#include <ts7200.h>
#include <lib.h>
#include <bwio.h>

static struct TaskDescriptor *interruptTable[64];
static int vic[2];

void initInterruptSystem(void){
    for(int i = 0; i < 64; ++i){
        interruptTable[i] = 0;
    }
    vic[0] = 0x800B0000;
    vic[1] = 0x800C0000;
}

int awaitInterrupt(int interruptID){
    if(interruptID < 0 || interruptID > 63){
        return -1;
    }
    interruptTable[interruptID] = g_active_task;
    g_active_task = 0;
    // Finally set the mask.
    volatile unsigned int *enableMask = (unsigned int *)(vic[interruptID/32] + VIC_INT_ENABLE);
    *enableMask |= 1 << (interruptID % 32);

    return 0;
}

void handleInterrupt(){
    for(int i = 0; i < 2; ++i){
        int statusMask;
        while((statusMask = *((volatile unsigned int *)(vic[i]+VIC_IRQ_STATUS_OFFSET)))){
            int intIdOffset = countLeadingZeroes(statusMask);
            struct TaskDescriptor *td = interruptTable[intIdOffset + i*32];
            priorityQueuePush(taskPriority(td->id), td);
            *((volatile unsigned int *)(vic[i]+VIC_INT_ENABLE_CLEAR)) = 1 << intIdOffset;
        } 
    }
}
