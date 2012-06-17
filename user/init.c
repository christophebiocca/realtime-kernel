#include <bwio.h>

#include <ts7200.h>
#include <user/nameserver.h>
#include <user/syscall.h>
#include <user/init.h>

#include <user/string.h>
#include <user/mio.h>
#include <user/priorities.h>

static void childTask() {
    int tid = MyTid();
    int p_tid = MyParentTid();
    char* message = "Yo";
    bwprintf(COM2, "Task: %d, Parent: %d sending message.\r\n", tid, p_tid);
    char reply[3];
    int len = Send(p_tid, message, 3, reply, 3);
    bwprintf(COM2, "Task: %d got a reply of length %d (%s)\r\n", tid, len, reply);
    Exit();
}

void userModeTask(){
    for (int i = 0; i < 4; ++i) {
        bwprintf(COM2, "Created: %d\r\n", Create(
            (i < 2) ? 2 : 0,
            childTask
        ));
    }

    for (int i = 0; i < 4; ++i) {
        char msg[3];
        int tid = -3;
        int len = Receive(&tid, msg, 3);
        bwprintf(COM2, "First got a message of length %d (%s) from %d\r\n", len, msg, tid);
        Reply(tid, "Oy", 3);
    }

    bwputstr(COM2, "First: exiting\r\n");
    Exit();
}

/* Nameserver testing init tasks */

static void nsChildTask() {
    bwprintf(COM2, "Server ID: %d\r\n", WhoIs("server"));
    bwprintf(COM2, "ServerPrime ID: %d\r\n", WhoIs("serverprime"));
    Exit();
}

static void nsServerPrimeTask() {
    bwprintf(COM2, "[task %d] Registering \"serverprime\": %d\r\n", MyTid(), RegisterAs("serverprime"));
    Exit();
}

static void nsServerTask() {
    bwprintf(COM2, "[task %d] Registering \"server\": %d\r\n", MyTid(), RegisterAs("server"));
    Exit();
}

void nsUserModeTask(){
    bwprintf(COM2, "Nameserver: %d\r\n", Create(1, task_nameserver));
    bwprintf(COM2, "Server: %d\r\n", Create(2, nsServerTask));
    bwprintf(COM2, "Server Prime: %d\r\n", Create(2, nsServerPrimeTask));
    bwprintf(COM2, "Child: %d\r\n", Create(3, nsChildTask));

    Exit();
}

/* Send receive reply timings */
int g_receiver_tid;
typedef struct {
    int padding[10];
} Message64;

typedef struct {
    int padding;
} Message4;

typedef Message64 Message;

static void timeSenderTask(void) {
    Message request, reply;
    Send(
        g_receiver_tid,
        (char *) &request, sizeof(Message),
        (char *) &reply, sizeof(Message)
    );

    Exit();
}

static void timeReceiveTask(void) {
    int tid;
    Message msg;

    Receive(&tid, (char *) &msg, sizeof(Message));
    Reply(tid, (char *) &msg, sizeof(Message));

    Exit();
}

void timeUserModeTask(void) {
    Create(2, timeSenderTask);
    g_receiver_tid = Create(1, timeReceiveTask);

    Exit();
}

void listenerTask(void){
    while(1){
        bwprintf(COM2, "Listening...\r\n");
        AwaitEvent(0);
        bwprintf(COM2, "HOLY SHIT\r\n");
        *((unsigned int *)(VIC1_BASE + VIC_SOFTWARE_INT_CLEAR)) = 0x1;
    }
    Exit();
}

void annoyingTask(void){
    for(int i = 0; i<10; ++i){
        bwprintf(COM2, "I will interrupt now.\r\n");
        *((unsigned int *)(VIC1_BASE + VIC_SOFTWARE_INT)) = 0x1;
        bwprintf(COM2, "Done interrupting now.\r\n");
    }
    Exit();
}

void idlerTask(void){
    while(1)
        ;
}

void interrupterTask(void) {
    bwprintf(COM2, "Spawning listener child\r\n");
    Create(1, listenerTask);
    Create(2, annoyingTask);
    Create(31, idlerTask);
    Exit();
}

/* Monitor IO test */
void echoTask(void) {
    struct String s;

    mioInit();

    sinit(&s);
    sputstr(&s, "Hello world\r\n");
    mioPrint(&s);

    for (int i = 0; i < 100; ++i) {
        mioRead(&s);
        mioPrint(&s);
    }

    sinit(&s);
    sputstr(&s, "\r\nGoodbye world\r\n");
    mioPrint(&s);

    mioQuit();
    
    Exit();
}

void echoTaskInit(void) {
    Create(TASK_PRIORITY, echoTask);
    Create(31, idlerTask);
    Exit();
}

