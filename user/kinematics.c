#include <user/kinematics.h>
#include <debug.h>
#include <user/log.h>

// TODO: Confirm this actually works for train 44.

// if (abs(current_speed - expected_speed) < threshold) acceleration = 0
#define SPEED_THRESHOLD                     (5)

#define MINACCEL                            (4)
#define MAXACCEL                            (26)
#define MAX_SPEED                           (5480)
#define TRANSITION1                         (400)
#define TRANSITION2                         (1800)

void computeAcceleration(struct Kinematics *k) {
    int sign = (k->target_speed < k->current_speed) ? -1 : 1;
    int diffspeed = (k->target_speed - k->current_speed) * sign;

    if (diffspeed < SPEED_THRESHOLD) {
        k->acceleration = 0;
        k->current_speed = k->target_speed;
    } else {
        if(k->current_speed < TRANSITION1){
            k->acceleration = MINACCEL;
        } else if(k->current_speed > TRANSITION2){
            k->acceleration = MAXACCEL;
        } else {
            k->acceleration = MINACCEL + (k->target_speed-TRANSITION1)/(TRANSITION2-TRANSITION1);
        }
        k->acceleration *= sign;
    }
}

static inline void computeStop(struct Kinematics *k) {
    if (k->acceleration == 0 && k->target_speed == 0) {
        k->stop = 0;
    }

    int speed = k->current_speed;
    int travelled = 0;

    // TODO: Bust out the algebra, make this computation simpler.

    while(speed > TRANSITION2){
        speed -= MAXACCEL;
        travelled += speed;
    }
    while(speed > TRANSITION1){
        speed -= MINACCEL + (k->target_speed-TRANSITION1)/(TRANSITION2-TRANSITION1);
        travelled += speed;
    }
    while(speed > 0){
        speed -= MINACCEL;
        travelled += speed;
    }

    k->stop = travelled;
}

void tick(struct Kinematics *k, int time){
    computeAcceleration(k);

    // Measure time delta
    int dt = time - k->time;
    assert(dt >= 0);
    k->time = time;

    // Update the train speed
    int oldspeed = k->current_speed;
    {
        // Correct for overshoot
        int dv = dt * k->acceleration;
        if((oldspeed < k->target_speed && oldspeed + dv > k->target_speed) ||
            (oldspeed > k->target_speed && oldspeed + dv < k->target_speed)){
            k->current_speed = k->target_speed;
            k->acceleration = (k->current_speed - oldspeed)/dt;
        } else {
            k->current_speed += dt * k->acceleration;
        }
    }

    // Changed current_speed -> recompute stop distance.
    computeStop(k);

    if (k->acceleration != 0 || k->target_speed != 0) {
        // Distance travelled because of the old speed
        k->distance += (oldspeed * dt);

        // Deviation due to acceleration
        k->distance += (k->acceleration * (dt * dt))/2;
    }

    /*
    {
        struct String s;
        sinit(&s);
        sputstr(&s,"T:");
        sputint(&s,k->time,10);
        sputstr(&s," A:");
        sputint(&s,k->acceleration,10);
        sputstr(&s," Sp:");
        sputint(&s,k->current_speed,10);
        sputstr(&s," St:");
        sputint(&s,k->stop,10);
        sputstr(&s," D:");
        sputint(&s,k->distance,10);
        logS(&s);
    }
    */
}

int distForTime(struct Kinematics *k, int time) {
    int dt = time - k->time;
    assert(dt >= 0);

    int d = k->current_speed * dt + (k->acceleration * dt * dt) / 2;
    return k->distance + d;
}

// This is hard since it requires solving kinematics equations with a sqrt
// For now, return 1 second for everything
// FIXME: stub
int timeForDist(struct Kinematics *k, int distance) {
    (void) distance;
    return k->time + 100;
}
