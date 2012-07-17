#ifndef USER_TURNOUT_H
#define USER_TURNOUT_H  1

#include <stdbool.h>
#include <debug.h>

// bits 0-17 are switches 1-18, 18-22 are 153-156
typedef int TurnoutTable;

static inline bool isTurnoutCurved(TurnoutTable table, int address){
    if(address >= 153 && address <= 156){
        address -= 135;
    } else {
        assert(address >= 1 && address <= 18);
        address -= 1;
    }
    return (table >> address) & 0x1;
}

static inline bool isTurnoutStraight(TurnoutTable table, int address){
    return !isTurnoutCurved(table, address);
}

void turnoutInit(void);
void turnoutQuit(void);

void turnoutCurve(int address, TurnoutTable* tbl);
void turnoutStraight(int address, TurnoutTable* tbl);
TurnoutTable turnoutQuery(void);

#endif /* USER_TURNOUT_H */
