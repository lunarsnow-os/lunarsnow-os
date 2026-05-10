#ifndef PROGS_H
#define PROGS_H

typedef struct { const char *name; void (*init)(void); } Prog;

extern Prog progs[];
extern int progs_n;

#endif
