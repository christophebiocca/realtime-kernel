#include <debug.h>
#include <lib.h>

#include <kernel/ipc.h>
#include <kernel/task.h>

#include "task_internal.h"

static inline void copyMessage(struct TaskDescriptor *src, struct TaskDescriptor *dest){
    char* sent_msg = (char*)src->sp[2];
    int sent_msglen = src->sp[3];
    char* rcv_msg = (char*)dest->sp[2];
    int rcv_msglen = dest->sp[3];

    int len = (sent_msglen > rcv_msglen) ? rcv_msglen : sent_msglen;
    memcpy16(rcv_msg, sent_msg, len);

    *((int*)dest->sp[1]) = src->id;
    (dest->sp)[1] = sent_msglen;
}

void ipcSend(unsigned int task_id){
    struct TaskDescriptor *rec = &g_task_table[taskIndex(task_id)];

    // don't send to dead tasks
    if(rec->status == TSK_ZOMBIE){
        bwsetfifo(COM2,false);
        bwsetspeed(COM2,115200);
        bwprintf(COM2,"%d sent to %d which is a zombie.",g_active_task->id,
            task_id);
        assert(rec->status != TSK_ZOMBIE);
    }

    // don't send to yourself
    assert(g_active_task->id != rec->id);

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

void ipcReceive(){
    if(receiveQueueEmpty(g_active_task)){
        g_active_task->status = TSK_SEND_BLOCKED;
        g_active_task = 0;
    } else {
        struct TaskDescriptor *sender = receiveQueuePop(g_active_task, 0);
        copyMessage(sender, g_active_task);
        sender->status = TSK_REPLY_BLOCKED;
    }
}

void ipcReply(unsigned int task_id){
    struct TaskDescriptor *sender = &g_task_table[taskIndex(task_id)];

    if(task_id != sender->id){
        g_active_task->sp[1] = -2;
        return;
    }
    if(sender->status != TSK_REPLY_BLOCKED){
        g_active_task->sp[1] = -3;
        return;
    }

    char* src_reply = (char*)g_active_task->sp[2];
    int src_replylen = g_active_task->sp[3];
    char* dest_reply = (char*)sender->sp[4];
    int dest_replylen = sender->sp[5];

    int len = (src_replylen > dest_replylen) ? dest_replylen : src_replylen;
    memcpy16(dest_reply, src_reply, len);

    sender->sp[1] = src_replylen;
    g_active_task->sp[1] = 0;
    sender->status = TSK_READY;
    priorityQueuePush(taskPriority(task_id), sender);
}
