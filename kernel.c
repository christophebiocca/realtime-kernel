#include "stdbool.h"
#include "bwio.h"
#include "cpsr.h"
#include "user_task.h"

struct TaskDescriptor {
    unsigned int ret;
    unsigned int spsr;
    unsigned int sp;
};

struct Request {
    char dummy;
};

static struct TaskDescriptor desc;
static void initTask(struct TaskDescriptor *task){
    // By default, 1 process owning 0x250000 to 0x300000
    // with stack at 0x300000
    task->ret = 1;
    task->spsr = UserMode | DisableIRQ | DisableFIQ;
    task->sp = 0x300000 - 16;
    for(int i = 0; i < 16; ++i){
        *(((unsigned int *) task->sp) + i + 1) = 0;
    }
    *(((unsigned int *) task->sp) + 15) = 
        ((unsigned int) &userModeTask) + 0x200000;
    *(((unsigned int *) task->sp) + 13) = 0x300000;
    *(((unsigned int *) task->sp) + 11) = 0x300000;
    bwprintf(COM2, "%x %x %x\r\n", task->ret, task->spsr, task->sp);
    bwputstr(COM2, "Stack:\r\n");
    for(int i = 0; i < 16; ++i){
        bwprintf(COM2, "\t%x(%x): %x\r\n", task->sp+i, i, 
        *(((unsigned int *) task->sp) + i));
    }
}

static void handle(struct Request *req){
    (void) req;
}

int main(){
    bwsetfifo(COM2, false);
    bwputstr(COM2, "Initializing\r\n");
    for(unsigned int i=0; i < 0x40; i+=4){
        bwprintf(COM2, "%x: %x\r\n", i, *((unsigned int *)i));
    }
    bwputstr(COM2, "Altering jump table.\r\n");
    unsigned int jump_addr;
    asm volatile(
        "ldr %0, =kerlabel" // Load the label from a literal pool
    : "=r"(jump_addr));
    *((unsigned int *)0x28) = jump_addr + 0x200000;
    bwprintf(COM2, "0x28: %x\r\n", *((unsigned int *)0x28));
    initTask(&desc);
    struct Request req;
    struct TaskDescriptor* active;
    for(unsigned int i = 0; i < 3; ++i){
        active = &desc;
        bwputstr(COM2, "kerxitEntry\r\n");
        bwprintf(COM2, "Entering a process with: sp: %x,"
            " ret: %x, cpsr: %x\r\n", active->sp, active->ret, active->spsr);
        bwputstr(COM2, "Stack:\r\n");
        for(int i = 0; i < 16; ++i){
            bwprintf(COM2, "\t%x(%x): %x\r\n", active->sp+i, i, *(((unsigned int *) active->sp) + i));
        }
        register unsigned int sp asm("r0") = active->sp;
        register unsigned int spsr asm("r1") = active->spsr;
        register unsigned int ret asm("r2") = active->ret;
        asm volatile(
            "msr spsr_all, %1\n\t"          // Replace spsr with active's cpsr
            "stmfd sp!, {r3-r12,r14}\n\t"   // Store kernel registers.
            "str %2, [%0]\n\t"              // Overwrite r0 with return value.
            //"ldmfd %2, {r0-r15}^\n\t"     // Jump!
            "kerlabel:\n\t"                 // Returns here //
            /* TODO: save the call arguments (use our stack). */
            "msr cpsr_flg, #0x1f\n\t"       // Switch to system mode
            //"stmfd sp, {r0-r15}\n\t"      // Save the registers onto stack
            //"add %0, sp, #0x10\n\t"       // Increment stack pointer
            "msr cpsr_flg, #0x13\n\t"       // Switch to supervisor mode
            "ldmfd sp!, {r3-r12,r14}\n\t"   // Restore kernel registers.
            "mrs %1, spsr_all\n\t"          // Get the spsr
            : "+r"(sp), "+r"(spsr)
            : "r"(ret)
        );
        bwputstr(COM2, "kerxitExit\r\n");
        handle(&req);
    }
}

