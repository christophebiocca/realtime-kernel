#include <stdbool.h>

#include <user/log.h>
#include <user/clock.h>
#include <user/mio.h>
#include <user/priorities.h>
#include <user/string.h>
#include <user/syscall.h>
#include <user/vt100.h>

#define CMD_LOG     0
#define CMD_DUMP    1
#define CMD_ASSOC   2
#define CMD_QUIT    3

#define BACKLOG_SIZE    40
#define ASSOC_SIZE      10
static void logServer(void) {
    struct String backlog[BACKLOG_SIZE];
    int backlog_head = 0;
    int backlog_tail = 0;

    struct {
        int tid;
        struct String name;
    } assocs[ASSOC_SIZE];
    int assocs_size = 0;

    int current_row = -1;
    const int ROW_MAX = SCROLL_BOTTOM - LOG_ROW;

    struct String request;
    int sender_tid;

    bool quitting = false;
    while (!quitting) {
        Receive(&sender_tid, (char *) &request, sizeof(struct String));
        Reply(sender_tid, (char *) 0, 0);

        switch (stag(&request)) {
            case CMD_LOG: {
                int who = -1;
                for (int i = 0; i < assocs_size; ++i) {
                    if (assocs[i].tid == sender_tid) {
                        who = i;
                        break;
                    }
                }

                struct String s;
                sinit(&s);
                if (who >= 0) {
                    sconcat(&s, &assocs[who].name);
                    sputc(&s, '@');
                    sputint(&s, Time(), 10);
                    sputstr(&s, ": ");
                }
                sconcat(&s, &request);

                sinit(&backlog[backlog_tail]);
                sconcat(&backlog[backlog_tail], &s);

                backlog_tail = (backlog_tail + 1) % BACKLOG_SIZE;
                if (backlog_tail == backlog_head) {
                    backlog_head = (backlog_head + 1) % BACKLOG_SIZE;
                }

                struct String output;
                sinit(&output);
                sputstr(&output, CURSOR_SAVE);
                vtPos(&output, LOG_ROW + current_row, 1);
                sputstr(&output, "\r\n");
                sconcat(&output, &s);
                sputstr(&output, CURSOR_RESTORE);
                mioPrint(&output);

                if (current_row < ROW_MAX) {
                    ++current_row;
                }

                break;
            }

            case CMD_ASSOC:
                assert(assocs_size < ASSOC_SIZE);
                assocs[assocs_size].tid = sender_tid;
                
                sinit(&assocs[assocs_size].name);
                sconcat(&assocs[assocs_size].name, &request);

                ++assocs_size;
                break;

            case CMD_DUMP:
                for (int i = backlog_head; i != backlog_tail;
                        i = (i + 1) % BACKLOG_SIZE) {
                    mioPrint(&backlog[i]);
                }
                break;

            case CMD_QUIT:
                quitting = true;
                break;

            default:
                assert(0);
        }
    }

    Exit();
}

static int g_log_server_tid;
void logInit(void) {
    g_log_server_tid = Create(LOG_PRIORITY, logServer);
}

void logAssoc(const char *name) {
    struct String s;

    sinit(&s);
    ssettag(&s, CMD_ASSOC);
    sputstr(&s, name);
    Send(
        g_log_server_tid,
        (char *) &s, sizeof(struct String),
        (char *) 0, 0
    );
}

void logC(const char *msg) {
    struct String s;
    sinit(&s);
    sputstr(&s, msg);
    logS(&s);
}

void logS(struct String *s) {
    ssettag(s, CMD_LOG);
    Send(
        g_log_server_tid,
        (char *) s, sizeof(struct String),
        (char *) 0, 0
    );
}

void logQuit(void) {
    struct String s;
    sinit(&s);
    ssettag(&s, CMD_QUIT);

    Send(
        g_log_server_tid,
        (char *) &s, sizeof(struct String),
        (char *) 0, 0
    );
}
