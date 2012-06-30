#include <bwio.h>
#include <lib.h>

#define BRUJIN_SEQUENCE 0x077CB531U
static int LOOKUP[32];
void libinit(void){
    for(unsigned int i = 0; i < 32; ++i){
        // Fill in the lookup table.
        LOOKUP[((1u << i) * BRUJIN_SEQUENCE) >> 27] = i;
    }
}

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

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2 && *s1++ == *s2++)
        ;
    return *s1 - *s2;
}

void memcpy16(void *dest, void *src, unsigned int n) {
    if(!n)return;
    if (n == 4) {
        *((unsigned int *) dest) = *((unsigned int *) src);
    } else {
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
}

void memcpy32(void *dest, void *src, unsigned int n) {
    if(!n)return;
    if (n == 4) {
        *((unsigned int *) dest) = *((unsigned int *) src);
    } else {
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
}

void memset16(void *dest, unsigned int word, unsigned int n) {
    if (!n) return;

    if (n == 4) {
        *((unsigned int *) dest) = word;
    } else {
        register void *rdest asm("r0") = dest;
        register unsigned int rn asm("r1") = n;
        register unsigned int rword asm("r2") = word;

        asm volatile (
            "mov r3, %2\n\t"                // Copy word into 3 other registers
            "mov r4, %2\n\t"
            "mov r5, %2\n\t"
            "memset16_loop:\n\t"
            "stmia %0!, {%2,r3-r5}\n\t"     // load them into dest
            "subs %1, %1, #16\n\t"          // subtract 16 from n and set COND
            "bgt memset16_loop\n\t"         // top of loop on greater than zero
            : /* no outputs */
            : "r"(rdest), "r"(rn), "r"(rword)
            : "r3", "r4", "r5"
        );
    }
}

int countLeadingZeroes(int num){
    return LOOKUP[(((unsigned int) num & -num) * BRUJIN_SEQUENCE) >> 27];
}
