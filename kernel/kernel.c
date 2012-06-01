#include <stdbool.h>

#include <bwio.h>
#include <cpsr.h>
#include <ts7200.h>

#include <kernel/ipc.h>
#include <kernel/task.h>
#include <user/syscall.h>
#include <user/init.h>
#include <lib.h>

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
                    taskID(task)
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

        default:
            bwprintf(COM2, "Invalid call %u!\r\n", syscall_id);
            break;
    }
}

int main(void) {
    libinit();
    static volatile unsigned int hardware_pc;
    hardware_pc = 0;

    bwsetfifo(COM2, false);

    register unsigned int swi_addr asm("r0");
    register unsigned int h_int_addr asm("r1");
    register unsigned int hardware_pc_addr asm("r2") = (unsigned int) &hardware_pc;
    asm volatile(
        "ldr %0, =kerlabel\n\t" // Load the label from a literal pool
        "ldr %1, =intentry\n\t" // Same
        "msr cpsr_c, #0xd2\n\t" // Switch to irq
        "mov r13, %2\n\t"       // Remember where to set hardware interrupt lr.
        "msr cpsr_c, #0xd3\n\t" // Switch to supervisor
    : "=r"(swi_addr), "=r"(h_int_addr) : "r"(hardware_pc_addr));
    *((unsigned int *)0x28) = swi_addr;
    *((unsigned int *)0x38) = h_int_addr;
    *((unsigned int *)0x08) = 0xe59ff018;
    *((unsigned int *)0x18) = 0xe59ff018;


    // Allow interrupt 0
    *((unsigned int *)(VIC1_BASE + VIC_INT_ENABLE)) = 0x1;

    initTaskSystem(&interrupterTask);

    struct TaskDescriptor* active;
    for(active = scheduleTask(); active; active = scheduleTask()) {

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
            "str lr, [r13]\n\t"             // Shove a task's pc into hardware

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

        if(hardware_pc) {
            // Fix the task PC (top of trap frame) with the PC captured by
            // hardware interrupt.
            //
            // We subtract 12 to reload the 3 instructions in the pipeline when
            // the interrupt occurred.
            *sp = hardware_pc - 12;

            bwprintf(COM2, "FIXME: Hardware int. %x\r\n", hardware_pc);
            *((unsigned int *)(VIC1_BASE + VIC_SOFTWARE_INT_CLEAR)) = 0xffffffff;

            // reset so we don't confuse all kernel entries as hardware
            // interrupts
            hardware_pc = 0;
        } else {
            unsigned int call = *(((unsigned int *) *sp) - 1) & 0x00FFFFFF;
            dispatchSyscall(active, call, sp + 1);
        }
    }
    return 0;
}

