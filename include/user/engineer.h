#ifndef USER_ENGINEER_H
#define USER_ENGINEER_H 1

#include <stdbool.h>


extern volatile int engineerID;
void initEngineer(int trainID);
int engineerCreate(int trainID);

#endif
