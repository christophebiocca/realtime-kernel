#include <user/parser.h>
#include <stdbool.h>
#include <user/string.h>
#include <user/train.h>
#include <user/mio.h>
#include <user/tio.h>
#include <user/priorities.h>
#include <user/syscall.h>
#include <user/sensor.h>
#include <user/turnout.h>
#include <user/clock.h>
#include <user/vt100.h>
#include <user/clock_drawer.h>

union ParserData {
    struct TrainSpeedParse {
        int trainNumber;
        int trainSpeed;
    } trainSpeed;
    struct SwitchThrowParse {
        int switchNumber;
        bool curved;
    } switchThrow;
    struct TrainReverseParse {
        int trainNumber;
    } trainReverse;
};

struct Parser {
    enum State {
        ErrorState,
        Empty,
        TR_T,
        TR_R,
        TR_firstSpace,
        TR_trainNumber,
        TR_secondSpace,
        TR_trainSpeed,
        RV_R,
        RV_V,
        RV_space,
        RV_trainNumber,
        SW_S,
        SW_W,
        SW_firstSpace,
        SW_switchNumber,
        SW_secondSpace,
        SW_S_Or_C,
        Q_Q
    } state;
    union ParserData data;
};

#define EXPECT_EXACT(chr,succ)\
parser->state = (c == chr) ? succ : ErrorState;

// Appends a decimal digit to the number.
// return 1 for success, 0 invalid character.
static inline int appendDecDigit(char c, int *num){
    unsigned int d;
    if('0' <= c && c <= '9'){
        d = c - '0';
    } else {
        return 0;
    }
    (*num) = (*num) * 10 + d;
    return 1;
}

// Main entry point for parsing, allows us to
bool parse(struct Parser *parser, char c){
    bool ret = true;
    struct String s;
    sinit(&s);
    // Only feeds printable chars + new line to the parser.
    // Allows us to handle accidental backspaces and the like.
    if(0x20 <= c && c <= 0x7E){
        // Lowercase incoming letters.
        if(0x41 <= c && c <= 0x5A) c |= 0x20;
        switch(parser->state){
            case Empty: // Figure out which command we're looking at.
                switch(c){
                    case 'q':
                        parser->state = Q_Q;
                        break;
                    case 't':
                        parser->state = TR_T;
                        break;
                    case 'r':
                        parser->state = RV_R;
                        break;
                    case 's':
                        parser->state = SW_S;
                        break;
                    default:
                        parser->state = ErrorState;
                        break;
                }
                break;
            case TR_T:
                EXPECT_EXACT('r', TR_R);
                break;
            case TR_R:
                EXPECT_EXACT(' ', TR_firstSpace);
                break;
            case TR_firstSpace:
                parser->data.trainSpeed.trainNumber = 0;
                if(appendDecDigit(c, &parser->data.trainSpeed.trainNumber)){
                    parser->state = TR_trainNumber;
                } else {
                    parser->state = ErrorState;
                }
                break;
            case TR_trainNumber:
                if(!appendDecDigit(c, &parser->data.trainSpeed.trainNumber)){
                    EXPECT_EXACT(' ', TR_secondSpace);
                }
                break;
            case TR_secondSpace:
                parser->data.trainSpeed.trainSpeed = 0;
                if(appendDecDigit(c, &parser->data.trainSpeed.trainSpeed)){
                    parser->state = TR_trainSpeed;
                } else {
                    parser->state = ErrorState;
                }
                break;
            case TR_trainSpeed:
                if(!appendDecDigit(c, &parser->data.trainSpeed.trainSpeed)){
                    parser->state = ErrorState;
                }
                break;
            case RV_R:
                EXPECT_EXACT('v', RV_V);
                break;
            case RV_V:
                EXPECT_EXACT(' ', RV_space);
                break;
            case RV_space:
                parser->data.trainReverse.trainNumber = 0;
                if(appendDecDigit(c, &parser->data.trainReverse.trainNumber)){
                    parser->state = RV_trainNumber;
                } else {
                    parser->state = ErrorState;
                }
                break;
            case RV_trainNumber:
                if(!appendDecDigit(c, &parser->data.trainReverse.trainNumber)){
                    parser->state = ErrorState;
                }
                break;
            case SW_S:
                EXPECT_EXACT('w', SW_W);
                break;
            case SW_W:
                EXPECT_EXACT(' ', SW_firstSpace);
                break;
            case SW_firstSpace:
                parser->data.switchThrow.switchNumber = 0;
                if(appendDecDigit(c, &parser->data.switchThrow.switchNumber)){
                    parser->state = SW_switchNumber;
                } else {
                    parser->state = ErrorState;
                }
                break;
            case SW_switchNumber:
                if(!appendDecDigit(c, &parser->data.switchThrow.switchNumber)){
                    if(c == ' '){
                        parser->state = SW_secondSpace;
                    } else {
                        parser->state = ErrorState;
                    }
                }
                break;
            case SW_secondSpace:
                switch(c){
                    case 'c':
                        parser->data.switchThrow.curved = true;
                        parser->state = SW_S_Or_C;
                        break;
                    case 's':
                        parser->data.switchThrow.curved = false;
                        parser->state = SW_S_Or_C;
                        break;
                    default:
                        parser->state = ErrorState;
                        break;
                }
                break;
            case SW_S_Or_C:
                parser->state = ErrorState;
                break;
            case Q_Q:
                parser->state = ErrorState;
                break;
            default:
                break;
        }
        if(parser->state == ErrorState){
            sputstr(&s, "\x1B[41m");
        }
        sputc(&s,c);
    } else if(c == 0x0D){
        sputstr(&s, "\x1B[0m\r\n");
        switch(parser->state){
            case Q_Q:
                {
                    ret = false;
                }
                break;
            case TR_trainSpeed:
                {
                    int trainSpeed = parser->data.trainSpeed.trainSpeed;
                    int trainNumber = parser->data.trainSpeed.trainNumber;
                    if(0 > trainNumber || trainNumber > 80){
                        sputstr(&s, "Invalid train number, should"
                            " be between 0 and 80 inclusively.\r\n");
                    } else if(0 > trainSpeed || trainSpeed > 14){
                        sputstr(&s, "Invalid train speed, should"
                            " be between 0 and 14 inclusively.\r\n");
                    } else {
                        setSpeed(trainNumber,trainSpeed);
                    }
                }
                break;
            case RV_trainNumber:
                {
                    int trainNumber = parser->data.trainReverse.trainNumber;
                    if(0 > trainNumber || trainNumber > 80){
                        sputstr(&s, "Invalid train number, should"
                            " be between 0 and 80 inclusively.\r\n");
                    } else {
                        reverse(trainNumber);
                    }
                }
                break;
            case SW_S_Or_C:
                {
                    int switchNumber = parser->data.switchThrow.switchNumber;
                    if(!((1 <= switchNumber && switchNumber <= 18) ||
                        (153 <= switchNumber && switchNumber <= 156))){
                        sputstr(&s, "Invalid switch number, should"
                            " be in [1-18],[153-156].");
                    } else {
                        if(parser->data.switchThrow.curved){
                            turnoutCurve(parser->data.switchThrow.switchNumber);
                        } else {
                            turnoutStraight(parser->data.switchThrow.switchNumber);
                        }
                    }
                }
                break;
            default:
                {
                    sputstr(&s, "Your syntax is invalid.\r\n");
                }
                break;
        }
        if(ret){
            sputstr(&s, "> ");
        }
        parser->state = Empty;
    } // Ignore non printing characters.
    mioPrint(&s);
    return ret;
}

void commandParser(void){
    struct Parser parser;
    parser.state = Empty;
    struct String s;
    bool active = true;
    sinit(&s);
    vtPos(&s, CONSOLE_ROW, 1);
    sputstr(&s, CURSOR_SHOW);
    sputstr(&s, "> ");
    mioPrint(&s);
    while(active){
        sinit(&s);
        mioRead(&s);
        for(int i = 0; i < s.offset; ++i){
            active = parse(&parser, s.buffer[i]);
        }
    }
    sinit(&s);
    sputstr(&s, "Graceful shutdown in progress\r\n");
    mioPrint(&s);
    shutdownTrains();
    sensorQuit();
    clockDrawerQuit();
    Delay(300);
    mioQuit();
    tioQuit();
    ClockQuit();
    Exit();
}

void parserInit(void){
    Create(TASK_PRIORITY, commandParser);
}
