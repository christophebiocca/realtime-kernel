#ifndef CLOCK_H
#define CLOCK_H 1

void clockInitTask(void);

int Delay(int ticks);

// delays until time >= nticks
// returns the time when Reply() was called
// if the delay was cancelled, time is negative
int DelayUntil(int nticks);

// returns the current time
int Time(void);

// cancels a delay for task tid, see DelayUntil()
// returns 0 if a delay actually existed, non-zero otherwise
int DelayCancel(int tid);

int ClockQuit(void);

void clockWaiter(int taskToNotify);

#endif
