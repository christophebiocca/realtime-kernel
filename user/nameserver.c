#include <bwio.h>
#include <stdbool.h>

#include <kernel/lib.h>
#include <kernel/task.h>

#include <user/nameserver.h>
#include <user/syscall.h>

#define NAMESERVER_ID   makeTid(2, 1, 2)
#define MAX_NAME_LEN    15

enum {
    SUCCESS = 0,
    GENERIC_ERROR = -1,
    NAMETABLE_FULL = -2,
    NO_SUCH_TASK = -2,
    NO_NAMESERVER = -3,
};

typedef struct {
    enum {
        REGISTER_AS,
        WHO_IS,
    } type:8;

    char name[MAX_NAME_LEN];
} NameserverRequest;

#define MAX_REGISTRATIONS   256

void task_nameserver(void) {
    struct {
        char name[MAX_NAME_LEN];
        int tid;
    } registrations[MAX_REGISTRATIONS];
    int num_registrations = 0;

    int sender;
    NameserverRequest request;
    int reply;

    while (true) {
        int bytes = Receive(&sender, (char *) &request, sizeof(NameserverRequest));
        if (bytes != sizeof(NameserverRequest)) {
            reply = GENERIC_ERROR;
            Reply(sender, (char *) &reply, sizeof(int));
            continue;
        }

        switch (request.type) {
            case REGISTER_AS:
                if (num_registrations >= MAX_REGISTRATIONS) {
                    reply = NAMETABLE_FULL;
                    Reply(sender, (char *) &reply, sizeof(int));
                    break;
                }

                int i;
                for (i = 0; i < num_registrations; ++i) {
                    if (strcmp(registrations[i].name, request.name) == 0) {
                        break;
                    }
                }

                if (i == num_registrations) {
                    ++num_registrations;
                    strncpy(registrations[i].name, request.name, MAX_NAME_LEN);
                }

                registrations[i].tid = sender;
                reply = SUCCESS;
                Reply(sender, (char *) &reply, sizeof(int));

                break;

            case WHO_IS: {
                bool has_replied = false;

                for (int i = 0; i < num_registrations; ++i) {
                    if (strcmp(registrations[i].name, request.name) == 0) {
                        reply = registrations[i].tid;
                        Reply(sender, (char *) &reply, sizeof(int));

                        has_replied = true;
                        break;
                    }
                }

                if (!has_replied) {
                    reply = NO_SUCH_TASK;
                    Reply(sender, (char *) &reply, sizeof(int));
                }

                break;
            }

            default:
                reply = GENERIC_ERROR;
                Reply(sender, (char *) &reply, sizeof(int));
        }
    }
}

int RegisterAs(char *name) {
    NameserverRequest request;
    int reply;

    request.type = REGISTER_AS;
    strncpy(request.name, name, MAX_NAME_LEN);
    int bytes = Send(
        NAMESERVER_ID,
        (char *) &request, sizeof(NameserverRequest),
        (char *) &reply, sizeof(int)
    );

    if (bytes != sizeof(int)) {
        return (bytes == -2) ? NO_NAMESERVER : GENERIC_ERROR;
    }

    return reply;
}

int WhoIs(char *name) {
    NameserverRequest request;
    int reply;

    request.type = WHO_IS;
    strncpy(request.name, name, MAX_NAME_LEN);
    int bytes = Send(
        NAMESERVER_ID,
        (char *) &request, sizeof(NameserverRequest),
        (char *) &reply, sizeof(int)
    );

    if (bytes != sizeof(int)) {
        return (bytes == -2) ? NO_NAMESERVER : GENERIC_ERROR;
    }

    return reply;
}
