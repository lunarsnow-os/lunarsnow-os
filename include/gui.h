#ifndef GUI_H
#define GUI_H

#include <stdint.h>

#define MAX_W  8
#define MAX_B 16

typedef struct { int x, y, w, h; char t[24]; void (*cb)(void); } Btn;

typedef struct {
    int x, y, w, h;
    char title[24];
    uint32_t bg, tb;
    Btn btns[MAX_B];
    int nb, fc;
    void (*draw)(int wi);
    void (*on_key)(int k);
    void (*on_click)(int wi);
    void (*on_rclick)(int wi);
    void (*on_close)(int wi);
    void *userdata;
} Win;

/* Window management */
int  gui_wnew(const char *t, int x, int y, int w, int h);
int  gui_wbtn(int wi, const char *t, int x, int y, int w, int h, void (*cb)(void));
void gui_wclose(int idx);
void gui_wfront(int idx);
void gui_render(void);
void gui_mouse_click(void);
void gui_menu_add(const char *name, void (*cb)(void));
void gui_menu_header(const char *name);
int  gui_menu_count(void);
int  gui_menu_prev(int i);
int  gui_menu_next(int i);
void gui_menu_exec(int i);
void gui_set_dirty(void);
void gui_update_cursor(void);
void gui_reset_cursor(void);

/* Colors */
enum {
    C_DSK    = 0x0F0F1C,
    C_WBG    = 0x1E1E32,
    C_TAC    = 0x3C50A0,
    C_TIN    = 0x323246,
    C_BDR    = 0x505078,
    C_BN     = 0x327878,
    C_BF     = 0x4A9696,
    C_BT     = 0xC8C8D2,
    C_LBL    = 0xB4B4BE,
    C_TTT    = 0xE6E6F0,
    C_CLS    = 0xB43232,
    C_TBAR   = 0x16162A,
    C_START  = 0x3C50A0,
    C_STARTF = 0x5A70C0,
    C_TBTN   = 0x282845,
    C_TBTNA  = 0x3C3C60,
    C_MBG    = 0x1E1E35,
    C_MFOC   = 0x3C50A0,
};

#define TB_H  28
#define ST_W  60
#define MN_W 176
#define MI_H 20

/* GUI state */
extern Win wins[MAX_W];
extern int nw, act, run;
extern int focus_mode, menu_open, menu_focus;
extern int mouse_drag, mouse_drag_win, mouse_drag_ox, mouse_drag_oy;
extern int need_render;
extern int close_on_esc;
extern int starfield_active;
extern void (*gui_tick)(void);
extern int hovered_app;
extern int hovered_start;
extern int hovered_menu;

void gui_hover_check(void);

#endif
