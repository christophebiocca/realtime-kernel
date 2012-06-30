#include <bwio.h>
#include <debug.h>
#include <user/syscall.h>

#define syscall_num(id) "swi " #id "\n\t"
#define syscall(name) syscall_num(name)

int Create(int priority, void (*code)()){
    return CreateArgs(priority, code, 0);
}

int CreateArgs(int priority, void (*code)(), int argc, ...){
    if(priority >= 32 || priority < 0){
        return -1; // Invalid priority.
    }
    int argv[4];
    assert(argc <= 4);
    va_list vlist;
    va_start(vlist, argc);
    for(int i = 0; i < argc; ++i){
        argv[i] = va_arg(vlist, int);
    }
    va_end(vlist);
    register unsigned int priority_in_ret_out asm("r0") = priority;
    register void (*code_in)() asm("r1") = code;
    register int argc_in asm("r2") = argc;
    register int *argv_in asm("r3") = argv;
    asm volatile(
        syscall(SYS_CREATE)
        : "+r"(priority_in_ret_out)
        : "r"(code_in), "r"(argc_in), "r"(argv_in));
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

__attribute__((noreturn)) void Exit(void){
    asm volatile(syscall(SYS_EXIT));
    while(1); // To shut gcc up about returns.
}

int Send(int tid, char *msg, int msglen, char *reply, int replylen){
    /*
    *((volatile int *) 0x80810064) = 0;
    *((volatile int *) 0x80810064) = 0x0100;
    */

    register int tid_in_len_out asm("r0") = tid;
    register char *msg_in asm("r1") = msg;
    register int msglen_in asm("r2") = msglen;
    register char *reply_in asm("r3") = reply;
    register int replylen_in asm("r4") = replylen;
    asm volatile(
        syscall(SYS_SEND)
        : "+r"(tid_in_len_out)
        : "r"(msg_in), "r"(msglen_in), "r"(reply_in), "r"(replylen_in));

    /*
    unsigned int timer_now = *((volatile int *) 0x80810060);
    *((volatile int *) 0x80810064) = 0;
    bwprintf(COM2, "%d ticks have elapsed\r\n", timer_now);
    */

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
        syscall(SYS_REPLY)
        : "+r"(tid_in_len_out)
        : "r"(msg_in), "r"(msglen_in));
    return (int) tid_in_len_out;
}


int AwaitEvent(int eventid){
    register unsigned int event_in_ret_out asm("r0") = eventid;
    asm volatile(
        syscall(SYS_AWAIT_EVENT)
        : "+r"(event_in_ret_out));
    return (int) event_in_ret_out;
}
