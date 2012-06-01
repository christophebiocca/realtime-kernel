#ifndef INTERRUPTS_H
#define INTERRUPTS_H 1
// Initializes the system
void initInterruptSystem(void);

// Enqueues a task onto an interrupt table.
int awaitInterrupt(int interruptID);

// Reschedules any tasks that were waiting on interrupts to happen.
void handleInterrupt();
#endif
