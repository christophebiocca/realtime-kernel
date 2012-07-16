#include <debug.h>

#include <user/clock.h>
#include <user/mio.h>
#include <user/priorities.h>
#include <user/string.h>
#include <user/syscall.h>
#include <user/tio.h>
#include <user/turnout.h>
#include <user/vt100.h>

#define CLEAR_SOLENOID  32
#define STRAIGHT        33
#define CURVE           34

static void turnoutCmd(char address, char cmd, TurnoutTable *turnout_state) {
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

    int mask = 1 << i;
    char *colour;
    char rep;

    // don't repeat commands
    if (cmd == CURVE) {
        if ((*turnout_state & mask) == 0) {
            colour = YELLOW;
            rep = 'C';
            *turnout_state |= mask;
        } else {
            return;
        }
    } else if (cmd == STRAIGHT) {
        if ((*turnout_state & mask) == 0) {
            return;
        } else {
            colour = GREEN;
            rep = 'S';
            *turnout_state &= ~mask;
        }
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

enum {
    CMD_TURNOUT_QUIT,
    CMD_TURNOUT_STRAIGHT,
    CMD_TURNOUT_CURVE,
    CMD_TURNOUT_QUERY
};

struct TurnoutMessage {
    char cmd;
    char address;

    char padding_DO_NOT_USE[2];
};

static void turnoutServer(void) {
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

    // set to all zeros (straight) so curve commands actually get sent
    TurnoutTable turnout_state = 0;
    for (int i = 1; i <= 18; ++i) {
        turnoutCmd(i, CURVE, &turnout_state);
    }

    for (int i = 153; i <= 156; ++i) {
        turnoutCmd(i, CURVE, &turnout_state);
    }

    bool quit = false;

    int tid;
    struct TurnoutMessage request;
    while (!quit) {
        int len = Receive(&tid, (char *) &request, sizeof(struct TurnoutMessage));
        assert(len == sizeof(struct TurnoutMessage));

        switch (request.cmd) {
            case CMD_TURNOUT_STRAIGHT:
                turnoutCmd(request.address, STRAIGHT, &turnout_state);
                break;

            case CMD_TURNOUT_CURVE:
                turnoutCmd(request.address, CURVE, &turnout_state);
                break;

            case CMD_TURNOUT_QUERY:
                // already responded
                break;

            case CMD_TURNOUT_QUIT:
                quit = true;
                break;
        }

        Reply(tid, (char *) &turnout_state, sizeof(int));
    }

    Exit();
}

static int g_turnout_server_tid;
void turnoutInit(void) {
    static_assert(sizeof(struct TurnoutMessage) == 4);
    static_assert(sizeof(TurnoutTable) == 4);
    g_turnout_server_tid = Create(TURNOUT_PRIORITY, turnoutServer);
}

void turnoutCurve(int address, TurnoutTable *tbl) {
    struct TurnoutMessage request;
    request.cmd = CMD_TURNOUT_CURVE;
    request.address = address;

    TurnoutTable table_state;
    Send(
        g_turnout_server_tid,
        (char *) &request, sizeof(struct TurnoutMessage),
        (char *) &table_state, sizeof(TurnoutTable)
    );

    *tbl = table_state;
}

void turnoutStraight(int address, TurnoutTable *tbl) {
    struct TurnoutMessage request;
    request.cmd = CMD_TURNOUT_STRAIGHT;
    request.address = address;

    TurnoutTable table_state;
    Send(
        g_turnout_server_tid,
        (char *) &request, sizeof(struct TurnoutMessage),
        (char *) &table_state, sizeof(TurnoutTable)
    );

    *tbl = table_state;
}

TurnoutTable turnoutQuery(void) {
    struct TurnoutMessage request;
    request.cmd = CMD_TURNOUT_QUERY;

    TurnoutTable table_state;
    Send(
        g_turnout_server_tid,
        (char *) &request, sizeof(struct TurnoutMessage),
        (char *) &table_state, sizeof(TurnoutTable)
    );

    return table_state;
}

void turnoutQuit(void) {
    struct TurnoutMessage request;
    request.cmd = CMD_TURNOUT_QUIT;

    int response;
    Send(
        g_turnout_server_tid,
        (char *) &request, sizeof(struct TurnoutMessage),
        (char *) &response, sizeof(int)
    );
}

