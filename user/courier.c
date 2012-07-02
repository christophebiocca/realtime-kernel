#include <user/courier.h>
#include <user/syscall.h>
#include <debug.h>
#include <stdbool.h>

#define BUFFERSIZE 128

void courier(int sendertid, int msgsize, int receivertid) {
    assert(msgsize <= BUFFERSIZE);
    char buffer[BUFFERSIZE];

    while(true) {
        int replylen = Send(sendertid, 0, 0, buffer, msgsize);
        
        if (replylen == 0) {
            break;
        }

        Send(receivertid, buffer, msgsize, 0, 0);
    }

    Exit();
}
