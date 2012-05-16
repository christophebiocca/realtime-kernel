#ifndef _CPSR_H
#define _CPSR_H 1
#define NegativeMask (1<<31)
#define ZeroMask (1<<30)
#define CarryMask (1<<29)
#define OverflowMask (1<<28)

#define ModeMask 0x1f
    #define UserMode 0x10
    #define FIQMode 0x11
    #define IRQMode 0x12
    #define SVCMode 0x13
    #define ABTMode 0x17
    #define UNDMode 0x1b
    #define SYSMode 0x1f
    
#define DisableIRQ (1<<7)
#define DisableFIQ (1<<6)

#define Thumb (1<<5)

#endif
