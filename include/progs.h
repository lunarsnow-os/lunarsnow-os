#ifndef PROGS_H
#define PROGS_H

typedef struct { const char *name; void (*init)(void); } Prog;

extern Prog progs[];
extern int progs_n;

void prog_viewfile(const char *name);
void prog_bmpview(const char *name);
void prog_filemgr(void);
void prog_notepad(void);
void prog_notepad_open(const char *name);
void prog_paint(void);
void prog_inputname(void);
void prog_clock(void);
void prog_snake(void);
void prog_minesweeper(void);
void prog_pong(void);
void prog_starfield(void);
void prog_bmpviewer(void);
void prog_tetris(void);
void prog_about(void);
void prog_controlpanel(void);
void prog_taskmgr(void);

#endif
