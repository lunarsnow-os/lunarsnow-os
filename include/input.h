#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

#define KEY_UP      256
#define KEY_DOWN    257
#define KEY_LEFT    258
#define KEY_RIGHT   259
#define KEY_SUPER   260
#define KEY_HOME    261
#define KEY_END     262
#define KEY_PGUP    263
#define KEY_PGDN    264
#define KEY_DEL     265
#define KEY_INS     266
#define KEY_F4      268

void kb_poll(void);
int  kb_pop(void);
void mouse_init(void);

extern int mouse_x, mouse_y, mouse_btn, mouse_wheel;
extern int kb_mod_alt;

#endif
