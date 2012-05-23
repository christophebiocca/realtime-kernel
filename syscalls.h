// Creates a new task with the given priority and code.
// Returns positive tid if success
// -1 for invalid priority
// -2 Not enough task descriptors
int Create(int priority, void (*code)());

// Always returns the current task's tid.
int MyTid(void);

// Returns the parent task's tid, if it still exists.
// Otherwise the behavior is implementation dependent.
int MyParentTid(void);

// Schedule this at the back of its priority queue.
void Pass(void);

// Stop executing this task. Never returns.
void Exit(void);

// Sends a message to another task, only returns on error or success.
// -1: impossible task id
// -2: task doesn't exist
// -3: TODO: I don't understand that one
// Otherwise, the length of the reply.
int Send(int tid, char *msg, int msglen, char *reply, int replylen);

// Receives a message from anyone.
// Returns the length of the message.
int Receive(int *tid, char *msg, int msglen);
