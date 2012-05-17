#include "user_task.h"
#include "bwio.h"

void userModeTask(){
    while(1){
        bwputstr(COM2, "START -> USER TASK!\r\n");
        asm volatile("swi 0");
        int i = 10;
        while(--i > 0) {
            bwprintf(COM2, "%d -> USER TASK!\r\n", i);
            asm volatile("swi 0");
        }
        bwputstr(COM2, "END -> USER TASK!\r\n");
    }
}

