#include <stdint.h>
#include "lunarsnow.h"
#include "font8x16.h"
#include "fb.h"
#include "input.h"
#include "gui.h"
#include "smbus.h"

/* ================================================================
   STATE
   ================================================================ */

Win wins[MAX_W];
int nw, act, run;
int focus_mode, menu_open, menu_focus;
int mouse_drag, mouse_drag_win, mouse_drag_ox, mouse_drag_oy;
int close_on_esc;
int starfield_active;

/* Dynamic menu */
#define MENU_MAX 24
static struct { const char *name; void (*cb)(void); int is_header; } menu[MENU_MAX];
static int menu_n;

void gui_menu_add(const char *name, void (*cb)(void)) {
    if (menu_n < MENU_MAX) { menu[menu_n].name = name; menu[menu_n].cb = cb; menu[menu_n].is_header = 0; menu_n++; }
}

void gui_menu_header(const char *name) {
    if (menu_n < MENU_MAX) { menu[menu_n].name = name; menu[menu_n].cb = 0; menu[menu_n].is_header = 1; menu_n++; }
}

int gui_menu_count(void) { return menu_n; }

int gui_menu_prev(int i) {
    int n = gui_menu_count();
    if (n <= 0) return 0;
    do { i = (i - 1 + n) % n; } while (menu[i].is_header);
    return i;
}

int gui_menu_next(int i) {
    int n = gui_menu_count();
    if (n <= 0) return 0;
    do { i = (i + 1) % n; } while (menu[i].is_header);
    return i;
}

void gui_menu_exec(int i) { if (i >= 0 && i < menu_n && !menu[i].is_header && menu[i].cb) menu[i].cb(); }

/* ================================================================
   WINDOW MANAGEMENT
   ================================================================ */

int gui_wnew(const char *t, int x, int y, int w, int h) {
    if (nw >= MAX_W) return -1;
    int i = nw++;
    wins[i].x = x; wins[i].y = y; wins[i].w = w; wins[i].h = h;
    s_cpy(wins[i].title, t, 24);
    wins[i].bg = C_WBG; wins[i].tb = C_TAC;
    wins[i].nb = 0; wins[i].fc = 0; wins[i].minimized = 0;
    wins[i].draw = 0; wins[i].on_key = 0; wins[i].on_click = 0;
    wins[i].on_rclick = 0; wins[i].on_close = 0;
    wins[i].userdata = 0;
    act = i; return i;
}

int gui_wbtn(int wi, const char *t, int x, int y, int w, int h, void (*cb)(void)) {
    if (wi < 0 || wi >= nw) return -1;
    Btn *b = &wins[wi].btns[wins[wi].nb++];
    b->x = x; b->y = y; b->w = w; b->h = h; b->cb = cb;
    s_cpy(b->t, t, 24); return wins[wi].nb - 1;
}

void gui_wclose(int idx) {
    if (idx < 0 || idx >= nw) return;
    if (wins[idx].on_close) wins[idx].on_close(idx);
    for (int i = idx; i < nw - 1; i++) mcpy(&wins[i], &wins[i+1], sizeof(Win));
    nw--;
    if (act >= nw && nw > 0) act = nw - 1;
}

void gui_wfront(int idx) {
    if (idx < 0 || idx >= nw || idx == nw - 1) { act = idx; return; }
    Win tmp;
    mcpy(&tmp, &wins[idx], sizeof(Win));
    for (int i = idx; i < nw - 1; i++) mcpy(&wins[i], &wins[i+1], sizeof(Win));
    mcpy(&wins[nw - 1], &tmp, sizeof(Win));
    act = nw - 1;
}

/* ================================================================
   WINDOW DRAW
   ================================================================ */

static void wdraw(int wi) {
    Win *w = &wins[wi];
    if (w->minimized) return;
    int x = w->x, y = w->y, ww = w->w, hh = w->h;
    int a = (wi == act);
    uint32_t tc = a ? C_TAC : C_TIN;
    uint32_t bc = a ? C_BDR : C_TIN;

    /* Shadow */
    fb_rect(x + 4, y + 4, ww, hh, 0x05050A);

    fb_rect(x + 2, y + 20, ww - 4, hh - 22, w->bg);
    fb_rect(x + 2, y + 2, ww - 4, 18, tc);
    fb_txt(x + 6, y + 3, w->title, C_TTT, tc);

    /* Minimize button */
    int mbx = x + ww - 42, mby = y + 3;
    fb_rect(mbx, mby, 15, 15, tc);
    fb_rect(mbx + 3, mby + 11, 9, 2, C_TTT);

    /* Close button — draw X with lines */
    int cbx = x + ww - 18, cby = y + 3;
    fb_rect(cbx, cby, 15, 15, C_CLS);
    for (int i = 3; i < 12; i++) {
        fb_pixel(cbx + i, cby + i, C_TTT);
        fb_pixel(cbx + 14 - i, cby + i, C_TTT);
    }

    fb_border(x, y, ww, hh, bc);

    for (int i = 0; i < w->nb; i++) {
        Btn *b = &w->btns[i];
        int bx = x + 2 + b->x, by = y + 20 + b->y;
        int f = (i == w->fc && wi == act);
        uint32_t col = f ? C_BF : C_BN;
        fb_rect(bx, by, b->w, b->h, col);
        uint32_t light = f ? 0x8ADADA : 0x4A9696;
        uint32_t dark  = f ? 0x1A3E3E : 0x0A2020;
        fb_rect(bx, by, b->w, 1, light);
        fb_rect(bx, by, 1, b->h, light);
        fb_rect(bx, by + b->h - 1, b->w, 1, dark);
        fb_rect(bx + b->w - 1, by, 1, b->h, dark);
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
    0x82, 0x81, 0xFF, 0x7E, 0x3C, 0x18
};

static uint32_t curs_save[CUR_H][CUR_W];
static int curs_sx = -1, curs_sy = -1;

void gui_reset_cursor(void) { curs_sx = -1; curs_sy = -1; }

int need_render = 1;
void (*gui_tick)(void);
void gui_set_dirty(void) { need_render = 1; }

int hovered_app = -1;
int hovered_start = 0;
int hovered_menu = -1;

void gui_hover_check(void) {
    int mx = mouse_x, my = mouse_y;
    int new_hover = -1;
    int new_start = 0;
    int new_menuh = -1;

    if (menu_open) {
        int my2 = fb_h - TB_H - menu_n * MI_H - 8;
        if (mx >= 2 && mx < 2 + MN_W && my >= my2 && my < my2 + menu_n * MI_H + 4) {
            new_menuh = (my - my2 - 2) / MI_H;
            if (new_menuh < 0 || new_menuh >= menu_n) new_menuh = -1;
            if (new_menuh >= 0 && !menu[new_menuh].is_header) {
                menu_focus = new_menuh;
                focus_mode = 2;
            }
        }
    }

    if (my >= fb_h - TB_H && my < fb_h) {
        if (mx < ST_W + 2) {
            new_start = 1;
        } else {
            int bx = ST_W + 6;
            for (int i = 0; i < nw; i++) {
                int bw = s_len(wins[i].title) * 8 + 12;
                if (bw > 140) bw = 140;
                if (mx >= bx && mx < bx + bw) { new_hover = i; break; }
                bx += bw + 2;
            }
        }
    }

    if (new_hover != hovered_app || new_start != hovered_start || new_menuh != hovered_menu) {
        hovered_app = new_hover;
        hovered_start = new_start;
        hovered_menu = new_menuh;
        need_render = 1;
    }
}

static void curs_restore(void) {
    int x = curs_sx, y = curs_sy;
    if (x < 0) return;
    for (int r = 0; r < CUR_H && y + r < fb_h; r++)
        for (int c = 0; c < CUR_W && x + c < fb_w; c++)
            sbuf[(y + r) * fb_w + (x + c)] = curs_save[r][c];
}

static void curs_save_area(void) {
    int x = mouse_x, y = mouse_y;
    for (int r = 0; r < CUR_H && y + r < fb_h; r++)
        for (int c = 0; c < CUR_W && x + c < fb_w; c++)
            curs_save[r][c] = sbuf[(y + r) * fb_w + (x + c)];
    curs_sx = x; curs_sy = y;
}

static void curs_draw_at(int x, int y) {
    int nr = 12;
    for (int r = 0; r < nr && y + r + 1 < fb_h; r++) {
        uint8_t bits = curs_img[r];
        for (int c = 0; c < 8 && x + c + 1 < fb_w; c++)
            if (bits & (0x80 >> c))
                sbuf[(y + r + 1) * fb_w + (x + c + 1)] = 0x000000;
    }
    for (int r = 0; r < nr && y + r < fb_h; r++) {
        uint8_t bits = curs_img[r];
        for (int c = 0; c < 8 && x + c < fb_w; c++)
            if (bits & (0x80 >> c))
                sbuf[(y + r) * fb_w + (x + c)] = 0xFFFFFF;
    }
}

void gui_update_cursor(void) {
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

void gui_mouse_click(void) {
    int x = mouse_x, y = mouse_y;

    if (menu_open) {
        if (hovered_menu >= 0 && !menu[hovered_menu].is_header && menu[hovered_menu].cb)
            menu[hovered_menu].cb();
        menu_open = 0; focus_mode = 0;
        return;
    }

    if (y >= fb_h - TB_H) {
        if (x < ST_W + 2) {
            menu_open = 1; menu_focus = gui_menu_next(-1); focus_mode = 2;
            return;
        }
        int bx = ST_W + 6;
        for (int i = 0; i < nw; i++) {
            int bw = s_len(wins[i].title) * 8 + 12;
            if (bw > 140) bw = 140;
            if (x >= bx && x < bx + bw) {
                if (wins[i].minimized) {
                    wins[i].minimized = 0;
                    gui_wfront(i);
                } else if (i == act) {
                    wins[i].minimized = 1;
                    int found = 0;
                    for (int j = nw - 1; j >= 0; j--)
                        if (j != i && !wins[j].minimized) { gui_wfront(j); found = 1; break; }
                    if (!found) act = -1;
                } else {
                    gui_wfront(i);
                }
                focus_mode = 0; return;
            }
            bx += bw + 2;
        }
        return;
    }

    for (int i = nw - 1; i >= 0; i--) {
        Win *w = &wins[i];
        int wx = w->x, wy = w->y, ww = w->w, wh = w->h;
        if (x < wx || x >= wx + ww || y < wy || y >= wy + wh) continue;
        focus_mode = 0;

        if (x >= wx + ww - 18 && x < wx + ww - 3 &&
            y >= wy + 3 && y < wy + 18) {
            gui_wclose(i); return;
        }
        if (x >= wx + ww - 42 && x < wx + ww - 27 &&
            y >= wy + 3 && y < wy + 18) {
            wins[i].minimized = 1;
            int found = 0;
            for (int j = nw - 1; j >= 0; j--)
                if (j != i && !wins[j].minimized) { gui_wfront(j); found = 1; break; }
            if (!found && act == i) act = -1;
            need_render = 1; return;
        }
        if (y >= wy + 2 && y < wy + 20) {
            gui_wfront(i);
            mouse_drag = 1;
            mouse_drag_win = act;
            mouse_drag_ox = x - wx;
            mouse_drag_oy = y - wy;
            return;
        }
        gui_wfront(i);
        w = &wins[act];
        wx = w->x; wy = w->y;
        for (int j = 0; j < w->nb; j++) {
            Btn *b = &w->btns[j];
            int bx = wx + 2 + b->x, by = wy + 20 + b->y;
            if (x >= bx && x < bx + b->w && y >= by && y < by + b->h) {
                if (b->cb) b->cb();
                return;
            }
        }
        if (w->on_click) { w->on_click(act); return; }
        return;
    }
}

/* ================================================================
   RENDER
   ================================================================ */

void gui_render(void) {
    fb_rect(0, 0, fb_w, fb_h, C_DSK);
    for (int i = 0; i < nw; i++) wdraw(i);

    /* Taskbar */
    fb_rect(0, fb_h - TB_H, fb_w, TB_H, C_TBAR);
    fb_border(0, fb_h - TB_H, fb_w, TB_H, C_BDR);

    /* Start button */
    uint32_t scol = (focus_mode == 1 || hovered_start) ? C_STARTF : C_START;
    fb_rect(2, fb_h - TB_H + 2, ST_W, TB_H - 4, scol);
    int siy = fb_h - TB_H + 7;
    fb_rect(6, siy, 4, 4, 0x5A8ACC);
    fb_rect(6, siy + 8, 4, 4, 0x5A8ACC);
    fb_rect(12, siy, 4, 4, 0x5A8ACC);
    fb_rect(12, siy + 8, 4, 4, 0x5A8ACC);
    fb_txt(20, fb_h - TB_H + 6, "Start", C_TTT, scol);

    int bx = ST_W + 6;
    int max_bx, sep_x;
    if (battery_present) {
        max_bx = fb_w - 146;
        sep_x = fb_w - 76;
    } else {
        max_bx = fb_w - 76;
        sep_x = max_bx + 4;
    }
    for (int i = 0; i < nw; i++) {
        uint32_t c = (i == act) ? C_TBTNA : (i == hovered_app) ? 0x4A5AB0 : C_TBTN;
        if (wins[i].minimized) c = 0x22223A;
        int bw = s_len(wins[i].title) * 8 + 12;
        if (bw > 140) bw = 140;
        if (bx + bw >= max_bx) bw = max_bx - bx - 2;
        if (bw < 12) break;
        fb_rect(bx, fb_h - TB_H + 2, bw, TB_H - 4, c);
        if (i == act)
            fb_rect(bx, fb_h - TB_H + 2, bw, 2, 0x6A90D0);
        if (i == hovered_app && i != act)
            fb_rect(bx, fb_h - TB_H + 2, bw, 2, 0x3C5AA0);
        int maxc = (bw - 8) / 8;
        if (maxc < 1) maxc = 1;
        char trim[24];
        s_cpy(trim, wins[i].title, maxc + 1);
        fb_txt(bx + 4, fb_h - TB_H + 6, trim, C_TTT, c);
        bx += bw + 2;
    }
    /* Fill remainder of taskbar */
    fb_rect(bx, fb_h - TB_H + 2, max_bx - bx - 2, TB_H - 4, C_TBAR);

    /* Battery indicator */
    if (battery_present) {
        int ba_x = max_bx + 4;
        int ba_iy = fb_h - TB_H + (TB_H - 10) / 2;
        uint32_t ba_c = battery_percent < 10 ? 0xFF3030
                      : battery_percent < 20 ? 0xFF9030
                      : battery_charging  ? 0x30E030 : 0x60C060;
        int fill = (12 * battery_percent) / 100;
        if (fill < 0) fill = 0;
        if (fill > 12) fill = 12;
        fb_border(ba_x, ba_iy, 14, 10, ba_c);
        if (fill > 0) fb_rect(ba_x + 1, ba_iy + 1, fill, 8, ba_c);
        fb_rect(ba_x + 14, ba_iy + 3, 2, 4, ba_c);
        if (battery_charging) {
            fb_txt(ba_x + 3, ba_iy + 1, "~", 0x16162A, ba_c);
        }
        char buf[8]; int bi = 0;
        str_int(buf, battery_percent);
        while (buf[bi]) bi++;
        buf[bi++] = '%'; buf[bi] = 0;
        fb_txt(ba_x + 20, fb_h - TB_H + 6, buf, ba_c, C_TBAR);
    }

    /* Separator */
    fb_rect(sep_x, fb_h - TB_H + 4, 1, TB_H - 8, 0x3C3C60);

    /* Clock with seconds */
    int h, m, s; rtc_read(&h, &m, &s);
    char tstr[9]; int ti = 0;
    tstr[ti++] = '0' + h / 10; tstr[ti++] = '0' + h % 10;
    tstr[ti++] = ':'; tstr[ti++] = '0' + m / 10; tstr[ti++] = '0' + m % 10;
    tstr[ti++] = ':'; tstr[ti++] = '0' + s / 10; tstr[ti++] = '0' + s % 10;
    tstr[ti] = 0;
    int cx = fb_w - 4 - 8 * 8;
    fb_rect(cx, fb_h - TB_H, 8 * 9, TB_H, C_TBAR);
    fb_txt(cx, fb_h - TB_H + 6, tstr, C_TTT, C_TBAR);

    /* Start menu — modern style */
    if (menu_open) {
        int mx = 2;
        int my = fb_h - TB_H - menu_n * MI_H - 8;

        /* Shadow (doesn't overlap taskbar) */
        fb_rect(mx + 4, my + 4, MN_W, menu_n * MI_H, 0x05050A);

        /* Background */
        fb_rect(mx, my, MN_W, menu_n * MI_H + 4, C_MBG);

        for (int i = 0; i < menu_n; i++) {
            int iy = my + 2 + i * MI_H;
            int is_hov = (i == hovered_menu) || (i == menu_focus && focus_mode == 2);

            if (menu[i].is_header) {
                fb_txt(mx + 8, iy + 2, menu[i].name, 0x5A7AD0, C_MBG);
                fb_rect(mx + 8, iy + MI_H - 2, MN_W - 16, 1, 0x2A2A48);
            } else if (is_hov) {
                fb_rect(mx + 4, iy, MN_W - 8, MI_H, C_MFOC);
                fb_txt(mx + 8, iy + 2, menu[i].name, C_TTT, C_MFOC);
            } else {
                fb_txt(mx + 8, iy + 2, menu[i].name, C_TTT, C_MBG);
            }
        }
    }

    curs_save_area();
    curs_draw_at(mouse_x, mouse_y);
    fb_flip();
}
