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
    /* 14 because we don't pop SP and PC off the stack */
    task->sp = 0x300000 - (14 * 4);

    for(int i = 0; i < 16; ++i){
        *(((unsigned int *) task->sp) + i + 1) = 0;
    }
    *(((unsigned int *) task->sp) + 0) =
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

        register unsigned int sp asm("r1") = active->sp;
        register unsigned int spsr asm("r2") = active->spsr;
        register unsigned int ret asm("r3") = active->ret;

        asm volatile(
            "stmfd sp!, {r0-r12, r14}\n\t"  // save kregs on kstack
            "msr cpsr_flg, #0x1f\n\t"       // switch to system mode
            "mov sp, %0\n\t"                // restore ustack pointer
            "ldmfd sp!, {r0-r12, r14}\n\t"  // unroll uregs from ustack, except SP and PC
            "mov lr, r0\n\t"                // reload user PC from r0
            "mov r0, %2\n\t"                // load return value in r0
            "msr cpsr_flg, #0x13\n\t"       // switch to supervisor mode
            "msr spsr_all, %1\n\t"          // restore the spsr
            "movs pc, lr\n\t"               // jump to user code!

            : /* no output */
            : "r"(sp), "r"(spsr), "r" (ret)
        );

        asm volatile(
            "kerlabel:\n\t"                 // ucode: SWI n
            // TODO: acquire arguments to request here
            "mov r0, lr\n\t"                // save the user PC in r0
            "msr cpsr_flg, #0x1f\n\t"       // switch to system mode
            "stmfd sp!, {r0-r12, r14}\n\t"  // store uregs on ustack
            "mov r0, sp\n\t"                // move updated ustack pointer on r0
            "msr cpsr_flg, #0x13\n\t"       // switch to supervisor mode
            "ldmfd sp!, {r0-r12, r14}\n\t"  // unroll kregs from kstack
            "mov %0, r0\n\t"                // store ustack pointer
            "mrs %1, spsr_all\n\t"          // store the spsr

            : "=r"(sp), "=r"(spsr)
        );

        bwputstr(COM2, "kerxitExit\r\n");

        struct Request req;
        handle(&req);
    }
}

