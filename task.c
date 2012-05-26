#include "bwio.h"
#include "cpsr.h"
#include "task.h"

struct TaskDescriptor {
    unsigned int id;
    unsigned int spsr;
    unsigned int *sp;

    // Allows threading a linked list through the descriptors.
    struct TaskDescriptor* next;

    int parent_task_id;
};

static struct TaskDescriptor g_task_table[MAX_TASKS];
static int g_next_task_id;
static struct TaskDescriptor *g_active_task;

struct TaskQueue {
    struct TaskDescriptor *head;
    struct TaskDescriptor **tail;
};

struct TaskQueue g_task_queue[MAX_PRIORITY];

unsigned int g_task_queue_mask;

#define STACK_HIGH      0x300000
#define STACK_LOW       0x250000
static unsigned int *g_current_stack;

#define BRUJIN_SEQUENCE 0x077CB531U
static int LOOKUP[32];

static inline struct TaskDescriptor *queuePop(struct TaskQueue *queue, bool *empty){
    struct TaskDescriptor *desc = queue->head;
    queue->head = desc->next;
    desc->next = 0;
    if(!queue->head){
        queue->tail = &queue->head;
        if(empty){
            *empty=true;
        };
    }
    return desc;
}

static inline void queuePush(struct TaskQueue *queue, struct TaskDescriptor *t){
    *(queue->tail) = t;
    queue->tail = &(t->next);
}

static inline struct TaskDescriptor *priorityQueuePop(int priority){
    bool empty = false;
    struct TaskDescriptor* task = queuePop(&g_task_queue[priority], &empty);
    if(empty){
        g_task_queue_mask &= ~(1 << priority);
    }
    return task;
}

static inline void priorityQueuePush(int priority, struct TaskDescriptor *t){
    queuePush(&g_task_queue[priority], t);
    g_task_queue_mask |= 1 << priority;
}

void initTaskSystem(void (*initialTask)(void)) {

    for(unsigned int i = 0; i < 32; ++i){
        // Fill in the lookup table.
        LOOKUP[((1u << i) * BRUJIN_SEQUENCE) >> 27] = i;
    }

    // FIXME: write bzero and zero out g_task_table
    g_next_task_id = 0;
    g_active_task = 0;
    g_current_stack = (unsigned int *) STACK_HIGH;

    for (int i = 0; i < MAX_PRIORITY; ++i) {
        g_task_queue[i].head = 0;
        g_task_queue[i].tail = &g_task_queue[i].head;
    }
    g_task_queue_mask = 0;

    // Priority 0 because init task must run to completion before anything else
    // it may even issue multiple syscalls and must be guaranteed to run after
    // them.
    //
    // But for the purposes of assignment 1, where we need the ability to create
    // tasks with higher priority than init.
    int ret = createTask(1, initialTask, DEFAULT_STACK_SIZE, -1);
    if(ret < 0){
        bwprintf(COM2, "Fatal error, %d when setting up initial task");
    }
}

#define RELOCATION_CONSTANT 0x200000

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
        unsigned int stack_size, int parent_task_id) {
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
    t->spsr = UserMode | DisableIRQ | DisableFIQ;
    t->parent_task_id = parent_task_id;

    t->sp = g_current_stack - TRAP_FRAME_SIZE;
    t->next=0;
    // FIXME: we're going to be sticking stacks right next to each other
    g_current_stack -= (stack_size + TRAP_FRAME_SIZE);

    // FIXME: zero out the trap frame

    // see trap frame layout above TRAP_FRAME_SIZE
    *(t->sp) = ((unsigned int) code) + RELOCATION_CONSTANT;
    *(t->sp + 12) = (unsigned int) t->sp;  // for now, set frame pointer = stack pointer

    priorityQueuePush(priority, t);

    return t->id;
}

void exitCurrentTask(void){
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
        int priority = LOOKUP[(((unsigned int) g_task_queue_mask & -g_task_queue_mask)
            * BRUJIN_SEQUENCE) >> 27];
        return g_active_task = priorityQueuePop(priority);
    }
    return 0;
}
