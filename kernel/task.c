#include <bwio.h>
#include <cpsr.h>
#include <lib.h>

#include <kernel/task.h>
#include <kernel/trampoline.h>
#include "task_internal.h"
#include <debug.h>

struct TaskDescriptor g_task_table[MAX_TASKS];
static int g_next_task_id;
struct TaskDescriptor *g_active_task;

// Use 0 to signify that there is no such value.
struct TaskQueue {
    unsigned int head:8;
    unsigned int tail:8;
};

static struct TaskQueue g_task_queue[MAX_PRIORITY];
unsigned int g_task_queue_mask;

#define STACK_HIGH      0x300000
#define STACK_LOW       0x250000
static unsigned int *g_current_stack;

#define DEFN_QUEUE(name, type, head_field, tail_field)                      \
struct TaskDescriptor *name ## Pop(struct type *queue, bool *empty){        \
    struct TaskDescriptor *desc = &g_task_table[queue->head_field];         \
    queue->head_field = desc->next;                                         \
    desc->next = 0;                                                         \
    if(!queue->head_field){                                                 \
        queue->tail_field = queue->head_field;                              \
        if(empty){                                                          \
            *empty=true;                                                    \
        };                                                                  \
    }                                                                       \
    return desc;                                                            \
}                                                                           \
                                                                            \
void name ## Push(struct type *queue, struct TaskDescriptor *t){            \
    char next = taskIndex(t->id);                                           \
    if(queue->tail_field){                                                  \
        g_task_table[queue->tail_field].next = next;                        \
    } else {                                                                \
        queue->head_field = next;                                           \
    }                                                                       \
    queue->tail_field = next;                                               \
}                                                                           \
                                                                            \
bool name ## Empty(struct type *queue){                                     \
    if(!queue->head_field){                                                 \
        return true;                                                        \
    }                                                                       \
    return false;                                                           \
}

DEFN_QUEUE(queue, TaskQueue, head, tail)
DEFN_QUEUE(receiveQueue, TaskDescriptor, rcvBlockedHead, rcvBlockedTail);

struct TaskDescriptor *priorityQueuePop(int priority) {
    bool empty = false;
    struct TaskDescriptor* task = queuePop(&g_task_queue[priority], &empty);
    if(empty){
        g_task_queue_mask &= ~(1 << priority);
    }
    assert(task->status == TSK_READY);
    return task;
}

void priorityQueuePush(int priority, struct TaskDescriptor *t) {
    assert(t->status == TSK_READY);
    queuePush(&g_task_queue[priority], t);
    g_task_queue_mask |= 1 << priority;
}

void initTaskSystem(void (*initialTask)(void)) {

    g_next_task_id = 1;
    g_active_task = 0;
    g_current_stack = (unsigned int *) STACK_HIGH;

    for (int i = 0; i < MAX_PRIORITY; ++i) {
        g_task_queue[i].head = 0;
        g_task_queue[i].tail = 0;
    }
    g_task_queue_mask = 0;

    for (int i = 1; i < MAX_TASKS; ++i){
        struct TaskDescriptor *t = g_task_table + i;
        t->id = 0;
        t->spsr = 0;
        t->next = 0;
        t->sp = 0;
        t->parent_task_id = 0;
        t->status = TSK_ZOMBIE;
        t->rcvBlockedHead = 0;
        t->rcvBlockedTail = 0;
    }

    // Priority 0 because init task must run to completion before anything else
    // it may even issue multiple syscalls and must be guaranteed to run after
    // them.
    int ret = createTask(0, initialTask, DEFAULT_STACK_SIZE, -1, 0, 0);
    if(ret < 0){
        bwprintf(COM2, "Fatal error, %d when setting up initial task\r\n");
    }
}

// The following code MUST be kept in sync with the trap frame unrolling
// during context switch. Trap frame layout:
//      sp + 0 = r15 (pc)
//      sp + 1 = r0
//      ...
//      sp + 12 = r11 (fp)
//      sp + 13 = r12
//      sp + 14 = r14 (lr)
#define TRAP_FRAME_SIZE 15

int createTask(unsigned int priority, void (*code)(void),
        unsigned int stack_size, int parent_task_id, int argc, int *argv) {
    (void) argc; (void) argv;
    if (priority >= MAX_PRIORITY) {
        return -1;
    }

    if (g_next_task_id >= MAX_TASKS) {
        return -2;
    }

    if (((unsigned int) (g_current_stack - stack_size - TRAP_FRAME_SIZE)) < STACK_LOW) {
        return -3;
    }

    int unique, index;
    unique = index = g_next_task_id++;
    struct TaskDescriptor *t = &g_task_table[index];
    t->id = makeTid(index, priority, unique);
    t->spsr = UserMode;
    t->parent_task_id = parent_task_id;

    t->sp = g_current_stack - TRAP_FRAME_SIZE;
    t->next=0;
    t->status = TSK_READY;
    // FIXME: we're going to be sticking stacks right next to each other
    g_current_stack -= (stack_size + TRAP_FRAME_SIZE);

    // FIXME: zero out the trap frame

    // see trap frame layout above TRAP_FRAME_SIZE
    *(t->sp) = ((unsigned int) trampoline);
    for(int i = 0; i < argc; ++i){
        *(t->sp + 1 + i) = argv[i];
    }
    *(t->sp + 12) = (unsigned int) t->sp;  // for now, set frame pointer = stack pointer
    *(t->sp + 13) = ((unsigned int) code); // Store code to jump to in ip.

    priorityQueuePush(priority, t);

    TIMER_INIT(t->runtime);

    return t->id;
}

int getActiveTaskId(void) {
    return g_active_task->id;
}

void exitCurrentTask(void){
    g_active_task->status = TSK_ZOMBIE;
    g_active_task = 0;
}

void setReturnValue(struct TaskDescriptor *td, int ret){
    *(td->sp + 1) = ret;
}

int taskID(struct TaskDescriptor *td){
    return td->id;
}

int parentID(struct TaskDescriptor *td){
    return td->parent_task_id;
}

unsigned int *taskStackPointer(struct TaskDescriptor *td){
    return td->sp;
}

unsigned int taskSPSR(struct TaskDescriptor *td){
    return td->spsr;
}

void setTaskState(struct TaskDescriptor *td, unsigned int *sp, unsigned int spsr){
    td->sp = sp;
    td->spsr = spsr;
}

struct TaskDescriptor *scheduleTask(void){
    if(g_active_task){
        priorityQueuePush(taskPriority(g_active_task->id), g_active_task);
    }

    if(g_task_queue_mask){
        int priority = countLeadingZeroes(g_task_queue_mask);

        g_active_task = priorityQueuePop(priority);
        assert(g_active_task->status == TSK_READY);
        assert((unsigned int) g_active_task->sp >= STACK_LOW &&
                (unsigned int) g_active_task->sp <= STACK_HIGH);

        return g_active_task;
    }

    return 0;
}

bool idling(){
    return taskPriority(g_active_task->id) == 31;
}

void dumpTaskTimes(void){
    #ifndef PRODUCTION
    for(int i = 1; i < g_next_task_id; ++i){
        unsigned int id = g_task_table[i].id;
        bwprintf(COM2, "Task#%d(%d %d %d), time: %d\r\n",
            id,
            (id & INDEX_MASK),
            (id & PRIORITY_MASK) >> PRIORITY_OFFSET,
            (id & UNIQUE_MASK) >> UNIQUE_OFFSET,
            g_task_table[i].runtime.total);
    }
    #endif
}
