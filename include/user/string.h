#ifndef STRING_H
#define STRING_H    1

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
void sinit(struct String *s);

void sputc(struct String *s, char c);
void sputstr(struct String *s, char *str);

#endif
