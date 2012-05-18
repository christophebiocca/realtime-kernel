#include "stdbool.h"
#include "bwio.h"
#include "cpsr.h"
#include "user_task.h"
#include "task.h"
#include "syscall_ids.h"

struct Request {
    struct TaskDescriptor *task;
    unsigned int callID;
    unsigned int arg0;
    unsigned int arg1;
    unsigned int arg2;
    unsigned int arg3;
};

static void handle(struct Request *req){
    switch(req->callID){
        case SYS_CREATE:
            setReturnValue(req->task, createTask(req->arg0, (void (*)(void)) req->arg1,
                DEFAULT_STACK_SIZE, req->task->id));
            break;
        case SYS_MY_TID:
            setReturnValue(req->task, req->task->id);
            break;
        case SYS_MY_PARENT_TID:
            setReturnValue(req->task, req->task->parent_task_id);
            break;
        case SYS_PASS:
            break;
        case SYS_EXIT:
            exitTask(req->task->id);
            break;
        default:
            bwprintf(COM2, "Invalid call %u!\r\n", req->callID);
            break;
    }
}

int main(){
    unsigned int jump_addr;

    bwsetfifo(COM2, false);

    asm volatile(
        "ldr %0, =kerlabel" // Load the label from a literal pool
    : "=r"(jump_addr));
    *((unsigned int *)0x28) = jump_addr + 0x200000;

    initTaskSystem(&userModeTask);

    struct TaskDescriptor* active;
    for(active = scheduleTask(); active; active = scheduleTask()){

        register unsigned int *sp asm("r0") = active->sp;
        register unsigned int spsr asm("r1") = active->spsr;

        asm volatile(
            "stmfd sp!, {r2-r12, r14}\n\t"  // save kregs on kstack
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
            "ldmfd sp!, {r2-r12, r14}\n\t"  // unroll kregs from kstack
            : "+r"(sp), "+r"(spsr)
        );

        active->sp = sp;
        active->spsr = spsr;
        unsigned int call = *(((unsigned int *) *sp) - 1) & 0x00FFFFFF;
        struct Request req = {
            .task = active,
            .callID = call,
            .arg0 = *(sp+1),
            .arg1 = *(sp+2),
            .arg2 = *(sp+3),
            .arg3 = *(sp+4)
        };
        handle(&req);
    }
}

