#include "syscalls.h"
#include "syscall_ids.h"

#define syscall_num(id) "swi " #id "\n\t"
#define syscall(name) syscall_num(name)

int Create(int priority, void (*code)()){
    if(priority >= 32 || priority < 0){
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

int Send(int tid, char *msg, int msglen, char *reply, int replylen){
    register int tid_in_len_out asm("r0") = tid;
    register char *msg_in asm("r1") = msg;
    register int msglen_in asm("r2") = msglen;
    register char *reply_in asm("r3") = reply;
    register int replylen_in asm("r4") = replylen;
    asm volatile(
        syscall(SYS_SEND)
        : "+r"(tid_in_len_out)
        : "r"(msg_in), "r"(msglen_in), "r"(reply_in), "r"(replylen_in));
    return tid_in_len_out;
}

int Receive(int *tid, char *msg, int msglen){
    register int *tid_in_len_out asm("r0") = tid;
    register char *msg_in asm("r1") = msg;
    register int msglen_in asm("r2") = msglen;
    asm volatile(
        syscall(SYS_RECEIVE)
        : "+r"(tid_in_len_out)
        : "r"(msg_in), "r"(msglen_in));
    return (int) tid_in_len_out;
}

int Reply(int tid, char *reply, int replylen){
    register int tid_in_len_out asm("r0") = tid;
    register char *msg_in asm("r1") = reply;
    register int msglen_in asm("r2") = replylen;
    asm volatile(
        syscall(SYS_RECEIVE)
        : "+r"(tid_in_len_out)
        : "r"(msg_in), "r"(msglen_in));
    return (int) tid_in_len_out;
}
