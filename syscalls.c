#include "syscalls.h"
#include "syscall_ids.h"

#define syscall_num(id) "swi " #id "\n\t"
#define syscall(name) syscall_num(name)

int Create(int priority, void (*code)()){
    if(priority > 32 || priority < 1){
        return -1; // Invalid priority.
    }
    register unsigned int priority_in_ret_out asm("r0") = priority;
    register void (*code_in)() asm("r1") = code;
    asm volatile(
        syscall(SYS_CREATE)
        : "+r"(priority_in_ret_out)
        : "r"(code_in));
    return priority_in_ret_out;
}

int MyTid(void){
    register unsigned int ret_out asm("r0");
    asm volatile(
        syscall(SYS_MY_TID)
        : "=r"(ret_out));
    return ret_out;
}

int MyParentTid(void){
    register unsigned int ret_out asm("r0");
    asm volatile(
        syscall(SYS_MY_PARENT_TID)
        : "=r"(ret_out));
    return ret_out;
}

void Pass(void){
    asm volatile(syscall(SYS_PASS));
}

void Exit(void){
    asm volatile(syscall(SYS_EXIT));
}
