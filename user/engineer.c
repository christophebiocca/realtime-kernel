#include <user/engineer.h>
#include <user/mio.h>
#include <user/tio.h>
#include <user/string.h>
#include <user/clock.h>
#include <user/priorities.h>
#include <user/syscall.h>
#include <stdbool.h>

/*

Engineer's workflow:

1. Wait for an instruction to reach a given speed at a given time/distance.
2. Figure out the sequence of speed instructions needed to achieve this.
3. Enqueue them and use the clockwaiter to delay until the appropriate times.

*/

void engineer(int trainID){
    // speeds of a train in um/tick.
    int speedTable[15] = {0,100,200,400,600,800,1000,1200,1600,2000,2400,3000,3600,4200,5000};
    // transition time from one speed to the one above, in ticks.
    int accel[14] = {100,40,30,20,10,10,10, 10,10,10,10,10,10,10};
    // transition time into a speed from the one above, in ticks.
    int decel[14] = {100,40,30,20,10,10,10, 10,10,10,10,10,10,10};

    // pre compute a giant transition table.
    int speedChange[15][15];
    for(int to = 0; to < 15; ++to){
        speedChange[to][to] = 0;
        for(int from = to-1; from >= 0; --from){
            speedChange[from][to] = speedChange[from+1][to] + accel[from];
        }
        for(int from = to+1; from < 15; ++from){
            speedChange[from][to] = speedChange[from-1][to] + decel[from-1];
        }
    }

    // Spawn a clock waiter.
    int waiter = CreateArgs(TASK_PRIORITY, clockWaiter, 1, MyTid());
    int time;
    {
        int tid;
        int len = Receive(&tid, (char*) &time, sizeof(int));
        assert(tid == waiter);
        assert(len == sizeof(int));
    }

    bool waiting = false;
    int currentDistance = 0;
    int currentTime = time;
    int currentSpeed = 0;

    int nextSpeed = 0;
    int nextDistance = 0;
    int nextTime = 0;
    struct EngineerTarget targets[5];
    int depth = 0;

    // Main loop
    while(true){
        char mesg[16];
        int tid;
        int len = Receive(&tid, mesg, 16);
        (void) len;

        if (tid == waiter){
            assert(len == sizeof(int));
            waiting = false;
            time = *((int*) mesg);

            currentTime = nextTime;
            currentSpeed = nextSpeed;
            currentDistance = nextDistance;

            struct String s;
            sinit(&s);
            sputc(&s, 0xF & currentSpeed);
            sputc(&s, trainID);
            tioPrint(&s);
        } else {
            assert(len == sizeof(struct EngineerTarget));
            struct EngineerTarget *target = ((struct EngineerTarget *) mesg);
            Reply(tid,0,0); // Immediately free up caller.

            targets[depth].speed = target->speed;
            targets[depth].time = target->time;
            targets[depth].distance = target->distance;
            ++depth;
        }

        if(!waiting && depth > 0){
            struct EngineerTarget *target = targets;
            int deltatime = speedChange[currentSpeed][target->speed];
            int deltadist = deltatime * (speedTable[currentSpeed] + speedTable[target->speed])/2;

            if(!(deltatime <= (target->time - currentTime))) {
                struct String s;
                sinit(&s);

                sputuint(&s,deltatime,10);
                sputstr(&s, " <= (");
                sputuint(&s, target->time, 10);
                sputstr(&s, " - ");
                sputuint(&s, currentTime, 10);
                sputstr(&s, "))");
                mioPrint(&s);

                for(int i = 0; i < 1000000; ++i);

                assert(deltatime <= (target->time - currentTime));
            }
            if(!(deltadist <= (target->distance - currentDistance))){
                struct String s;
                sinit(&s);

                sputuint(&s,deltadist,10);
                sputstr(&s, " <= (");
                sputuint(&s, target->distance, 10);
                sputstr(&s, " - ");
                sputuint(&s, currentDistance, 10);
                sputstr(&s, "))");
                mioPrint(&s);

                for(int i = 0; i < 1000000; ++i);

                assert(deltadist <= (target->distance - currentDistance));
            }

            int dist1 = currentDistance + deltadist + speedTable[currentSpeed]*(target->time - currentTime);
            int dist2 = currentDistance + deltadist + speedTable[target->speed]*(target->time - currentTime);

            assert(dist1 <= target->distance || dist2 <= target->distance);
            assert(dist1 >= target->distance || dist2 >= target->distance);

            int firetime = (-target->distance + currentDistance - speedTable[currentSpeed]*currentTime +
                deltadist + speedTable[target->speed]*target->time - speedTable[target->speed]*deltatime)
                /(speedTable[target->speed]-speedTable[currentSpeed]);

            assert(firetime >= currentTime);
            assert(firetime <= target->time);

            nextTime = target->time;
            nextSpeed = target->speed;
            nextDistance = target->distance;

            {
                struct String s;
                sinit(&s);
                sputstr(&s, "Will fire at ");
                sputuint(&s, firetime, 10);
                sputstr(&s, "\r\n");
                mioPrint(&s);
            }

            Reply(waiter,(char*)&firetime,sizeof(int));
            waiting = true;

            for(int i = 0; i < depth-1; ++i){
                targets[i].speed = targets[i+1].speed;
                targets[i].time = targets[i+1].time;
                targets[i].distance = targets[i+1].distance;
            }

            --depth;
        }
    }
}

volatile int engineerID;
void initEngineer(int trainID){
    engineerID = CreateArgs(TASK_PRIORITY, engineer, 1, trainID);
}
