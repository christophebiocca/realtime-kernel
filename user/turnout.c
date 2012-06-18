#include <debug.h>

#include <user/mio.h>
#include <user/tio.h>
#include <user/string.h>
#include <user/turnout.h>
#include <user/vt100.h>

#define CLEAR_SOLENOID  32
#define STRAIGHT        33
#define CURVE           34

void turnoutInit(void) {
    struct String s;

    sinit(&s);
    vtPos(&s, SWITCH_ROW, 1);
    sputstr(&s, "+--------+--------+--------+\r\n");
    mioPrint(&s);
    sinit(&s);
    sputstr(&s, "|   1: # |   2: # |   3: # |\r\n");
    mioPrint(&s);
    sinit(&s);
    sputstr(&s, "|   4: # |   5: # |   6: # |\r\n");
    mioPrint(&s);
    sinit(&s);
    sputstr(&s, "|   7: # |   8: # |   9: # |\r\n");
    mioPrint(&s);
    sinit(&s);
    sputstr(&s, "|  10: # |  11: # |  12: # |\r\n");
    mioPrint(&s);
    sinit(&s);
    sputstr(&s, "|  13: # |  14: # |  15: # |\r\n");
    mioPrint(&s);
    sinit(&s);
    sputstr(&s, "|  16: # |  17: # |  18: # |\r\n");
    mioPrint(&s);
    sinit(&s);
    sputstr(&s, "+--------+--------+--------+--------+\r\n");
    mioPrint(&s);
    sinit(&s);
    sputstr(&s, "| 153: # | 154: # | 155: # | 156: # |\r\n");
    mioPrint(&s);
    sinit(&s);
    sputstr(&s, "+--------+--------+--------+--------+");
    mioPrint(&s);

    for (int i = 1; i <= 18; ++i) {
        turnoutStraight(i);
    }

    for (int i = 153; i <= 156; ++i) {
        turnoutStraight(i);
    }
}

static void turnoutCmd(char address, char cmd, char *colour, char rep) {
    int i = 0, row = 0, col = 0;

    if (address >= 1 && address <= 18) {
        i = address - 1;
        row = i / 3 + 1;
        col = 9 * (i % 3) + 8;
    } else if (address >= 153 && address <= 156) {
        i = address - 135;
        row = 8;
        col = 9 * (address - 153) + 8;
    } else {
        assert(0);
    }

    struct String s;
    sinit(&s);
    sputc(&s, cmd);
    sputc(&s, address);
    sputc(&s, CLEAR_SOLENOID);
    tioPrint(&s);

    sinit(&s);
    sputstr(&s, CURSOR_SAVE);
    vtPos(&s, SWITCH_ROW + row, col);
    sputstr(&s, colour);
    sputc(&s, rep);
    sputstr(&s, RESET);
    sputstr(&s, CURSOR_RESTORE);
    mioPrint(&s);
}

void turnoutCurve(int address) {
    turnoutCmd(address, CURVE, YELLOW, 'C');
}

void turnoutStraight(int address) {
    turnoutCmd(address, STRAIGHT, GREEN, 'S');
}
