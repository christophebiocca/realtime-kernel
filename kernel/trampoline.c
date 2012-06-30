#include <debug.h>
#include <user/syscall.h>

__attribute__((naked, noreturn)) void trampoline(){
    asm volatile(
        "mov lr, pc\n\t"
        "bx ip\n\t"
    );
    Exit();
}
