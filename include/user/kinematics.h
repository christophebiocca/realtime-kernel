#ifndef KINEMATICS_H
#define KINEMATICS_H 1

// All values are based on micrometers/ticks
struct Kinematics {
    int ideal_speed[15];
    int target_speed;
    int current_speed;
    int stop;
    int acceleration;
    int time;
    int distance;
    enum {
        FORWARD,
        BACKWARD
    } orientation;
};

void computeAcceleration(struct Kinematics *k);

// Calculate the changes that have occurred in the given time period.
// Updates the kinematics internal distance.
void tick(struct Kinematics *k, int newtime);

// Return the value of distance when time reaches a given point.
int distForTime(struct Kinematics *k, int time);

// Returns the time at which distance will have a certain value.
int timeForDist(struct Kinematics *k, int distance);

#endif
