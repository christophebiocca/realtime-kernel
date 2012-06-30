#ifndef USER_TRAIN_H
#define USER_TRAIN_H    1

// Process that manages trains
void trainPlanner(void);

// Sets the speed for the given train.
void setSpeed(int train, int speed);

// Reverses the train.
void reverse(int train);

// Initialize the train process
void trainInit(void);

// Shutdown trains.
void shutdownTrains(void);

#endif
