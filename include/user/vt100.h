#ifndef VT100_H
#define VT100_H

#include <user/string.h>

#define TIMER_ROW         1
#define TRAIN_ROW         2
#define TIMER_COL         60
#define SWITCH_ROW        5
#define SENSOR_ROW        16
#define SENSOR_COL        18
#define LOG_ROW           18
#define SCROLL_BOTTOM     50
#define CONSOLE_ROW       52

void vtInit(void);

void vtPos(struct String *s, unsigned int row, unsigned int col);

/* dumb hack because CPP doesn't expand macros on stringification */
#define pp_str(x)   #x
#define pp_xstr(x)  pp_str(x)

#define ESC               "\033"
#define CSI               ESC "["
#define SEP               ";"

#define SCROLL(top, bot)  (CSI pp_xstr(top) SEP pp_xstr(bot) "r")

#define CURSOR_SAVE       (ESC "7")
#define CURSOR_RESTORE    (ESC "8")
#define CURSOR_HIDE       (CSI "?25l")
#define CURSOR_SHOW       (CSI "?25h")

#define CLEAR_SCREEN      (CSI "2J")
#define CLEAR_LINE        (CSI "2K")

#define RESET             (CSI "0m")
#define RED               (CSI "31m")
#define GREEN             (CSI "32m")
#define YELLOW            (CSI "33m")
#define CYAN              (CSI "36m")

#endif
