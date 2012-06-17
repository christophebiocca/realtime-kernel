#ifndef USER_MIO_H
#define USER_MIO_H  1

#include <user/string.h>

void mioInit(void);

void mioPrint(struct String *s);
void mioRead(struct String *s);
void mioQuit(void);

#endif /* USER_MIO_H */
