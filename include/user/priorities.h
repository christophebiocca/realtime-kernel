#ifndef USER_PRIORITIES_H
#define USER_PRIORITIES_H   1

#define INIT_PRIORITY               0
#define NOTIFIER_PRIORITY           1
#define HARDWARE_SERVER_PRIORITY    2
#define TASK_SERVER_PRIORITY        4
#define TASK_PRIORITY               10
#define IDLE_PRIORITY               31

/* individual task priorities, to allow for fine tuning */

#define NAMESERVER_PRIORITY SERVER_PRIORITY

#define CLOCK_NOTIFIER_PRIORITY NOTIFIER_PRIORITY
#define CLOCK_SERVER_PRIORITY   HARDWARE_SERVER_PRIORITY

#define MIO_NOTIFIER_PRIORITY   NOTIFIER_PRIORITY
#define MIO_SERVER_PRIORITY     HARDWARE_SERVER_PRIORITY

#define TIO_NOTIFIER_PRIORITY   NOTIFIER_PRIORITY
#define TIO_SERVER_PRIORITY     HARDWARE_SERVER_PRIORITY

#define LOG_PRIORITY        TASK_SERVER_PRIORITY
#define TURNOUT_PRIORITY    TASK_SERVER_PRIORITY
#define CONTROLLER_PRIORITY TASK_SERVER_PRIORITY
#define ENGINEER_PRIORITY   TASK_SERVER_PRIORITY

#endif /* USER_PRIORITIES_H */
