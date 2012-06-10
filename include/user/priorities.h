#ifndef USER_PRIORITIES_H
#define USER_PRIORITIES_H   1

#define INIT_PRIORITY       0
#define NOTIFIER_PRIORITY   1
#define SERVER_PRIORITY     2
#define TASK_PRIORITY       10

/* individual task priorities, to allow for fine tuning */

#define NAMESERVER_PRIORITY SERVER_PRIORITY

#define CLOCK_NOTIFIER_PRIORITY NOTIFIER_PRIORITY
#define CLOCK_SERVER_PRIORITY   SERVER_PRIORITY

#endif /* USER_PRIORITIES_H */
