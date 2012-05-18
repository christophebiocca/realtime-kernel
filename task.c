#include "bwio.h"
#include "cpsr.h"
#include "task.h"

#define MAX_TASKS       100

static struct TaskDescriptor g_task_table[MAX_TASKS];
static int g_next_task_id;
static int g_active_task_id;

#define MAX_PRIORITY    32
#define MAX_QUEUE_SIZE  16
struct TaskQueue {
    struct TaskDescriptor *buffer[MAX_QUEUE_SIZE];
    unsigned int head;
    unsigned int tail;
} g_task_queue[MAX_PRIORITY];
unsigned int g_task_queue_mask;

#define STACK_HIGH      0x300000
#define STACK_LOW       0x250000
static unsigned int *g_current_stack;

#define BRUJIN_SEQUENCE 0x077CB531U
static int LOOKUP[32];

void initTaskSystem(void (*initialTask)(void)) {

    for(unsigned int i = 0; i < 32; ++i){
        // Fill in the lookup table.
        LOOKUP[((1u << i) * BRUJIN_SEQUENCE) >> 27] = i;
    }

    // FIXME: write bzero and zero out g_task_table
    g_next_task_id = 0;
    g_active_task_id = -1;
    g_current_stack = (unsigned int *) STACK_HIGH;

    for (int i = 0; i < MAX_PRIORITY; ++i) {
        // FIXME: zero out buffer
        g_task_queue[i].head = 0;
        g_task_queue[i].tail = 0;
    }
    g_task_queue_mask = 0;

    // Priority 0 because init task must run to completion before anything else
    // it may even issue multiple syscalls and must be guaranteed to run after
    // them.
    int ret = createTask(0, initialTask, DEFAULT_STACK_SIZE, -1);
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

    struct TaskDescriptor *t = &g_task_table[g_next_task_id];
    t->id = g_next_task_id++;
    t->spsr = UserMode | DisableIRQ | DisableFIQ;
    t->active = true;
    t->parent_task_id = parent_task_id;

    t->sp = g_current_stack - TRAP_FRAME_SIZE;
    // FIXME: we're going to be sticking stacks right next to each other
    g_current_stack -= (stack_size + TRAP_FRAME_SIZE);

    // FIXME: zero out the trap frame

    // see trap frame layout above TRAP_FRAME_SIZE
    *(t->sp) = ((unsigned int) code) + RELOCATION_CONSTANT;
    *(t->sp + 12) = (unsigned int) t->sp;  // for now, set frame pointer = stack pointer

    struct TaskQueue *queue = &g_task_queue[priority];
    queue->buffer[queue->tail++] = t;
    queue->tail %= MAX_QUEUE_SIZE;

    g_task_queue_mask |= 1 << priority;

    return t->id;
}

bool exitTask(unsigned int task_id) {
    if (task_id >= MAX_TASKS) {
        return false;
    }

    g_task_table[task_id].active = false;
    return true;
}

void exitCurrentTask(void){
    int priority = LOOKUP[(((unsigned int) g_task_queue_mask & -g_task_queue_mask)
        * BRUJIN_SEQUENCE) >> 27];
    struct TaskQueue *queue = &g_task_queue[priority];
    queue->tail = (queue->tail - 1) % MAX_QUEUE_SIZE; // Drop the tail of the queue.
    if(queue->tail == queue->head){
        // And if the queue is empty, clear the ready bit of the queue.
        g_task_queue_mask &= ~(1 << priority);
    }
}

void setReturnValue(struct TaskDescriptor *td, int ret){
    *(td->sp + 1) = ret;
}

struct TaskDescriptor *scheduleTask(void){
    struct TaskDescriptor *desc;
    while(g_task_queue_mask){
        int priority = LOOKUP[(((unsigned int) g_task_queue_mask & -g_task_queue_mask)
            * BRUJIN_SEQUENCE) >> 27];
        struct TaskQueue *queue = &g_task_queue[priority];
        do {
            desc = queue->buffer[queue->head];
            queue->head = (queue->head+1) % MAX_QUEUE_SIZE;
            if(desc->active){
                queue->buffer[queue->tail] = desc;
                queue->tail = (queue->tail+1) % MAX_QUEUE_SIZE;
                return desc;
            }
        } while(queue->head != queue->tail);
        // We emptied the queue.
        g_task_queue_mask &= ~(1 << priority);
    }
    return 0;
}
