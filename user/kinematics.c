#include <user/kinematics.h>
#include <debug.h>
#include <user/log.h>

// TODO: Confirm this actually works for train 44.

// if (abs(current_speed - expected_speed) < threshold) acceleration = 0
#define SPEED_THRESHOLD                     (5)

void kinematicsInit(struct Kinematics *k, int trainID){
    for(int i = 0; i < 15; ++i){
        k->ideal_speed[i] = 0;
    }
    k->ideal_speed[14] = 5480;
    k->target_speed = 0;
    k->current_speed = 0;
    k->stop = 0;
    k->acceleration = 0;
    k->time = 0;
    k->distance = 0;
    k->orientation = FORWARD;
    switch(trainID){
        default:
            logC("Default Kinematics");
            k->minAccel = 7;
            k->maxAccel = 26;
            k->transition1 = 300;
            k->transition2 = 1600;
    }
}

void computeAcceleration(struct Kinematics *k) {
    int sign = (k->target_speed < k->current_speed) ? -1 : 1;
    int diffspeed = (k->target_speed - k->current_speed) * sign;

    if (diffspeed < SPEED_THRESHOLD) {
        k->acceleration = 0;
        k->current_speed = k->target_speed;
    } else {
        if(k->current_speed < k->transition1){
            k->acceleration = k->minAccel;
        } else if(k->current_speed > k->transition2){
            k->acceleration = k->maxAccel;
        } else {
            k->acceleration = k->minAccel + (k->target_speed-k->transition1)/(k->transition2-k->transition1);
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

    while(speed > k->transition2){
        speed -= k->maxAccel;
        travelled += speed;
    }
    while(speed > k->transition1){
        speed -= k->minAccel + (k->target_speed-k->transition1)/(k->transition2-k->transition1);
        travelled += speed;
    }
    while(speed > 0){
        speed -= k->minAccel;
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
