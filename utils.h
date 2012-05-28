#ifndef UTILS_H
#define UTILS_H

// consider replacing with static moves memcpy
char *strncpy(char *dest, char *from, unsigned int max_len);
int strcmp(char *s1, char *s2);

void memcpy16(void *dest, void *src, unsigned int n);
void memcpy32(void *dest, void *src, unsigned int n);

#endif
