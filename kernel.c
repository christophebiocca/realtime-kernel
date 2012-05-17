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
    // sp + 0 = r15 (pc)
    // sp + 1 = r0
    // ...
    // sp + 12 = r11 (fp)
    // sp + 13 = r12
    // sp + 14 = r14 (lr)

    task->ret = 1;
    task->spsr = UserMode | DisableIRQ | DisableFIQ;
    task->sp = ((unsigned int *) 0x300000) - 15;

    for(int i = 0; i < 15; ++i){
        *(task->sp + i) = i;
    }
    *(task->sp) = ((unsigned int) &userModeTask) + 0x200000; // Set starting pc
    *(task->sp + 12) = 0x300000; // For now, set frame to same as sp
    bwprintf(COM2, "%x %x %x\r\n", task->ret, task->spsr, task->sp);
    bwputstr(COM2, "Stack:\r\n");
    for(int i = 0; i < 15; ++i){
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

    for(unsigned int i = 0; i < 150; ++i){
        struct TaskDescriptor* active = &desc;
        bwputstr(COM2, "kerxitEntry\r\n");

        register unsigned int *sp asm("r4") = active->sp;
        *(sp + 1) = active->ret;
        unsigned int arg0;
        unsigned int arg1;
        unsigned int arg2;
        unsigned int arg3;
        
        register unsigned int spsr asm("r5") = active->spsr;

        asm volatile(
            "stmfd sp!, {r6-r12, r14}\n\t"  // save kregs on kstack
            "ldmfd %0!, {r14}\n\t"          // Get the stored pc
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
            "stmfd %0!, {r14}\n\t"          // Store the task's pc on it's stack
            "mrs %1, spsr\n\t"              // Obtain activity's spsr
            "ldmfd sp!, {r6-r12, r14}\n\t"  // unroll kregs from kstack
            "mov %2, r0\n\t"
            "mov %3, r1\n\t"
            "mov %4, r2\n\t"
            "mov %5, r3\n\t"
            : "+r"(sp), "+r"(spsr), "=r"(arg0),
                "=r"(arg1), "=r"(arg2), "=r"(arg3)
            :
            : "r0", "r1", "r2", "r3"
        );

        bwputr(COM2, arg0);
        bwputr(COM2, arg1);
        bwputr(COM2, arg2);
        bwputr(COM2, arg3);
        active->sp = sp;
        active->spsr = spsr;
        struct Request req;
        handle(&req);
    }
}

