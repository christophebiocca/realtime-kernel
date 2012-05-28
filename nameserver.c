#include <bwio.h>
#include <stdbool.h>
#include <utils.h>

#include <nameserver.h>
#include <syscalls.h>
#include <task.h>

#define NAMESERVER_ID   makeTid(2, 1, 2)
#define MAX_NAME_LEN    32

enum {
    SUCCESS = 0,
    GENERIC_ERROR = -1,
    NAMETABLE_FULL = -2,
    NO_SUCH_TASK = -2,
    NO_NAMESERVER = -3,
};

// FIXME: make this 32 bytes for fast mempcpy
typedef struct {
    enum {
        REGISTER_AS,
        WHO_IS,
    } type;

    char name[MAX_NAME_LEN];
    int reply;
} NameserverMessage;

#define MAX_REGISTRATIONS   256

void task_nameserver(void) {
    struct {
        char name[MAX_NAME_LEN];
        int tid;
    } registrations[MAX_REGISTRATIONS];
    int num_registrations = 0;

    int sender;
    NameserverMessage msg;

    while (true) {
        int bytes = Receive(&sender, (char *) &msg, sizeof(NameserverMessage));
        if (bytes != sizeof(NameserverMessage)) {
            msg.reply = GENERIC_ERROR;
            Reply(sender, (char *) &msg, sizeof(NameserverMessage));
            continue;
        }

        switch (msg.type) {
            case REGISTER_AS:
                if (num_registrations >= MAX_REGISTRATIONS) {
                    msg.reply = NAMETABLE_FULL;
                    Reply(sender, (char *) &msg, sizeof(NameserverMessage));
                    break;
                }

                int i;
                for (i = 0; i < num_registrations; ++i) {
                    if (strcmp(registrations[i].name, msg.name) == 0) {
                        break;
                    }
                }

                if (i == num_registrations) {
                    ++num_registrations;
                    strncpy(registrations[i].name, msg.name, MAX_NAME_LEN);
                }

                registrations[i].tid = sender;
                msg.reply = SUCCESS;
                Reply(sender, (char *) &msg, sizeof(NameserverMessage));

                break;

            case WHO_IS: {
                bool has_replied = false;

                for (int i = 0; i < num_registrations; ++i) {
                    if (strcmp(registrations[i].name, msg.name) == 0) {
                        msg.reply = registrations[i].tid;
                        Reply(sender, (char *) &msg, sizeof(NameserverMessage));

                        has_replied = true;
                        break;
                    }
                }

                if (!has_replied) {
                    msg.reply = NO_SUCH_TASK;
                    Reply(sender, (char *) &msg, sizeof(NameserverMessage));
                }

                break;
            }

            default:
                msg.reply = GENERIC_ERROR;
                Reply(sender, (char *) &msg, sizeof(NameserverMessage));
        }
    }
}

int RegisterAs(char *name) {
    NameserverMessage request, reply;

    request.type = REGISTER_AS;
    strncpy(request.name, name, MAX_NAME_LEN);
    int bytes = Send(
        NAMESERVER_ID,
        (char *) &request, sizeof(NameserverMessage),
        (char *) &reply, sizeof(NameserverMessage)
    );

    if (bytes != sizeof(NameserverMessage)) {
        return (bytes == -2) ? NO_NAMESERVER : GENERIC_ERROR;
    }

    return reply.reply;
}

int WhoIs(char *name) {
    NameserverMessage request, reply;

    request.type = WHO_IS;
    strncpy(request.name, name, MAX_NAME_LEN);
    int bytes = Send(
        NAMESERVER_ID,
        (char *) &request, sizeof(NameserverMessage),
        (char *) &reply, sizeof(NameserverMessage)
    );

    if (bytes != sizeof(NameserverMessage)) {
        return (bytes == -2) ? NO_NAMESERVER : GENERIC_ERROR;
    }

    return reply.reply;
}
