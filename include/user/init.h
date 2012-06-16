#ifndef _USER_TASK_H
#define _USER_TASK_H

#include <user/k3task.h>

void userModeTask();
void nsUserModeTask();
void timeUserModeTask();
void echoTaskInit();

// rock paper scissors
void rpsUserModeTask();

void interrupterTask(void);

#endif
