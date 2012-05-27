#ifndef NAMESERVER_H
#define NAMESERVER_H 1

void task_nameserver(void);

/* Returns:
 *  0   Success
 *  -1  Generic error
 *  -2  Name table is full
 *  -3  Name server doesn't exist
 */
int RegisterAs(char *name);

/* Returns:
 * >=0  Task id
 * -1   Generic error
 * -2   No task registered under that name
 * -3   Name server doesn't exist
 */
int WhoIs(char *name);

#endif
