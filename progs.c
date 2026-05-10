#include "progs.h"

/* import all external program init functions */
extern void prog_hello(void);
extern void prog_inputname(void);

Prog progs[] = {
    {"Hello Demo", prog_hello},
    {"Input Name", prog_inputname},
};

int progs_n = sizeof(progs) / sizeof(progs[0]);
