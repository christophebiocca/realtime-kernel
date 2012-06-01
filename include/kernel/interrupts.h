#ifndef INTERRUPTS_H
#define INTERRUPTS_H 1
#include <stdbool.h>

// Initializes the system
void initInterruptSystem(void);

// Enqueues a task onto an interrupt table.
int awaitInterrupt(int interruptID);

// Reschedules any tasks that were waiting on interrupts to happen.
void handleInterrupt();

// Returns whether there is anyone waiting for an interrupt right now.
bool awaitingInterrupts(void);
#endif
