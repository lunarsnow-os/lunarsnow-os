#include "progs.h"

/* import all external program init functions */
extern void prog_hello(void);
extern void prog_inputname(void);
extern void prog_notepad(void);
extern void prog_msgbox_demo(void);
extern void prog_about(void);

Prog progs[] = {
    {"Hello Demo", prog_hello},
    {"Input Name", prog_inputname},
    {"Notepad", prog_notepad},
    {"Message Demo", prog_msgbox_demo},
    {"About", prog_about},
};

int progs_n = sizeof(progs) / sizeof(progs[0]);
