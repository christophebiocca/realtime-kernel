#ifndef STRING_H
#define STRING_H    1

#include <debug.h>

// FIXME: we can't make struct String opaque because we need to be able to
// allocate it on the stack.

// The definition of this structure is carefully chosen:
//      - offset needs to have enough bits to
//        represent 0 - STRING_MAX_BUFFER_LEN
//      - The size of the structure is a multiple of 16 bytes (4 words) so a
//        String can be efficiently memcpy16()'d.
// tag is included to allow a String to be used with Send/Receive.
#define STRING_MAX_BUFFER_LEN   63
struct String {
    unsigned int tag : 2;
    unsigned int offset : 6;
    char buffer[STRING_MAX_BUFFER_LEN];
};

// (re-)initializes a string
static inline void sinit(struct String *s) {
    // make sure String can be memcpy16()'d
    static_assert(sizeof(struct String) % 16 == 0);
    s->offset = 0;
}

static inline unsigned int slen(struct String *s) {
    return s->offset;
}

static inline char *sbuffer(struct String *s) {
    return s->buffer;
}

static inline unsigned int stag(struct String *s) {
    return s->tag;
}

static inline void ssettag(struct String *s, unsigned int tag) {
    assert(tag < 4); // only have 2 bits
    s->tag = tag;
}

static inline void sputc(struct String *s, char c) {
    assert(s->offset < STRING_MAX_BUFFER_LEN);
    s->buffer[s->offset++] = c;
}

static inline void sputstr(struct String *s, char *str) {
    for (; *str != '\0'; ++str) {
        sputc(s, *str);
    }
}

#endif
