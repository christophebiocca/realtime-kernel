#include <ts7200.h>
#include <stdbool.h>

#include <user/mio.h>
#include <user/priorities.h>
#include <user/sensor.h>
#include <user/string.h>
#include <user/syscall.h>
#include <user/tio.h>
#include <user/vt100.h>

#include <user/train.h>

/* 0 <= sensor < NUM_SENSORS
 *  * 0 <= bit < 16 */
#define SENSOR_POS(s, sensor, bit) vtPos(s, SENSOR_ROW + sensor, bit * 4 + 1)
#define NUM_SENSORS 5

#define SENSOR_RESET    192
#define SENSOR_NORESET  128
#define SENSOR_QUERY    133 /* 128 + NUM_SENSORS */

static int g_sensor_quit;

static void sensorFormat(struct String *s, char byte, char bit) {
    char sensor = byte / 2;
    /* Marklin does this dumb thing where the high order bit is the low order
     * sensor. Not sure why this needs to be 9 though... */
    bit = 9 - bit;
    if (byte % 2) {
        bit += 8;
    }

    sputc(s, 'A' + sensor);
    sputc(s, (bit > 9) ? '1' : '0');
    sputc(s, '0' + (bit % 10));
}

#define NUM_RECENT_SENSORS  6
struct RecentSensor {
    char byte;
    char bit;
};

static void updateSensorDisplay(struct RecentSensor *recent_sensors, int recent_i) {
    struct String s;
    sinit(&s);

    sputstr(&s, CURSOR_HIDE);
    sputstr(&s, CURSOR_SAVE);
    vtPos(&s, SENSOR_ROW, SENSOR_COL);
    sputstr(&s, RESET);

    for (int i = (recent_i + 1) % NUM_RECENT_SENSORS, j = 0;
            recent_sensors[i].bit != 0;
            i = (i + 1) % NUM_RECENT_SENSORS, ++j) {
        sensorFormat(&s, recent_sensors[i].byte, recent_sensors[i].bit);
        sputc(&s, ' ');
    }

    sputstr(&s, RESET);
    sputstr(&s, CURSOR_RESTORE);
    sputstr(&s, CURSOR_SHOW);

    mioPrint(&s);
}

static struct {
    int train_number;
    int byte;
    int bit;
} g_current_sensor_interrupt;

static struct {
    int sensor1_byte;
    int sensor1_bit;
    int sensor2_byte;
    int sensor2_bit;
} g_current_sensor_timer;

static void sensorTask(void) {
    struct RecentSensor recent_sensors[NUM_RECENT_SENSORS];
    int recent_i;
    recent_i = NUM_RECENT_SENSORS - 2;

    for (int i = 0; i < NUM_RECENT_SENSORS; ++i) {
        recent_sensors[i].byte = recent_sensors[i].bit = 0;
    }

    int last_byte;
    last_byte = 0;

    char sensor_state[10]; // NUM_SENSORS * 2
    for (int i = 0; i < (NUM_SENSORS * 2); ++i) {
        sensor_state[i] = 0;
    }

    struct String query;
    sinit(&query);
    sputc(&query, SENSOR_RESET);
    sputc(&query, SENSOR_QUERY);
    tioPrint(&query);

    sinit(&query);
    sputc(&query, SENSOR_QUERY);

    struct String s;
    sinit(&s);
    vtPos(&s, SENSOR_ROW, 1);
    sputstr(&s, "Recent Switches: ");
    mioPrint(&s);

    bool first_loop = true;
    while (!g_sensor_quit) {
        tioRead(&s);

        for (unsigned int i = 0; i < slen(&s); ++i) {
            char c = sbuffer(&s)[i];
            char old = sensor_state[last_byte];
            char diff = ~old & c;
            sensor_state[last_byte] = c;

            for (int bit = 1; !first_loop && diff; diff >>= 1, ++bit) {
                if (diff & 1) {
                    recent_sensors[recent_i].byte = last_byte;
                    recent_sensors[recent_i].bit = bit;

                    if (last_byte == g_current_sensor_interrupt.byte
                            && bit == g_current_sensor_interrupt.bit) {
                        setSpeed(g_current_sensor_interrupt.train_number, 0);
                    }

                    if (last_byte == g_current_sensor_timer.sensor1_byte
                            && bit == g_current_sensor_timer.sensor1_bit) {
                        *(TIMER4_CRTL) = TIMER4_ENABLE;
                    }

                    if (last_byte == g_current_sensor_timer.sensor2_byte
                            && bit == g_current_sensor_timer.sensor2_bit) {
                        unsigned int ticks = *(TIMER4_VAL);
                        *(TIMER4_CRTL) = 0;
                        struct String s;

                        sinit(&s);
                        sputstr(&s, CURSOR_SAVE);
                        vtPos(&s, TIMER_ROW, 1);
                        sputstr(&s, "Ticks: ");
                        sputuint(&s, ticks, 10);
                        sputstr(&s, "        ");
                        sputstr(&s, CURSOR_RESTORE);
                        mioPrint(&s);
                    }

                    /* becase modulo of a negative number can be negative >_> */
                    if (--recent_i < 0) {
                        recent_i += NUM_RECENT_SENSORS;
                    }

                    recent_sensors[recent_i].byte = 0;
                    recent_sensors[recent_i].bit = 0;
                    updateSensorDisplay(recent_sensors, recent_i);
                }
            }

            last_byte = (last_byte + 1) % 10;
            if (last_byte == 0) {
                first_loop = false;
                tioPrint(&query);
            }
        }
    }

    Exit();
}

void sensorInit(void) {
    g_sensor_quit = false;
    Create(TASK_PRIORITY, sensorTask);

    g_current_sensor_interrupt.byte = -1;
    g_current_sensor_timer.sensor1_byte = -1;
    g_current_sensor_timer.sensor2_byte = -1;
}

void sensorQuit(void) {
    g_sensor_quit = true;
}

static inline void sensorToByteBit(int sensor, int number, int *byte, int *bit) {
    *byte = (sensor - 'a') * 2;
    if (number > 8) {
        ++(*byte);
        number -= 8;
    }

    *bit = 9 - number;
}

void sensorInterrupt(int train_number, int sensor, int sensor_number) {
    g_current_sensor_interrupt.train_number = train_number;

    sensorToByteBit(
        sensor, sensor_number,
        &g_current_sensor_interrupt.byte,
        &g_current_sensor_interrupt.bit
    );
}

void sensorTimer(int sensor1, int sensor1_num, int sensor2, int sensor2_num) {
    sensorToByteBit(
        sensor1, sensor1_num,
        &g_current_sensor_timer.sensor1_byte,
        &g_current_sensor_timer.sensor1_bit
    );

    sensorToByteBit(
        sensor2, sensor2_num,
        &g_current_sensor_timer.sensor2_byte,
        &g_current_sensor_timer.sensor2_bit
    );
}
