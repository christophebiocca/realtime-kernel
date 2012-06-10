#include <debug.h>
#include <user/string.h>

void sinit(struct String *s) {
    // make sure String can be memcpy16()'d
    static_assert(sizeof(struct String) % 16 == 0);

    s->offset = 0;
}

void sputc(struct String *s, char c) {
    assert(s->offset < STRING_MAX_BUFFER_LEN);

    s->buffer[s->offset++] = c;
}

void sputstr(struct String *s, char *str) {
    for (; *str != '\0'; ++str) {
        sputc(s, *str);
    }
}
