#ifndef TASK_H
#define TASK_H 1

#include <stdbool.h>

struct TaskDescriptor {
    unsigned int id;
    unsigned int spsr;
    unsigned int *sp;

    int parent_task_id;
};

void initTaskSystem(void (*initialTask)(void));

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
#define DEFAULT_STACK_SIZE 0x400

/* Remove the last scheduled task from the queues */
void exitCurrentTask(void);

/* Sets the return value of the task */
void setReturnValue(struct TaskDescriptor *td, int ret);

/* Returns the new active task or NULL if there are no tasks left on the system
 * (at which point the kernel must return cleanly to RedBoot */
struct TaskDescriptor *scheduleTask(void);

int getActiveTaskId(void);

/* Returns NULL on invalid task_id */
struct TaskDescriptor *getTask(unsigned int task_id);

#endif
