#ifndef TASK_INTERNAL_H
#define TASK_INTERNAL_H 1

#include <kernel/task.h>

struct TaskDescriptor {
    unsigned int id;
    unsigned int spsr;
    unsigned int *sp;

    unsigned int rcvBlockedHead:8;
    unsigned int rcvBlockedTail:8;

    // Allows threading a linked list through the descriptors.
    unsigned int next:8;

    enum {
        TSK_READY,
        TSK_RECEIVE_BLOCKED,
        TSK_REPLY_BLOCKED,
        TSK_SEND_BLOCKED,
        TSK_ZOMBIE,
    } status:8;

    int parent_task_id;
};

// pointer to the currently active task
extern struct TaskDescriptor *g_active_task;

extern struct TaskDescriptor g_task_table[MAX_TASKS];

struct TaskDescriptor *receiveQueuePop(struct TaskDescriptor *queue, bool *empty);
void receiveQueuePush(struct TaskDescriptor *queue, struct TaskDescriptor *t);
bool receiveQueueEmpty(struct TaskDescriptor *queue);

struct TaskDescriptor *priorityQueuePop(int priority);
void priorityQueuePush(int priority, struct TaskDescriptor *t);

#endif
