#ifndef SYSCALL_H
#define SYSCALL_H 1

#define SYS_CREATE 0
#define SYS_MY_TID 1
#define SYS_MY_PARENT_TID 2
#define SYS_PASS 3
#define SYS_EXIT 4
#define SYS_SEND 5
#define SYS_RECEIVE 6
#define SYS_REPLY 7
#define SYS_AWAIT_EVENT 8

// Creates a new task with the given priority and code.
// Returns positive tid if success
// -1 for invalid priority
// -2 Not enough task descriptors
int Create(int priority, void (*code)());

// Same as above except that it accepts 0-4 arguments to pass
// to the spawned function. They must have 1 word widths.
int CreateArgs(int priority, void (*code)(), int argc, ...);

// Always returns the current task's tid.
int MyTid(void);

// Returns the parent task's tid, if it still exists.
// Otherwise the behavior is implementation dependent.
int MyParentTid(void);

// Schedule this at the back of its priority queue.
void Pass(void);

// Stop executing this task. Never returns.
void Exit(void) __attribute__((noreturn));

// Sends a message to another task, only returns on error or success.
// -1: impossible task id
// -2: task doesn't exist
// -3: TODO: I don't understand that one
// Otherwise, the length of the reply.
int Send(int tid, char *msg, int msglen, char *reply, int replylen);

// Receives a message from anyone.
// Returns the length of the message.
int Receive(int *tid, char *msg, int msglen);

// Replies to a message that has been sent.
// 0: success
// -1: impossible task id
// -2: task doesn't exist
// -3: task isn't waiting for a reply.
int Reply(int tid, char *reply, int replylen);

// Waits until the correct interrupt fires.
// 0: interrupt has fired
// -1: invalid eventid
int AwaitEvent(int eventid);

#endif
