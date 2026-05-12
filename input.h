#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

#define KEY_UP    256
#define KEY_DOWN  257
#define KEY_LEFT  258
#define KEY_RIGHT 259
#define KEY_SUPER 260
#define KEY_F4    264

void kb_poll(void);
int  kb_pop(void);
void mouse_init(void);

extern int mouse_x, mouse_y, mouse_btn;
extern int kb_mod_alt;

#endif
