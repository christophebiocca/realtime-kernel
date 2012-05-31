#ifndef IPC_H
#define IPC_H   1

/* Sends a message to another task */
void ipcSend(unsigned int task_id);

/* Receives a message from any task */
void ipcReceive();

/* Replies to a task that had sent a message */
void ipcReply(unsigned int task_id);

#endif
