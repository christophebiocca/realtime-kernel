#include <user/kinematics.h>
#include <debug.h>
#include <user/log.h>

// TODO: Confirm this actually works for train 44.

#define FORWARD_STOPPING_COEFFICIENT        (0.0034)
#define BACKWARD_STOPPING_COEFFICIENT       (0.0028)

// if (abs(current_speed - expected_speed) < threshold) acceleration = 0
#define SPEED_THRESHOLD                     (5)

#define MINACCEL                            (4)
#define MAXACCEL                            (26)
#define MAX_SPEED                           (5480)
#define HALF_SPEED                          (MAX_SPEED/2)
#define TRANSITION1                         (2100)
#define TRANSITION2                         (600)

void computeAcceleration(struct Kinematics *k) {
    int sign = (k->target_speed < k->current_speed) ? -1 : 1;
    int diffspeed = (k->target_speed - k->current_speed) * sign;

    if (diffspeed < SPEED_THRESHOLD) {
        k->acceleration = 0;
        k->current_speed = k->target_speed;
    } else {
        int dev = HALF_SPEED - k->current_speed;
        if(dev < 0) dev = -dev;
        if(dev > TRANSITION1){
            k->acceleration = MINACCEL;
        } else if(dev < TRANSITION2){
            k->acceleration = MAXACCEL;
        } else {
            k->acceleration = (MAXACCEL - ((dev-TRANSITION2)*(MAXACCEL - MINACCEL))/(TRANSITION1-TRANSITION2));
        }
        k->acceleration *= sign;
    }
}

static inline void computeStop(struct Kinematics *k) {
    if (k->acceleration == 0 && k->target_speed == 0) {
        k->stop = 0;
    }

    float coeff = (k->orientation == FORWARD)
        ? FORWARD_STOPPING_COEFFICIENT
        : BACKWARD_STOPPING_COEFFICIENT;

    float s = k->current_speed / (2 * coeff);
    k->stop = (int) s;
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
