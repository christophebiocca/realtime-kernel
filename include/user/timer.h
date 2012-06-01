#ifndef TIMER_H
#define TIMER_H 1

void timerInitTask(void);

int Delay(int ticks);
int Time(void);
int DelayUntil(int nticks);
int TimeQuit(void);

#endif
