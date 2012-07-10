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
#define UNIQUE_BITS (31 - UNIQUE_OFFSET) // Highest bit is always 0.
#define NUM_UNIQUES (1 << UNIQUE_BITS)
#define UNIQUE_MASK ((NUM_UNIQUES-1) << UNIQUE_OFFSET)

#define taskIndex(tid)      ((tid & INDEX_MASK) >> INDEX_OFFSET)
#define taskPriority(tid)   ((tid & PRIORITY_MASK) >> PRIORITY_OFFSET)
#define taskUnique(tid)     ((tid & UNIQUE_MASK) >> UNIQUE_OFFSET)

// Does not range check inputs. Make sure you respect the below.
// 0 <= index < MAX_TASKS
// 0 <= priority < MAX_PRIORITY
// 0 <= unique < NUM_UNIQUES
#define makeTid(index, priority, unique)                                    \
    ((index << INDEX_OFFSET) |                                              \
        (priority << PRIORITY_OFFSET) |                                     \
        (unique << UNIQUE_OFFSET))

struct TaskDescriptor;

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
    int parent_task_id,
    int argc,
    int *argv
);
#define DEFAULT_STACK_SIZE 0x800

/* Remove the last scheduled task from the queues */
void exitCurrentTask(void);

/* Sets the return value of the task */
void setReturnValue(struct TaskDescriptor *td, int ret);

/* Gets the taskId */
int taskID(struct TaskDescriptor *td);

/* Gets the parent task id */
int parentID(struct TaskDescriptor *td);

/* Gets the stack pointer */
unsigned int *taskStackPointer(struct TaskDescriptor *td);

/* Gets the task's spsr */
unsigned int taskSPSR(struct TaskDescriptor *td);

/* Sets the stack pointer and spsr for a task */
void setTaskState(struct TaskDescriptor *td, unsigned int *sp, unsigned int spsr);

/* Returns the new active task or NULL if there are no tasks left on the system
 * (at which point the kernel must return cleanly to RedBoot) */
struct TaskDescriptor *scheduleTask(void);

int getActiveTaskId(void);

/* Returns NULL on invalid task_id */
struct TaskDescriptor *getTask(unsigned int task_id);

// Returns true iff the current task is the idle task.
bool idling();

// All the task runtimes.
void dumpTaskTimes(void);

#endif
