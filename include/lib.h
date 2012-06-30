#ifndef UTILS_H
#define UTILS_H

void libinit(void);

// consider replacing with static moves memcpy
char *strncpy(char *dest, char *from, unsigned int max_len);
int strcmp(const char *s1, const char *s2);

// Copies memory 16 or 32 bytes at a time.
// Special case exists for n == 4 (1 word) and 0.
void memcpy16(void *dest, void *src, unsigned int n);
void memcpy32(void *dest, void *src, unsigned int n);

// sets memory 16 bytes at a time
// Special case exists for n == 4 (1 word) and 0.
void memset16(void *dest, unsigned int word, unsigned int n);

int countLeadingZeroes(int num);

#endif
