#ifndef TASK_H
#define TASK_H 1

#include <stdbool.h>

#define TASKS_BITS 8
#define PRIORITY_BITS 5

#define MAX_TASKS (1<<TASKS_BITS)
#define MAX_PRIORITY (1<<PRIORITY_BITS)

#define INDEX_OFFSET 0
#define INDEX_MASK ((MAX_TASKS-1) << INDEX_OFFSET)
#define PRIORITY_OFFSET TASKS_BITS
#define PRIORITY_MASK ((MAX_PRIORITY-1) << PRIORITY_OFFSET)
#define UNIQUE_OFFSET (TASKS_BITS + PRIORITY_BITS)
#define UNIQUE_BITS (32 - UNIQUE_OFFSET)
#define NUM_UNIQUES (1 << UNIQUE_BITS)
#define UNIQUE_MASK ((NUM_UNIQUES-1) << UNIQUE_OFFSET)

static inline int taskIndex(int tid){
    return (INDEX_MASK & tid) >> INDEX_OFFSET;
}

static inline int taskPriority(int tid){
    return (PRIORITY_MASK & tid) >> PRIORITY_OFFSET;
}

static inline int taskUnique(int tid){
    return (UNIQUE_MASK & tid) >> UNIQUE_OFFSET;
}

// Does not range check inputs. Make sure you respect the below.
// 0 <= index < MAX_TASKS
// 0 <= priority < MAX_PRIORITY
// 0 <= unique < NUM_UNIQUES
static inline int makeTid(int index, int priority, int unique){
    return
        (index << INDEX_OFFSET) |
        (priority << PRIORITY_OFFSET) |
        (unique << UNIQUE_OFFSET);
}

struct TaskDescriptor {
    unsigned int id;
    unsigned int spsr;
    unsigned int *sp;

    // Allows threading a linked list through the descriptors.
    struct TaskDescriptor* next;

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

/* Gets the taskId */
int taskID(struct TaskDescriptor *td);

/* Gets the parent task id */
int parentID(struct TaskDescriptor *td);

/* Returns the new active task or NULL if there are no tasks left on the system
 * (at which point the kernel must return cleanly to RedBoot */
struct TaskDescriptor *scheduleTask(void);

int getActiveTaskId(void);

/* Returns NULL on invalid task_id */
struct TaskDescriptor *getTask(unsigned int task_id);

#endif
