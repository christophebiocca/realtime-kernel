#include <user/kinematics.h>
#include <debug.h>
#include <user/log.h>

#define FORWARD_STOPPING_COEFFICIENT        (0.0034)
#define BACKWARD_STOPPING_COEFFICIENT       (0.0028)

// FIXME: This is probably incorrect...
#define ACCELERATION_COEFFICIENT            (0.0034)

// if (abs(current_speed - expected_speed) < threshold) acceleration = 0
#define SPEED_THRESHOLD                     (250)

void computeAcceleration(struct Kinematics *k) {
    int sign = (k->target_speed < k->current_speed) ? -1 : 1;
    int diffspeed = (k->target_speed - k->current_speed) * sign;

    if (diffspeed < SPEED_THRESHOLD) {
        k->acceleration = 0;
        if (k->target_speed == 0) {
            k->current_speed = 0;
        }
    } else {
        float acl = diffspeed * ACCELERATION_COEFFICIENT;
        k->acceleration = (int) acl * sign;
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
    int sign = (k->target_speed < k->current_speed) ? -1 : 1;
    int diffspeed = (k->target_speed - k->current_speed) * sign;

    if (k->target_speed > k->current_speed) {
        computeAcceleration(k);
    } else if (diffspeed < SPEED_THRESHOLD) {
        k->acceleration = 0;
        if (k->target_speed == 0) {
            k->current_speed = 0;
        }
    }

    // Measure time delta
    int dt = time - k->time;
    assert(dt >= 0);
    k->time = time;

    // Update the train speed
    int oldspeed = k->current_speed;
    k->current_speed += dt * k->acceleration;

    // Changed current_speed -> recompute stop distance.
    computeStop(k);

    if (k->acceleration != 0 || k->target_speed != 0) {
        // Distance travelled because of the old speed
        k->distance += (oldspeed * dt);

        // Deviation due to acceleration
        k->distance += (k->acceleration * (dt * dt))/2;
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
