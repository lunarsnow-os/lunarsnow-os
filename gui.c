#include <stdint.h>
#include "font8x16.h"
#include "fb.h"
#include "input.h"
#include "gui.h"

/* ================================================================
   STATE
   ================================================================ */

Win wins[MAX_W];
int nw, act, run;
int focus_mode, menu_open, menu_focus;
int mouse_drag, mouse_drag_win, mouse_drag_ox, mouse_drag_oy;

/* Dynamic menu */
#define MENU_MAX 16
static struct { const char *name; void (*cb)(void); } menu[MENU_MAX];
static int menu_n;

void gui_menu_add(const char *name, void (*cb)(void))
{
    if (menu_n < MENU_MAX) { menu[menu_n].name = name; menu[menu_n].cb = cb; menu_n++; }
}

int gui_menu_count(void) { return menu_n; }
void gui_menu_exec(int i) { if (i >= 0 && i < menu_n && menu[i].cb) menu[i].cb(); }

extern void rtc_read(int *h, int *m, int *s);

/* ================================================================
   WINDOW MANAGEMENT
   ================================================================ */

int gui_wnew(const char *t, int x, int y, int w, int h)
{
    if (nw >= MAX_W) return -1;
    int i = nw++;
    wins[i].x = x; wins[i].y = y; wins[i].w = w; wins[i].h = h;
    s_cpy(wins[i].title, t, 24);
    wins[i].bg = C_WBG; wins[i].tb = C_TAC;
    wins[i].nb = 0; wins[i].fc = 0;
    wins[i].draw = 0; wins[i].on_key = 0; wins[i].on_click = 0;
    wins[i].userdata = 0;
    act = i; return i;
}

int gui_wbtn(int wi, const char *t, int x, int y, int w, int h, void (*cb)(void))
{
    if (wi < 0 || wi >= nw) return -1;
    Btn *b = &wins[wi].btns[wins[wi].nb++];
    b->x = x; b->y = y; b->w = w; b->h = h; b->cb = cb;
    s_cpy(b->t, t, 24); return wins[wi].nb - 1;
}

void gui_wclose(int idx)
{
    if (idx < 0 || idx >= nw) return;
    for (int i = idx; i < nw - 1; i++) mcpy(&wins[i], &wins[i+1], sizeof(Win));
    nw--;
    if (act >= nw && nw > 0) act = nw - 1;
}

/* ================================================================
   WINDOW DRAW
   ================================================================ */

static void wdraw(int wi)
{
    Win *w = &wins[wi];
    int x = w->x, y = w->y, ww = w->w, hh = w->h;
    int a = (wi == act);
    uint32_t tc = a ? C_TAC : C_TIN;
    uint32_t bc = a ? C_BDR : C_TIN;

    fb_rect(x + 2, y + 20, ww - 4, hh - 22, w->bg);
    fb_rect(x + 2, y + 2, ww - 4, 18, tc);
    fb_txt(x + 6, y + 3, w->title, C_TTT, tc);

    fb_rect(x + ww - 18, y + 3, 15, 15, C_CLS);
    fb_txt(x + ww - 15, y + 4, "X", C_TTT, C_CLS);

    fb_border(x, y, ww, hh, bc);

    for (int i = 0; i < w->nb; i++) {
        Btn *b = &w->btns[i];
        int bx = x + 2 + b->x, by = y + 20 + b->y;
        int f = (i == w->fc && wi == act);
        uint32_t col = f ? C_BF : C_BN;
        fb_rect(bx, by, b->w, b->h, col);
        fb_border(bx, by, b->w, b->h, f ? 0xFFFFFF : 0x1E4E4E);
        int l = s_len(b->t);
        fb_txt(bx + (b->w - l * 8) / 2, by + (b->h - 16) / 2, b->t, C_BT, col);
    }

    if (w->draw) w->draw(wi);
}

/* ================================================================
   MOUSE CURSOR — save/restore for fast redraw
   ================================================================ */

#define CUR_W 9
#define CUR_H 13

static const uint8_t curs_img[] = {
    0x80, 0xC0, 0xA0, 0x90, 0x88, 0x84,
    0x82, 0x81, 0x80, 0xA0, 0x50, 0x20
};

static uint32_t curs_save[CUR_H][CUR_W];
static int curs_sx = -1, curs_sy = -1;

int need_render = 1;
void gui_set_dirty(void) { need_render = 1; }

static void curs_restore(void)
{
    int x = curs_sx, y = curs_sy;
    if (x < 0) return;
    for (int r = 0; r < CUR_H && y + r < fb_h; r++)
        for (int c = 0; c < CUR_W && x + c < fb_w; c++)
            sbuf[(y + r) * fb_w + (x + c)] = curs_save[r][c];
}

static void curs_save_area(void)
{
    int x = mouse_x, y = mouse_y;
    for (int r = 0; r < CUR_H && y + r < fb_h; r++)
        for (int c = 0; c < CUR_W && x + c < fb_w; c++)
            curs_save[r][c] = sbuf[(y + r) * fb_w + (x + c)];
    curs_sx = x; curs_sy = y;
}

static void curs_draw_at(int x, int y)
{
    for (int r = 0; r < 12 && y + r + 1 < fb_h; r++) {
        uint8_t bits = curs_img[r];
        for (int c = 0; c < 8 && x + c + 1 < fb_w; c++)
            if (bits & (0x80 >> c))
                sbuf[(y + r + 1) * fb_w + (x + c + 1)] = 0x000000;
    }
    for (int r = 0; r < 12 && y + r < fb_h; r++) {
        uint8_t bits = curs_img[r];
        for (int c = 0; c < 8 && x + c < fb_w; c++)
            if (bits & (0x80 >> c))
                sbuf[(y + r) * fb_w + (x + c)] = 0xFFFFFF;
    }
}

void gui_update_cursor(void)
{
    int ox = curs_sx, oy = curs_sy;
    curs_restore();
    curs_save_area();
    curs_draw_at(mouse_x, mouse_y);
    if (ox >= 0) {
        int x1 = ox < mouse_x ? ox : mouse_x;
        int y1 = oy < mouse_y ? oy : mouse_y;
        int x2 = (ox > mouse_x ? ox : mouse_x) + CUR_W;
        int y2 = (oy > mouse_y ? oy : mouse_y) + CUR_H;
        if (x2 > fb_w) x2 = fb_w;
        if (y2 > fb_h) y2 = fb_h;
        fb_flip_rect(x1, y1, x2 - x1, y2 - y1);
    } else {
        fb_flip_rect(mouse_x, mouse_y, CUR_W, CUR_H);
    }
}

/* ================================================================
   MOUSE CLICK
   ================================================================ */

void gui_mouse_click(void)
{
    int x = mouse_x, y = mouse_y;

    if (menu_open) {
        int mx = 2;
        int my = fb_h - TB_H - menu_n * 24 - 4;
        if (x >= mx && x < mx + MN_W && y >= my && y < my + menu_n * 24 + 4) {
            int item = (y - my - 2) / 24;
            if (item >= 0 && item < menu_n && menu[item].cb) menu[item].cb();
        }
        menu_open = 0; focus_mode = 0;
        return;
    }

    if (y >= fb_h - TB_H) {
        if (x < ST_W + 2) {
            menu_open = 1; menu_focus = 0; focus_mode = 2;
            return;
        }
        int bx = ST_W + 6;
        for (int i = 0; i < nw; i++) {
            int bw = s_len(wins[i].title) * 8 + 12;
            if (bw > 140) bw = 140;
            if (x >= bx && x < bx + bw) { act = i; focus_mode = 0; return; }
            bx += bw + 2;
        }
        return;
    }

    for (int i = nw - 1; i >= 0; i--) {
        Win *w = &wins[i];
        int wx = w->x, wy = w->y, ww = w->w, wh = w->h;
        if (x < wx || x >= wx + ww || y < wy || y >= wy + wh) continue;
        act = i; focus_mode = 0;

        if (x >= wx + ww - 18 && x < wx + ww - 3 &&
            y >= wy + 3 && y < wy + 18) {
            gui_wclose(i); return;
        }
        if (y >= wy + 2 && y < wy + 20) {
            mouse_drag = 1;
            mouse_drag_win = i;
            mouse_drag_ox = x - wx;
            mouse_drag_oy = y - wy;
            return;
        }
        for (int j = 0; j < w->nb; j++) {
            Btn *b = &w->btns[j];
            int bx = wx + 2 + b->x, by = wy + 20 + b->y;
            if (x >= bx && x < bx + b->w && y >= by && y < by + b->h) {
                if (b->cb) b->cb();
                return;
            }
        }
        /* Custom click handler (e.g. file list) */
        if (w->on_click) { w->on_click(i); return; }
        return;
    }
}

/* ================================================================
   RENDER
   ================================================================ */

void gui_render(void)
{
    fb_rect(0, 0, fb_w, fb_h, C_DSK);
    for (int i = 0; i < nw; i++) wdraw(i);

    /* Taskbar */
    fb_rect(0, fb_h - TB_H, fb_w, TB_H, C_TBAR);
    fb_border(0, fb_h - TB_H, fb_w, TB_H, C_BDR);

    uint32_t scol = (focus_mode == 1) ? C_STARTF : C_START;
    fb_rect(2, fb_h - TB_H + 2, ST_W, TB_H - 4, scol);
    fb_txt(8, fb_h - TB_H + 6, "Start", C_TTT, scol);

    int bx = ST_W + 6;
    for (int i = 0; i < nw; i++) {
        uint32_t c = (i == act) ? C_TBTNA : C_TBTN;
        int bw = s_len(wins[i].title) * 8 + 12;
        if (bw > 140) bw = 140;
        fb_rect(bx, fb_h - TB_H + 2, bw, TB_H - 4, c);
        fb_txt(bx + 4, fb_h - TB_H + 6, wins[i].title, C_TTT, c);
        bx += bw + 2;
    }

    /* Clock */
    int h, m, s; rtc_read(&h, &m, &s);
    char tstr[6]; int ti = 0;
    tstr[ti++] = '0' + h / 10; tstr[ti++] = '0' + h % 10;
    tstr[ti++] = ':'; tstr[ti++] = '0' + m / 10; tstr[ti++] = '0' + m % 10;
    tstr[ti] = 0;
    int cx = fb_w - 4 - 8 * 5;
    fb_rect(cx, fb_h - TB_H, 8 * 6, TB_H, C_TBAR);
    fb_txt(cx, fb_h - TB_H + 6, tstr, C_TTT, C_TBAR);

    /* Start menu */
    if (menu_open) {
        int mx = 2;
        int my = fb_h - TB_H - menu_n * 24 - 4;
        fb_rect(mx, my, MN_W, menu_n * 24 + 4, C_MBG);
        fb_border(mx, my, MN_W, menu_n * 24 + 4, C_BDR);
        for (int i = 0; i < menu_n; i++) {
            uint32_t mc = (i == menu_focus && focus_mode == 2) ? C_MFOC : C_MBG;
            fb_rect(mx + 2, my + 2 + i * 24, MN_W - 4, 24, mc);
            fb_txt(mx + 6, my + 4 + i * 24, menu[i].name, C_TTT, mc);
        }
    }

    curs_save_area();
    curs_draw_at(mouse_x, mouse_y);
    fb_flip();
}
