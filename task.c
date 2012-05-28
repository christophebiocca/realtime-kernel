#include "bwio.h"
#include "cpsr.h"
#include "task.h"
#include <kassert.h>

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

static struct TaskDescriptor g_task_table[MAX_TASKS];
static int g_next_task_id;
static struct TaskDescriptor *g_active_task;

// Use 0 to signify that there is no such value.
struct TaskQueue {
    unsigned int head:8;
    unsigned int tail:8;
};

struct TaskQueue g_task_queue[MAX_PRIORITY];

unsigned int g_task_queue_mask;

#define STACK_HIGH      0x300000
#define STACK_LOW       0x250000
static unsigned int *g_current_stack;

#define BRUJIN_SEQUENCE 0x077CB531U
static int LOOKUP[32];

#define DEFN_QUEUE(name, type, head_field, tail_field)\
static inline struct TaskDescriptor *name ## Pop(struct type *queue, bool *empty){\
    struct TaskDescriptor *desc = &g_task_table[queue->head_field];\
    queue->head_field = desc->next;\
    desc->next = 0;\
    if(!queue->head_field){\
        queue->tail_field = queue->head_field;\
        if(empty){\
            *empty=true;\
        };\
    }\
    return desc;\
}\
\
static inline void name ## Push(struct type *queue, struct TaskDescriptor *t){\
    char next = taskIndex(t->id);\
    if(queue->tail_field){\
        g_task_table[queue->tail_field].next = next;\
    } else {\
        queue->head_field = next;\
    }\
    queue->tail_field = next;\
}\
\
static inline bool name ## Empty(struct type *queue){\
    if(!queue->head_field){\
        return true;\
    }\
    return false;\
}

DEFN_QUEUE(queue, TaskQueue, head, tail)
DEFN_QUEUE(receiveQueue, TaskDescriptor, rcvBlockedHead, rcvBlockedTail);

static inline struct TaskDescriptor *priorityQueuePop(int priority){
    bool empty = false;
    struct TaskDescriptor* task = queuePop(&g_task_queue[priority], &empty);
    if(empty){
        g_task_queue_mask &= ~(1 << priority);
    }
    assert(task->status == TSK_READY);
    return task;
}

static inline void priorityQueuePush(int priority, struct TaskDescriptor *t){
    assert(t->status == TSK_READY);
    queuePush(&g_task_queue[priority], t);
    g_task_queue_mask |= 1 << priority;
}

void initTaskSystem(void (*initialTask)(void)) {

    for(unsigned int i = 0; i < 32; ++i){
        // Fill in the lookup table.
        LOOKUP[((1u << i) * BRUJIN_SEQUENCE) >> 27] = i;
    }

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
    int ret = createTask(0, initialTask, DEFAULT_STACK_SIZE, -1);
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
    t->status = TSK_READY;
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

static inline void copyMessage(struct TaskDescriptor *src, struct TaskDescriptor *dest){
    char* sent_msg = (char*)src->sp[2];
    int sent_msglen = src->sp[3];
    char* rcv_msg = (char*)dest->sp[2];
    int rcv_msglen = dest->sp[3];
    int len = (sent_msglen > rcv_msglen) ? rcv_msglen : sent_msglen;
    for(int i = 0; i < len; ++i){
        rcv_msg[i]=sent_msg[i];
    }
    *((int*)dest->sp[1]) = src->id;
    (dest->sp)[1] = sent_msglen;
}

void send(unsigned int task_id){
    struct TaskDescriptor *rec = &g_task_table[taskIndex(task_id)];
    if(task_id != rec->id){
        g_active_task->sp[1]=-2;
        return;
    }
    if(rec->status == TSK_SEND_BLOCKED){
        copyMessage(g_active_task, rec);
        rec->status = TSK_READY;
        priorityQueuePush(taskPriority(task_id), rec);
        g_active_task->status = TSK_REPLY_BLOCKED;
    } else {
        receiveQueuePush(rec, g_active_task);
        g_active_task->status = TSK_RECEIVE_BLOCKED;
    }
    g_active_task = 0;
}

void receive(){
    if(receiveQueueEmpty(g_active_task)){
        g_active_task->status = TSK_SEND_BLOCKED;
        g_active_task = 0;
    } else {
        struct TaskDescriptor *sender = receiveQueuePop(g_active_task, 0);
        copyMessage(sender, g_active_task);
        sender->status = TSK_REPLY_BLOCKED;
    }
}

void reply(unsigned int task_id){
    struct TaskDescriptor *sender = &g_task_table[taskIndex(task_id)];
    if(task_id != sender->id){
        g_active_task->sp[1]=-2;
        return;
    }
    if(sender->status != TSK_REPLY_BLOCKED){
        g_active_task->sp[1]=-3;
        return;
    }
    char* src_reply = (char*)g_active_task->sp[2];
    int src_replylen = g_active_task->sp[3];
    char* dest_reply = (char*)sender->sp[4];
    int dest_replylen = sender->sp[5];
    int len = (src_replylen > dest_replylen) ? dest_replylen : src_replylen;
    for(int i = 0; i < len; ++i){
        dest_reply[i]=src_reply[i];
    }
    sender->sp[1] = src_replylen;
    g_active_task->sp[1] = 0;
    sender->status = TSK_READY;
    priorityQueuePush(taskPriority(task_id), sender);
}

struct TaskDescriptor *scheduleTask(void){
    if(g_active_task){
        priorityQueuePush(taskPriority(g_active_task->id), g_active_task);
    }
    if(g_task_queue_mask){
        int priority = LOOKUP[(((unsigned int) g_task_queue_mask & -g_task_queue_mask)
            * BRUJIN_SEQUENCE) >> 27];
        g_active_task = priorityQueuePop(priority);
        assert(g_active_task->status == TSK_READY);
        return g_active_task;
    }
    return 0;
}
