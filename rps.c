#include <bwio.h>
#include <syscalls.h>
#include <user_task.h>
#include <nameserver.h>

typedef enum {
    SIGN_UP,
    PLAY_ROCK,
    PLAY_PAPER,
    PLAY_SCISSORS,
    QUIT
} rpsRequest;

// we support 32 matches going at once
#define CONCURRENT_MATCHES  10
#define LOG(...)    bwprintf(COM2, "[rps-server] " __VA_ARGS__)
static void rpsServer(void) {
    static int awaiting_tid = 0; // client awaiting a challenger
    static struct {
        int tid1;
        rpsRequest tid1_state;
        int tid2;
        rpsRequest tid2_state;
    } current_matches[CONCURRENT_MATCHES];
    static int current_head = 0, current_tail = 0;

    LOG("Reticulating splines...");
    int ret = RegisterAs("rps");

    if (!ret) {
        LOG("Error registering with name server: %d\r\n", ret);
        Exit();
    }

    int tid, request;
    Receive(&tid, (char *) &request, sizeof(int));

    switch (request) {
        case SIGN_UP:
            LOG("Sign up request by %d\r\n", tid);

            if (!awaiting_tid) {
                LOG("We need to wait for a challenger...\r\n");
                awaiting_tid = tid;
            } else {
                LOG("Found match: %d vs %d\r\n", awaiting_tid, tid);

                if (current_tail == current_head) {
                    LOG(
                        "Unfortunately, we ran out of space to track this "
                        "match... dropping it silently!\r\n"
                    );
                }

                current_matches[current_tail].tid1 = awaiting_tid;
                current_matches[current_tail].tid1_state = SIGN_UP;
                current_matches[current_tail].tid2 = tid;
                current_matches[current_tail].tid2_state = SIGN_UP;
                ++current_tail;

                // FIXME: figure out void replies
                Reply(awaiting_tid, (char*) &request, sizeof(int));
                Reply(tid, (char*) &request, sizeof(int));
            }
            break;
    }

    Exit();
}

void rpsUserModeTask(void) {
    Create(1, task_nameserver);
    Create(2, rpsServer);
    //Create(2, rpsClient);
    //Create(2, rpsClient);
    Exit();
}
