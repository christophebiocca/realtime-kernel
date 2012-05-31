#ifndef	_KASSERT_H
#define	_KASSERT_H 1
#include "bwio.h"

/* void assert (int expression);

   If EXPRESSION is zero, print an error message and abort.  */

/* This prints an "Assertion failed" message and aborts.  */
static void __assert_fail (__const char *__assertion, __const char *__file,
        unsigned int __line, __const char *__function)
__attribute__((always_inline, noreturn));

static void __assert_fail (__const char *__assertion, __const char *__file,
        unsigned int __line, __const char *__function){
    bwprintf(COM2, "kernel: %s:%d: %s: Assertion `%s' failed\r\n", __file, __line,
        __function, __assertion);
    while(1){asm volatile("nop");};    
}

#define assert(expr)							\
  ((expr)								\
   ? (void)(0)						                \
   : __assert_fail (#expr, __FILE__, __LINE__, __func__))

#endif
