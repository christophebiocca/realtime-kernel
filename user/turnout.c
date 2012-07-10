#include <debug.h>

#include <user/mio.h>
#include <user/tio.h>
#include <user/clock.h>
#include <user/string.h>
#include <user/turnout.h>
#include <user/vt100.h>

#define CLEAR_SOLENOID  32
#define STRAIGHT        33
#define CURVE           34

void turnoutInit(void) {
    const char *rows[] = {
        "+--------+--------+--------+",
        "|   1: # |   2: # |   3: # |",
        "|   4: # |   5: # |   6: # |",
        "|   7: # |   8: # |   9: # |",
        "|  10: # |  11: # |  12: # |",
        "|  13: # |  14: # |  15: # |",
        "|  16: # |  17: # |  18: # |",
        "+--------+--------+--------+--------+",
        "| 153: # | 154: # | 155: # | 156: # |",
        "+--------+--------+--------+--------+"
    };

    struct String s;

    for (unsigned int i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        sinit(&s);
        sputstr(&s, CURSOR_SAVE);
        vtPos(&s, SWITCH_ROW + i, 1);
        sputstr(&s, rows[i]);
        sputstr(&s, CURSOR_RESTORE);
        mioPrint(&s);
    }

    for (int i = 1; i <= 18; ++i) {
        turnoutCurve(i,0);
    }

    for (int i = 153; i <= 156; ++i) {
        turnoutCurve(i,0);
    }
}

static void turnoutCmd(char address, char cmd, char *colour, char rep, TurnoutTable *tbl) {
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

    if(tbl){
        int mask = 1 << i;
        if(rep == 'C'){
            *tbl |= mask;
        } else {
            *tbl &= ~mask;
        }
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

void turnoutCurve(int address, TurnoutTable *tbl) {
    turnoutCmd(address, CURVE, YELLOW, 'C', tbl);
}

void turnoutStraight(int address, TurnoutTable *tbl) {
    turnoutCmd(address, STRAIGHT, GREEN, 'S', tbl);
}
