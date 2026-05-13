#include "progs.h"

/* import all external program init functions */
extern void prog_inputname(void);
extern void prog_notepad(void);
extern void prog_about(void);
extern void prog_controlpanel(void);
extern void prog_filemgr(void);
extern void prog_snake(void);
extern void prog_minesweeper(void);
extern void prog_paint(void);

Prog progs[] = {
    {"Input Name", prog_inputname},
    {"Notepad", prog_notepad},
    {"Snake", prog_snake},
    {"Minesweeper", prog_minesweeper},
    {"About", prog_about},
    {"Control Panel", prog_controlpanel},
    {"File Manager", prog_filemgr},
    {"Paint", prog_paint},
};

int progs_n = sizeof(progs) / sizeof(progs[0]);
