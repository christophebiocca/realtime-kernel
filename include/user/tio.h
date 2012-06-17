#ifndef USER_TIO_H
#define USER_TIO_H  1

#include <user/string.h>

void tioInit(void);

void tioPrint(struct String *s);
void tioRead(struct String *s);

void tioQuit(void);

#endif /* USER_TIO_H */

