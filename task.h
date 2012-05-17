#ifndef TASK_H
#define TASK_H 1

#include <stdbool.h>

struct TaskDescriptor {
    unsigned int id;
    unsigned int ret;
    unsigned int spsr;
    unsigned int *sp;

    bool active;
    int parent_task_id;
};

void initTaskSystem(void);

/*
 * Returns:
 *    non-negative: newly created task id
 *    -1: invalid priority
 *    -2: out of task descriptors
 *    -3: no more stack space
 */
int createTask(
    unsigned int priority,
    void (*code)(void),
    unsigned int stack_size,
    int parent_task_id
);

/* Returns false iff task_id is invalid */
bool exitTask(int task_id);

/* Returns the new active task or NULL if there are no tasks left on the system
 * (at which point the kernel must return cleanly to RedBoot */
struct TaskDescriptor *scheduleTask(void);

int getActiveTaskId(void);

/* Returns NULL on invalid task_id */
struct TaskDescriptor *getTask(int task_id);

#endif
