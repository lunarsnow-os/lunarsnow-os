#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

#define KEY_UP    256
#define KEY_DOWN  257
#define KEY_SUPER 258

void kb_poll(void);
int  kb_pop(void);
void mouse_init(void);

extern int mouse_x, mouse_y, mouse_btn;

#endif
