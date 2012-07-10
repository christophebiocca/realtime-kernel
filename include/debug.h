#ifndef DEBUG_H
#define DEBUG_H 1
#include <bwio.h>

#ifdef PRODUCTION

struct Perf {
   char nothing[0]; 
};

#define assert(expr)        (void)(0)
#define static_assert(e)    (void)(0)
#define trace(fmt, ...)     (void)(0)

#define TIMER_INIT(perf) (void)(0)
#define TIMER_START(perf) (void)(0)
#define TIMER_WORST(perf) (void)(0)
#define TIMER_SUM(perf) (void)(0)
#define TIMER_BEST(perf) (void)(0)
#define TIMER_PRINT(perf) (void)(0)

#else

/* void assert (int expression);

   If EXPRESSION is zero, print an error message and abort.  */

/* This prints an "Assertion failed" message and aborts.  */
static void __assert_fail (__const char *__assertion, __const char *__file,
        unsigned int __line, __const char *__function)
__attribute__((always_inline, noreturn));

static void __assert_fail (__const char *__assertion, __const char *__file,
        unsigned int __line, __const char *__function){
    bwsetfifo(COM2, 0);
    bwsetspeed(COM2, 115200);
    bwprintf(COM2, "kernel: %s:%d: %s: Assertion `%s' failed\r\n", __file, __line,
        __function, __assertion);
    while(1){asm volatile("nop");};
}

#define static_assert(e)                                                    \
    do {                                                                    \
        enum { assert_static__ = 1/(e) };                                   \
    } while (0)

#define assert(expr)                                                        \
  ((expr)                                                                   \
   ? (void)(0)                                                              \
   : __assert_fail (#expr, __FILE__, __LINE__, __func__))

/* void trace (char *fmt, ...)
 *
 * Print trace message to the screen */
#define trace(fmt, ...) bwprintf(                                           \
    COM2,                                                                   \
    "%s [%s:%d]: " fmt "\r\n",                                              \
    __func__,                                                               \
    __FILE__,                                                               \
    __LINE__,                                                               \
    ## __VA_ARGS__                                                          \
)

struct Perf {
    int start;
    int total;
};

#define TIMER_INIT(perf) do{perf.start = 0x7FFFFFFF; perf.total = 0;}while(0)

#define TIMER_START(perf) do{perf.start = *(TIMER4_VAL);}while(0)

#define TIMER_WORST(perf)                   \
do {                                        \
    int diff = *(TIMER4_VAL)-perf.start;    \
    if(diff > perf.total){                  \
        perf.total = diff;                  \
    }                                       \
} while(0)

#define TIMER_SUM(perf)                     \
do {                                        \
    int diff = *(TIMER4_VAL)-perf.start;    \
    if(diff > 0){                           \
        perf.total += diff;                 \
    }                                       \
} while(0)

#define TIMER_BEST(perf)                    \
do {                                        \
    int diff = *(TIMER4_VAL)-perf.start;    \
    if(diff < perf.total){                  \
        perf.total = diff;                  \
    }                                       \
} while(0)

#define TIMER_PRINT(perf)                   \
do {                                        \
    struct String s;                        \
    sinit(&s);                              \
    sputstr(&s, #perf ": ");                \
    sputint(&s, perf.total, 10);            \
    logS(&s);                               \
} while(0);


#endif // PRODUCTION

#endif // DEBUG_H
