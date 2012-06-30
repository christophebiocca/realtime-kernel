#include <stdbool.h>

struct EngineerTarget {
    int speed;
    int time;
    int distance;
    int dummy;
};

void engineer(int trainID);

extern volatile int engineerID;
void initEngineer(int trainID);

