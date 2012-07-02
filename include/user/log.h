#ifndef USER_LOG_H
#define USER_LOG_H  1

#include <user/string.h>

void logInit(void);
void logQuit(void);

void logAssoc(const char *name);
void logC(const char *s);
void logS(struct String *s);

#endif
