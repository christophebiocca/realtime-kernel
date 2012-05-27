#include <utils.h>

char *strncpy(char *dest, char *from, unsigned int max_len) {
    unsigned int i = 0;

    while(i < max_len && (dest[i] = from[i])) {
        ++i;
    }

    if (i == max_len && max_len > 0) {
        dest[i - 1] = '\0';
    }

    return dest;
}

int strcmp(char *s1, char *s2) {
    while (*s1 && *s2 && *s1++ == *s2++)
        ;
    return *s1 - *s2;
}
