#include "user_task.h"
#include "bwio.h"

void userModeTask(){
    bwputstr(COM2, "-> -> USER TASK!\r\n");
    asm volatile("swi 0");
}

