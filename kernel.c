#include "stdbool.h"
#include "bwio.h"
#include "cpsr.h"
#include "user_task.h"

struct TaskDescriptor {
    unsigned int ret;
    unsigned int spsr;
    unsigned int *sp;
};

struct Request {
    char dummy;
};

static struct TaskDescriptor desc;
static void initTask(struct TaskDescriptor *task){
    // By default, 1 process owning 0x250000 to 0x300000
    // with stack at 0x300000

    // Stored sp points to bottom of stack
    // Stack storage
    // sp - 1 = r15 (pc)
    // sp + 0 = r0
    // ...
    // sp + 12 = r12
    // sp + 13 = r14 (lr)

    task->ret = 1;
    task->spsr = UserMode | DisableIRQ | DisableFIQ;
    task->sp = ((unsigned int *) 0x300000) - 14;

    for(int i = -1; i < 14; ++i){
        *(task->sp + i) = i;
    }
    *(task->sp - 1) = ((unsigned int) &userModeTask) + 0x200000; // Set starting pc
    *(task->sp + 11) = 0x300000; // For now, set frame to same as sp
    bwprintf(COM2, "%x %x %x\r\n", task->ret, task->spsr, task->sp);
    bwputstr(COM2, "Stack:\r\n");
    for(int i = -1; i < 14; ++i){
        bwprintf(COM2, "\t%x(sp+%x): %x\r\n", task->sp+i, i, *(task->sp+i));
    }
}

static void handle(struct Request *req){
    (void) req;
}

int main(){
    unsigned int jump_addr;

    bwsetfifo(COM2, false);
    bwputstr(COM2, "Initializing\r\n");

    asm volatile(
        "ldr %0, =kerlabel" // Load the label from a literal pool
    : "=r"(jump_addr));
    *((unsigned int *)0x28) = jump_addr + 0x200000;

    bwprintf(COM2, "0x28: %x\r\n", *((unsigned int *)0x28));
    initTask(&desc);

    for(unsigned int i = 0; i < 3; ++i){
        struct TaskDescriptor* active = &desc;
        bwputstr(COM2, "kerxitEntry\r\n");
        
        {
            bwprintf(COM2, "%x %x %x\r\n", active->ret, active->spsr, active->sp);
            bwputstr(COM2, "Stack:\r\n");
            for(int i = -1; i < 14; ++i){
                bwprintf(COM2, "\t%x(sp+%x): %x\r\n", active->sp+i, i, *(active->sp+i));
            }
        }

        register unsigned int *sp  = active->sp;
        register unsigned int spsr = active->spsr;
        *sp = active->ret;

        asm volatile(
            "stmfd sp!, {r6-r12, r14}\n\t"  // save kregs on kstack
            "ldr r14, [%0, #-4]\n\t"        // Get the stored pc
            "msr spsr, %1\n\t"              // set active's spsr
            "msr cpsr_c, #0xdf\n\t"         // Switch to system mode
            "mov sp, %0\n\t"                // Set sp.
            "ldmfd sp!, {r0-r12, r14}\n\t"  // Unroll task registers.
            "msr cpsr_c, #0xd3\n\t"         // Switch to supervisor mode
            "movs pc, r14\n\t"              // JUMP!
            "kerlabel:\n\t"                 // ucode: SWI n
            "msr cpsr_c, #0xdf\n\t"         // Switch to system mode
            "stmfd sp!, {r0-r12, r14}\n\t"  // Store user registers.
            "mov %0, sp\n\t"                // Save user's stack pointer
            "msr cpsr_c, #0xd3\n\t"         // Switch to supervisor mode
            "str r14, [%0, #-4]\n\t"        // Store the task's pc on it's stack
            "mrs %1, spsr\n\t"              // Obtain activity's spsr
            "ldmfd sp!, {r6-r12, r14}\n\t"  // unroll kregs from kstack
            : "+r"(sp), "+r"(spsr)
            :
            : "r0", "r1", "r2", "r3"
        );

        active->sp = sp;
        active->spsr = spsr;
        active->ret = 0;
        bwputstr(COM2, "kerxitExit\r\n");

        struct Request req;
        handle(&req);
    }
}

