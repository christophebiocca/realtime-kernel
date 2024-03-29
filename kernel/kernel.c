#include <stdbool.h>

#include <bwio.h>
#include <cpsr.h>
#include <ts7200.h>

#include <kernel/ipc.h>
#include <kernel/task.h>
#include <kernel/interrupts.h>
#include <user/syscall.h>
#include <user/init.h>
#include <lib.h>

#include "task_internal.h"

static void dispatchSyscall(struct TaskDescriptor *task,
        unsigned int syscall_id, unsigned int *args) {

    switch(syscall_id) {
        case SYS_CREATE:
            setReturnValue(
                task,
                createTask(
                    args[0],
                    (void (*)(void)) args[1],
                    DEFAULT_STACK_SIZE,
                    taskID(task),
                    args[2],
                    ((int *) args[3])
                )
            );
            break;

        case SYS_MY_TID:
            setReturnValue(task, taskID(task));
            break;

        case SYS_MY_PARENT_TID:
            setReturnValue(task, parentID(task));
            break;

        case SYS_PASS:
            break;

        case SYS_EXIT:
            exitCurrentTask();
            break;

        case SYS_SEND:
            ipcSend(args[0]);
            break;

        case SYS_RECEIVE:
            ipcReceive();
            break;

        case SYS_REPLY:
            ipcReply(args[0]);
            break;

        case SYS_AWAIT_EVENT:
            setReturnValue(task, awaitInterrupt(args[0]));
            break;

        default:
            bwprintf(COM2, "Invalid call %u!\r\n", syscall_id);
            break;
    }
}

#define DUMPSAVE asm volatile (                                             \
    "stmfd sp!, {r0-r12}\n\t"                                               \
    "msr cpsr_c, #0xd0\n\t"                                                 \
    "mov r0, sp\n\t"                                                        \
    "msr cpsr_c, #0xd3\n\t"                                                 \
    "stmfd sp!, {r0}\n\t"                                                   \
)

#define DUMPR(r) do {                                                       \
    bwputstr(COM2, #r ": ");                                                \
    asm volatile (                                                          \
        "mov r0, #1\n\t"                                                    \
        "ldmfd sp!, {r1}\n\t"                                               \
        "bl bwputr\n\t"                                                     \
        ::: "r0", "r1"                                                      \
    );                                                                      \
    bwputstr(COM2, "\r\n");                                                 \
} while(0)

static void undefined_instr(void) {
    DUMPSAVE;

    unsigned int *lr;
    asm volatile("mov %0, lr\n\t" : "=r"(lr));

    bwprintf(COM2, "\r\n*** UNDEFINED INSTRUCTION: 0x%x\r\n", lr);
    bwprintf(COM2, "Task ID: %d\r\n", getActiveTaskId());

    DUMPR(usr_sp);
    DUMPR(r12);
    DUMPR(r11);
    DUMPR(r10);
    DUMPR(r9);
    DUMPR(r8);
    DUMPR(r7);
    DUMPR(r6);
    DUMPR(r5);
    DUMPR(r4);
    DUMPR(r3);
    DUMPR(r2);
    DUMPR(r1);
    DUMPR(r0);

    unsigned int *pc = lr;
    while ((pc[0] & 0xff000000) != 0xff000000) {
        --pc;
    }
    char *fn = ((char *) pc) - (pc[0] & 0x00ffffff);
    bwprintf(COM2, "*** UNDEFINED INSTRUCTION: %s(): %x\r\n", fn, lr);

    // loop forever
    while (1);
}

static void abort_prefetch(void) {
    DUMPSAVE;

    unsigned int *lr;
    asm volatile("mov %0, lr\n\t" : "=r"(lr));

    bwprintf(COM2, "\r\n*** ABORT PREFETCH: 0x%x\r\n", lr);
    bwprintf(COM2, "Task ID: %d\r\n", getActiveTaskId());

    DUMPR(usr_sp);
    DUMPR(r12);
    DUMPR(r11);
    DUMPR(r10);
    DUMPR(r9);
    DUMPR(r8);
    DUMPR(r7);
    DUMPR(r6);
    DUMPR(r5);
    DUMPR(r4);
    DUMPR(r3);
    DUMPR(r2);
    DUMPR(r1);
    DUMPR(r0);

    unsigned int *pc = lr;
    while ((pc[0] & 0xff000000) != 0xff000000) {
        --pc;
    }
    char *fn = ((char *) pc) - (pc[0] & 0x00ffffff);
    bwprintf(COM2, "*** PREFECTH ABORT: %s(): %x\r\n", fn, lr);

    // loop forever
    while (1);
}

static void abort_data(void) {
    DUMPSAVE;

    unsigned int *lr;
    asm volatile("mov %0, lr\n\t" : "=r"(lr));

    bwprintf(COM2, "\r\n*** ABORT DATA: 0x%x\r\n", lr);
    bwprintf(COM2, "Task ID: %d\r\n", getActiveTaskId());

    DUMPR(usr_sp);
    DUMPR(r12);
    DUMPR(r11);
    DUMPR(r10);
    DUMPR(r9);
    DUMPR(r8);
    DUMPR(r7);
    DUMPR(r6);
    DUMPR(r5);
    DUMPR(r4);
    DUMPR(r3);
    DUMPR(r2);
    DUMPR(r1);
    DUMPR(r0);

    unsigned int *pc = lr;
    while ((pc[0] & 0xff000000) != 0xff000000) {
        --pc;
    }
    char *fn = ((char *) pc) - (pc[0] & 0x00ffffff);
    bwprintf(COM2, "*** DATA ABORT: %s(): %x\r\n", fn, lr);

    // loop forever
    while (1);
}

int main(void) {
    libinit();
    static volatile unsigned int hardware_pc_spsr[2];
    hardware_pc_spsr[0] = 0;
    hardware_pc_spsr[1] = 0;

    *TIMER4_CRTL = TIMER4_ENABLE;

    // set up memory bank
    *((unsigned int *) BANK_UNDEFINED_INSTR) = BANK_JUMP_INSTR;
    *((unsigned int *) BANK_SOFTWARE_INT) = BANK_JUMP_INSTR;
    *((unsigned int *) BANK_ABORT_PREFETCH) = BANK_JUMP_INSTR;
    *((unsigned int *) BANK_ABORT_DATA) = BANK_JUMP_INSTR;
    *((unsigned int *) BANK_IRQ) = BANK_JUMP_INSTR;

    *((unsigned int *) (BANK_UNDEFINED_INSTR + BANK_JUMP)) = (unsigned int) &undefined_instr;
    *((unsigned int *) (BANK_ABORT_PREFETCH + BANK_JUMP)) = (unsigned int) &abort_prefetch;
    *((unsigned int *) (BANK_ABORT_DATA + BANK_JUMP)) = (unsigned int) &abort_data;

    register unsigned int swi_addr asm("r0");
    register unsigned int h_int_addr asm("r1");
    register unsigned int hardware_pc_spsr_addr asm("r2") =
        (unsigned int) &hardware_pc_spsr;
    asm volatile(
        "ldr %0, =kerlabel\n\t" // Load the label from a literal pool
        "ldr %1, =intentry\n\t" // Same
        "msr cpsr_c, #0xd2\n\t" // Switch to irq
        "mov r13, %2\n\t"       // Remember where to set hardware interrupt lr.
        "msr cpsr_c, #0xd3\n\t" // Switch to supervisor
    : "=r"(swi_addr), "=r"(h_int_addr) : "r"(hardware_pc_spsr_addr));
    *((unsigned int *) (BANK_SOFTWARE_INT + BANK_JUMP)) = swi_addr;
    *((unsigned int *) (BANK_IRQ + BANK_JUMP)) = h_int_addr;

    *((unsigned int *) (VIC1_BASE + VIC_INT_ENABLE_CLEAR)) = 0xffffffff;
    *((unsigned int *) (VIC2_BASE + VIC_INT_ENABLE_CLEAR)) = 0xffffffff;

    initInterruptSystem();
    initTaskSystem(trainTaskInit);

    struct TaskDescriptor* active;
    for(active = scheduleTask(); active && (!idling() || awaitingInterrupts());
            active = scheduleTask()){

        TIMER_START(active->runtime);

        unsigned int *sp = taskStackPointer(active);
        unsigned int spsr = taskSPSR(active);

        register unsigned int *sp_reg asm("r0") = sp;
        register unsigned int spsr_reg asm("r1") = spsr;
        asm volatile(
            "stmfd sp!, {r2-r12, r14}\n\t"  // save kregs on kstack
            "ldmfd %0!, {r14}\n\t"          // Get the stored pc
            "msr spsr, %1\n\t"              // set active's spsr
            "msr cpsr_c, #0xdf\n\t"         // Switch to system mode
            "mov sp, %0\n\t"                // Set sp.
            "ldmfd sp!, {r0-r12, r14}\n\t"  // Unroll task registers.
            "msr cpsr_c, #0xd3\n\t"         // Switch to supervisor mode
            "movs pc, r14\n\t"              // JUMP!

            "intentry:\n\t"                 // Land here on hardward interrupts
            "str lr, [r13]\n\t"             // Shove a task's pc into hardware_pc_spsr[0]
            "mrs lr, spsr\n\t"              // Put SPSR into LR
            "str lr, [r13, #4]\n\t"         // Shove SPSR into hardware_pc_spsr[1]

            "kerlabel:\n\t"                 // ucode: SWI n
            "msr cpsr_c, #0xdf\n\t"         // Switch to system mode
            "stmfd sp!, {r0-r12, r14}\n\t"  // Store user registers.
            "mov %0, sp\n\t"                // Save user's stack pointer
            "msr cpsr_c, #0xd3\n\t"         // Switch to supervisor mode
            "stmfd %0!, {r14}\n\t"          // Store the task's pc on it's stack
            "mrs %1, spsr\n\t"              // Obtain activity's spsr
            "ldmfd sp!, {r2-r12, r14}\n\t"  // unroll kregs from kstack
            : "+r"(sp_reg), "+r"(spsr_reg)
        );

        sp = sp_reg;
        spsr = spsr_reg;

        setTaskState(active, sp, spsr);

        TIMER_SUM(active->runtime);

        if (hardware_pc_spsr[0]) {
            setTaskState(active, sp, hardware_pc_spsr[1]);

            // Fix the task PC (top of trap frame) with the PC captured by
            // hardware interrupt.
            //
            // From the ARM documentation for MOV rd, rn:
            //      If rd is PC, then the real value moved is PC = rn + 8
            // 
            // Since we want to retry the instruction that got interrupted, we
            // store PC = LR - 8.
            *sp = hardware_pc_spsr[0] - 4;

            handleInterrupt();

            // reset so we don't confuse all kernel entries as hardware
            // interrupts
            hardware_pc_spsr[0] = 0;
            hardware_pc_spsr[1] = 0;
        } else {
            unsigned int call = *(((unsigned int *) *sp) - 1) & 0x00FFFFFF;
            dispatchSyscall(active, call, sp + 1);
        }
    }

    *((unsigned int *) (VIC1_BASE + VIC_INT_ENABLE_CLEAR)) = 0xffffffff;
    *((unsigned int *) (VIC2_BASE + VIC_INT_ENABLE_CLEAR)) = 0xffffffff;

    dumpTaskTimes();

    return 0;
}

