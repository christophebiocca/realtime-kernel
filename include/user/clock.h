#ifndef CLOCK_H
#define CLOCK_H 1

void clockInitTask(void);

int Delay(int ticks);
int Time(void);
int DelayUntil(int nticks);
int ClockQuit(void);

void clockWaiter(int taskToNotify);

#endif
