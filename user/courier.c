#include <user/courier.h>
#include <user/syscall.h>
#include <debug.h>
#include <stdbool.h>

#define BUFFERSIZE 128

void courier(int sendertid, int msgsize, int receivertid){
    assert(msgsize <= BUFFERSIZE);
    char buffer[BUFFERSIZE];
    while(true){
        Send(sendertid,0,0,buffer,msgsize);
        Send(receivertid,buffer,msgsize,0,0);
    }
}
