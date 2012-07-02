#include <stdbool.h>

#include <user/parser.h>
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
#include <user/engineer.h>
#include <user/controller.h>
#include <user/log.h>

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

    struct SensorInterruptParse {
        int trainNumber;
        int sensor;
        int sensorNumber;
    } sensorInterrupt;

    struct SensorTimerParse {
        int sensor1;
        int sensor1Number;
        int sensor2;
        int sensor2Number;
    } sensorTimer;

    struct RouteFindParse {
        char src[5];
        char dest[5];
    } routeFind;

    struct TrainPrep {
        int trainID;
    } prepareTrain;

    struct TrainSend {
        int train_id;
        char dest[5];
        int dest_index;
    } sendTrain;
};

struct Parser {
    enum State {
        ErrorState,
        Empty,
        A_A,
        B_B,
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
        I_I,
        I_firstSpace,
        I_trainNumber,
        I_secondSpace,
        I_sensorAlpha,
        I_sensorNumber,
        D_D,
        D_firstSpace,
        D_firstSensorAlpha,
        D_firstSensorNumber,
        D_secondSpace,
        D_secondSensorAlpha,
        D_secondSensorNumber,
        P_P,
        P_firstSpace,
        P_src,
        P_secondSpace,
        P_dest,
        Z_Z,
        Z_firstSpace,
        Z_trainNumber,
        Z_secondSpace,
        Z_dest,
        E_E,
        E_firstSpace,
        E_train,
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
                    case 'a':
                        parser->state = A_A;
                        break;
                    case 'b':
                        parser->state = B_B;
                        break;
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
                    case 'd':
                        parser->state = D_D;
                        break;
                    case 'i':
                        parser->state = I_I;
                        break;
                    case 'p':
                        parser->state = P_P;
                        break;
                    case 'e':
                        parser->state = E_E;
                        break;
                    case 'z':
                        parser->state = Z_Z;
                        break;
                    default:
                        parser->state = ErrorState;
                        break;
                }
                break;

            case D_D:
                EXPECT_EXACT(' ', D_firstSpace);
                break;

            case D_firstSpace:
                parser->data.sensorTimer.sensor1 = c;
                parser->state = D_firstSensorAlpha;
                break;

            case D_firstSensorAlpha:
                parser->data.sensorTimer.sensor1Number = 0;
                if (appendDecDigit(c, &parser->data.sensorTimer.sensor1Number)) {
                    parser->state = D_firstSensorNumber;
                } else {
                    parser->state = ErrorState;
                }
                break;

            case D_firstSensorNumber:
                if (!appendDecDigit(c, &parser->data.sensorTimer.sensor1Number)) {
                    EXPECT_EXACT(' ', D_secondSpace);
                }
                break;

            case D_secondSpace:
                parser->data.sensorTimer.sensor2 = c;
                parser->state = D_secondSensorAlpha;
                break;

            case D_secondSensorAlpha:
                parser->data.sensorTimer.sensor2Number = 0;
                if (appendDecDigit(c, &parser->data.sensorTimer.sensor2Number)) {
                    parser->state = D_secondSensorNumber;
                } else {
                    parser->state = ErrorState;
                }
                break;

            case D_secondSensorNumber:
                if (!appendDecDigit(c, &parser->data.sensorTimer.sensor2Number)) {
                    parser->state = ErrorState;
                }
                break;

            case I_I:
                EXPECT_EXACT(' ', I_firstSpace);
                break;

            case I_firstSpace:
                parser->data.sensorInterrupt.trainNumber = 0;
                if (appendDecDigit(c, &parser->data.sensorInterrupt.trainNumber)) {
                    parser->state = I_trainNumber;
                } else {
                    parser->state = ErrorState;
                }
                break;

            case I_trainNumber:
                if (!appendDecDigit(c, &parser->data.sensorInterrupt.trainNumber)) {
                    EXPECT_EXACT(' ', I_secondSpace);
                }
                break;

            case I_secondSpace:
                parser->data.sensorInterrupt.sensor = c;
                parser->state = I_sensorAlpha;
                break;

            case I_sensorAlpha:
                parser->data.sensorInterrupt.sensorNumber = 0;
                if (appendDecDigit(c, &parser->data.sensorInterrupt.sensorNumber)) {
                    parser->state = I_sensorNumber;
                } else {
                    parser->state = ErrorState;
                }
                break;

            case I_sensorNumber:
                if (!appendDecDigit(c, &parser->data.sensorInterrupt.sensorNumber)) {
                    parser->state = ErrorState;
                }
                break;

            case Z_Z:
                EXPECT_EXACT(' ', Z_firstSpace);
                break;

            case Z_firstSpace:
                parser->data.sendTrain.train_id = 0;
                if (appendDecDigit(c, &parser->data.sendTrain.train_id)) {
                    parser->state = Z_trainNumber;
                } else {
                    parser->state = ErrorState;
                }
                break;

            case Z_trainNumber:
                if (!appendDecDigit(c, &parser->data.sendTrain.train_id)) {
                    EXPECT_EXACT(' ', Z_secondSpace);
                }
                break;

            case Z_secondSpace:
                parser->data.sendTrain.dest[0] =
                    (0x61 <= c && c <= 0x7A) ? c & ~0x20 : c;
                parser->data.sendTrain.dest_index = 1;
                parser->state = Z_dest;
                break;

            case Z_dest:
                if (parser->data.sendTrain.dest_index >= 4) {
                    parser->state = ErrorState;
                    break;
                }

                parser->data.sendTrain.dest[parser->data.sendTrain.dest_index++] =
                    (0x61 <= c && c <= 0x7A) ? c & ~0x20 : c;
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
            case P_P:
                for(int i = 0; i < 5; ++i){
                    parser->data.routeFind.src[i] = 0;
                    parser->data.routeFind.dest[i] = 0;
                }
                EXPECT_EXACT(' ', P_firstSpace);
                break;
            case P_firstSpace:
                parser->data.routeFind.src[0] = c;
                parser->state = P_src;
                break;
            case P_src:
                if(c == ' '){
                    parser->state = P_secondSpace;
                } else {
                    int i;
                    for(i = 1; i < 5 && parser->data.routeFind.src[i]; ++i);
                    if(i < 5){
                        parser->data.routeFind.src[i] = c;
                    } else {
                        parser->state = ErrorState;
                    }
                }
                break;
            case P_secondSpace:
                parser->data.routeFind.dest[0] = c;
                parser->state = P_dest;
                break;
            case P_dest:
                {
                    int i;
                    for(i = 1; i < 5 && parser->data.routeFind.dest[i]; ++i);
                    if(i < 5){
                        parser->data.routeFind.dest[i] = c;
                    } else {
                        parser->state = ErrorState;
                    }
                }
                break;
            case E_E:
                parser->data.prepareTrain.trainID = 0;
                EXPECT_EXACT(' ', E_train);
                break;
            case E_firstSpace:
                if(appendDecDigit(c, &parser->data.prepareTrain.trainID)){
                    parser->state = E_train;
                } else {
                    parser->state = ErrorState;
                }
                break;
            case E_train:
                if(!appendDecDigit(c, &parser->data.prepareTrain.trainID)){
                    parser->state = ErrorState;
                }
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
        sputstr(&s, CLEAR_LINE);
        vtPos(&s, CONSOLE_ROW, 1);

        switch(parser->state){
            case Q_Q:
                {
                    ret = false;
                }
                break;

            case A_A:
                {
                    logC("Setting up track A.");
                    initTrackA(nodes, hashtbl);
                }
                break;

            case B_B:
                {
                    logC("Setting up track B.");
                    initTrackB(nodes, hashtbl);
                }
                break;

            case TR_trainSpeed:
                {
                    int trainSpeed = parser->data.trainSpeed.trainSpeed;
                    int trainNumber = parser->data.trainSpeed.trainNumber;
                    if(0 > trainNumber || trainNumber > 80){
                        logC("Invalid train number, should be in [0, 80]");
                    } else if(0 > trainSpeed || trainSpeed > 14){
                        logC("Invalid train speed, should be in [0, 14]");
                    } else {
                        setSpeed(trainNumber,trainSpeed);
                    }
                }
                break;

            case RV_trainNumber:
                {
                    int trainNumber = parser->data.trainReverse.trainNumber;
                    if(0 > trainNumber || trainNumber > 80){
                        logC("Invalid train number, should be in [0, 80]");
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
                        logC("Invalid switch number, should be in [1-18],[153-156]");
                    } else {
                        if(parser->data.switchThrow.curved){
                            turnoutCurve(parser->data.switchThrow.switchNumber);
                        } else {
                            turnoutStraight(parser->data.switchThrow.switchNumber);
                        }
                    }
                }
                break;

            case I_sensorNumber: {
                int trainNumber = parser->data.sensorInterrupt.trainNumber;
                int sensor = parser->data.sensorInterrupt.sensor;
                int sensorNumber = parser->data.sensorInterrupt.sensorNumber;

                if (trainNumber < 0 || trainNumber > 80) {
                    logC("Invalid train number, should be in [0, 80]");
                    break;
                }

                if (sensor < 'a' || sensor > 'e') {
                    logC("Invalid sensor, should be in [A, E]");
                    break;
                }

                if (sensorNumber < 1 || sensorNumber > 16) {
                    logC("Invalid sensor number, should be in [1, 16]");
                    break;
                }

                sensorInterrupt(trainNumber, sensor, sensorNumber);
                break;
            }

            case D_secondSensorNumber: {
                int sensor1 = parser->data.sensorTimer.sensor1;
                int sensor1Number = parser->data.sensorTimer.sensor1Number;
                int sensor2 = parser->data.sensorTimer.sensor2;
                int sensor2Number = parser->data.sensorTimer.sensor2Number;

                if (sensor1 < 'a' || sensor2 < 'a' || sensor1 > 'e' || sensor2 > 'e') {
                    logC("Invalid sensor, should be in [A, E]");
                    break;
                }

                if (sensor1Number < 1 || sensor2Number < 1
                        || sensor1Number > 16 || sensor2Number > 16) {
                    logC("Invalid sensor number, should be in [1, 16]");
                    break;
                }

                sensorTimer(sensor1, sensor1Number, sensor2, sensor2Number);
                break;
            }

            case P_dest: {
                planRoute(parser->data.routeFind.src, parser->data.routeFind.dest);
                break;
            }

            case E_train: {
                if(parser->data.prepareTrain.trainID > 80 ||
                    parser->data.prepareTrain.trainID < 0){
                } else {
                    sputstr(&s, "Going to start engineer ");
                    sputuint(&s, parser->data.prepareTrain.trainID,10);
                    sputstr(&s, "\r\n");
                    controllerPrepareTrain(parser->data.prepareTrain.trainID);    
                }
                break;
            }

            case Z_dest: {
                parser->data.sendTrain.dest[parser->data.sendTrain.dest_index] = 0;
                struct TrackNode *destNode =
                    lookupTrackNode(hashtbl, parser->data.sendTrain.dest);

                if (!destNode) {
                    logC("Unknown destination");
                    break;
                }

                controllerSendTrain(
                    parser->data.sendTrain.train_id,
                    destNode,
                    // FIXME: accept mm
                    0
                );

                break;
            }

            default: {
                logC("Your syntax is invalid");
                break;
            }
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
    controllerQuit();
    shutdownTrains();
    sensorQuit();
    clockDrawerQuit();
    Delay(150);
    logQuit();
    Delay(150);
    mioQuit();
    tioQuit();
    ClockQuit();
    Exit();
}

void parserInit(void){
    Create(TASK_PRIORITY, commandParser);
}
