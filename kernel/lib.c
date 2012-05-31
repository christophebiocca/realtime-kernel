#include <bwio.h>
#include <lib.h>

char *strncpy(char *dest, char *from, unsigned int max_len) {
    unsigned int i = 0;

    while(i < max_len && (dest[i] = from[i])) {
        ++i;
    }

    if (i == max_len && max_len > 0) {
        dest[i - 1] = '\0';
    }

    return dest;
}

int strcmp(char *s1, char *s2) {
    while (*s1 && *s2 && *s1++ == *s2++)
        ;
    return *s1 - *s2;
}

void memcpy16(void *dest, void *src, unsigned int n) {
    register void *rdest asm("r0") = dest;
    register void *rsrc asm("r1") = src;
    register unsigned int rn asm("r2") = n;

    asm volatile (
        "memcpy16_loop:\n\t"
        "ldmia %1!, {r3-r6}\n\t"    // unroll 4 words of src
        "stmia %0!, {r3-r6}\n\t"    // load those into dest
        "subs %2, %2, #16\n\t"      // subtract 16 from n and set COND
        "bgt memcpy16_loop\n\t"     // top of loop on greater than zero
        : /* no outputs */
        : "r"(rdest), "r"(rsrc), "r"(rn)
        : "r3", "r4", "r5", "r6"
    );
}

void memcpy32(void *dest, void *src, unsigned int n) {
    register void *rdest asm("r0") = dest;
    register void *rsrc asm("r1") = src;
    register unsigned int rn asm("r2") = n;

    asm volatile (
        "memcpy32_loop:\n\t"
        "ldmia %1!, {r3-r9,r14}\n\t"    // unroll 4 words of src
        "stmia %0!, {r3-r9,r14}\n\t"    // load those into dest
        "subs %2, %2, #32\n\t"          // subtract 32 from n and set COND
        "bgt memcpy32_loop\n\t"         // top of loop on greater than zero
        : /* no outputs */
        : "r"(rdest), "r"(rsrc), "r"(rn)
        : "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r14"
    );
}
