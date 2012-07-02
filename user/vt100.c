#include <user/mio.h>
#include <user/string.h>
#include <user/vt100.h>

void vtInit(void) {
    struct String s;

    sinit(&s);
    sputstr(&s, CLEAR_SCREEN);
    sputstr(&s, CURSOR_HIDE);
    sputstr(&s, RESET);
    sputstr(&s, SCROLL(LOG_ROW, SCROLL_BOTTOM));
    mioPrint(&s);

    sinit(&s);
    vtPos(&s, TIMER_ROW, TIMER_COL - 6);
    sputstr(&s, "Time: ");

    vtPos(&s, CONSOLE_ROW - 1, 1);
    sputstr(&s, "Reticulating splines...\n");
    mioPrint(&s);
}

void vtPos(struct String *s, unsigned int row, unsigned int col) {
    sputstr(s, CSI);
    sputuint(s, row, 10);
    sputstr(s, SEP);
    sputuint(s, col, 10);
    sputc(s, 'H');
}
