#include <user/train.h>
#include <user/syscall.h>
#include <user/clock.h>
#include <user/tio.h>
#include <user/mio.h>
#include <user/string.h>
#include <debug.h>
#include <stdbool.h>

struct Trainstruction {
    enum {
        SetSpeed,
        Reverse,
        Started,
        SpeedReached,
        Reversed
    } messagetype : 8;
    unsigned char train;
    unsigned char speed;
};

void trainEngineer(void){
    struct Trainstruction mesg = {
        .messagetype = Started
    };
    int planner = MyParentTid();
    while(true){
        int length = Send(planner, (char*) &mesg, sizeof(struct Trainstruction),
                (char*) &mesg, sizeof(struct Trainstruction));
        (void) length;
        assert(length == sizeof(struct Trainstruction));
        struct String s;
        switch(mesg.messagetype){
            case SetSpeed:
                sinit(&s);
                sputstr(&s,"Going to change speeds.\r\n");
                mioPrint(&s);
                assert(mesg.speed < 15);
                assert(mesg.train < 80);
                sinit(&s);
                sputc(&s,0xF & mesg.speed);
                sputc(&s,mesg.train);
                tioPrint(&s);
                Delay(400);
                sinit(&s);
                sputstr(&s,"Done.\r\n");
                mioPrint(&s);
                mesg.messagetype = SpeedReached;
                break;
            case Reverse:
                sinit(&s);
                sputstr(&s,"Reversing!\r\n");
                mioPrint(&s);
                assert(mesg.train < 80);
                sinit(&s);
                sputc(&s,15);
                sputc(&s,mesg.train);
                tioPrint(&s);
                Delay(5);
                sinit(&s);
                sputstr(&s,"Done.\r\n");
                mioPrint(&s);
                mesg.messagetype = Reversed;
                break;
            default:
                assert(false);
        }
    }
}

static int trainPlannerId;
void trainPlanner(void){
    trainPlannerId = MyTid();
    int engineer[80];
    bool ready[80];
    char currentSpeed[80];
    char targetSpeed[80];
    bool reverse[80];
    for(int i = 0; i < 80; ++i){
        engineer[i] = 0;
        ready[i] = false;
        targetSpeed[i] = 0;
        currentSpeed[i] = 0;
        reverse[i] = false;
    }
    while(true){
        struct Trainstruction mesg;
        int tid;
        int length = Receive(&tid, (char*) &mesg, sizeof(struct Trainstruction));
        assert(length == sizeof(struct Trainstruction));
        (void) length;
        bool initializeEngineer = false;
        switch(mesg.messagetype){
            case SetSpeed:
                initializeEngineer = true;
                targetSpeed[mesg.train] = mesg.speed;
                Reply(tid, 0, 0);
                break;
            case Reverse:
                initializeEngineer = true;
                reverse[mesg.train] = !reverse[mesg.train];
                Reply(tid, 0, 0);
                break;
            case SpeedReached:
                assert(engineer[mesg.train] == tid);
                currentSpeed[mesg.train] = mesg.speed;
                ready[mesg.train] = true;
                break;
            case Reversed:
                assert(engineer[mesg.train] == tid);
                reverse[mesg.train] = !reverse[mesg.train];
                ready[mesg.train] = true;
                break;
            case Started:
                // The train doesn't know its id yet, find it.
                for(int i = 0; i < 80; ++i){
                    if(engineer[i] == tid){
                        mesg.train = i;
                        break;
                    }
                }
                ready[mesg.train] = true;
                break;
            default:
                assert(false);
        }
        // Deal with dispatching to/from the trains.
        bool send = false;
        if(initializeEngineer && !engineer[mesg.train]){
            engineer[mesg.train] = Create(8, trainEngineer);
        } else if(ready[mesg.train]){
            int target = (reverse[mesg.train]) ? 0 : targetSpeed[mesg.train];
            if(target != currentSpeed[mesg.train]){
                mesg.speed = target;
                mesg.messagetype = SetSpeed;
                send = true;
            } else if(reverse[mesg.train]){
                mesg.messagetype = Reverse;
                send = true;
            }
        }
        if(send){
            assert(engineer[mesg.train]);
            int ret = Reply(engineer[mesg.train], (char*) &mesg,
                sizeof(struct Trainstruction));
            (void) ret;
            assert(ret == 0);
            ready[mesg.train] = false;
        }
    }
}

// Sets the speed for the given train.
void setSpeed(int train, int speed){
    struct Trainstruction instr;
    instr.messagetype = SetSpeed;
    instr.train = train;
    instr.speed = speed;
    int length = Send(trainPlannerId, (char*) &instr,
        sizeof(struct Trainstruction), 0,0);
    (void) length;
    assert(length == 0);
}

// Reverses the train.
void reverse(int train){
    struct Trainstruction instr;
    instr.messagetype = Reverse;
    instr.train = train;
    int length = Send(trainPlannerId, (char*) &instr,
        sizeof(struct Trainstruction), 0,0);
    (void) length;
    assert(length == 0);
}
