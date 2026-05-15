#include "lunarsnow.h"

#define C 10
#define R 20
#define B 24
static int drop_interval;

static const int pd[7][4][2] = {
    {{0,0},{0,1},{0,2},{0,3}},
    {{0,0},{0,1},{1,0},{1,1}},
    {{0,0},{0,1},{0,2},{1,1}},
    {{1,0},{1,1},{0,1},{0,2}},
    {{0,0},{0,1},{1,1},{1,2}},
    {{0,0},{1,0},{1,1},{1,2}},
    {{0,2},{1,0},{1,1},{1,2}},
};

static const uint32_t pc[8] = {
    0x00F0F0, 0xF0F000, 0xA000F0, 0x00F000,
    0xF00000, 0x0000F0, 0xF0A000, 0x404040
};

static uint32_t g[R][C];
static int ct, cr, cq, cc;
static int nt;
static int sc, ln, lv;
static int tmr;
static int go;
static int wi;

static void gb(int t, int rr, int b, int *dr, int *dc) {
    int r = pd[t][b][0], c = pd[t][b][1];
    for (int i = 0; i < rr; i++) { int x = r; r = c; c = 3 - x; }
    *dr = r; *dc = c;
}

static int v(int t, int rr, int r, int c) {
    for (int b = 0; b < 4; b++) {
        int dr, dc; gb(t, rr, b, &dr, &dc);
        int rr2 = r + dr, cc2 = c + dc;
        if (cc2 < 0 || cc2 >= C || rr2 >= R) return 0;
        if (rr2 >= 0 && g[rr2][cc2]) return 0;
    }
    return 1;
}

/* Basic wall kick: try rotation, then shift left/right */
static int try_rot(void) {
    int nr = (cr + 1) % 4;
    if (v(ct, nr, cq, cc)) { cr = nr; return 1; }
    if (v(ct, nr, cq, cc - 1)) { cr = nr; cc--; return 1; }
    if (v(ct, nr, cq, cc + 1)) { cr = nr; cc++; return 1; }
    return 0;
}

static void lk(void) {
    for (int b = 0; b < 4; b++) {
        int dr, dc; gb(ct, cr, b, &dr, &dc);
        int rr = cq + dr, cc2 = cc + dc;
        if (rr >= 0) g[rr][cc2] = pc[ct];
    }
}

static void nlines(void) {
    int n = 0;
    for (int r = R - 1; r >= 0; r--) {
        int f = 1;
        for (int c = 0; c < C; c++) if (!g[r][c]) { f = 0; break; }
        if (f) {
            n++;
            for (int rr = r; rr > 0; rr--)
                for (int cc = 0; cc < C; cc++)
                    g[rr][cc] = g[rr-1][cc];
            for (int cc = 0; cc < C; cc++) g[0][cc] = 0;
            r++;
        }
    }
    if (n) { ln += n; sc += n * 100 * (n > 1 ? n : 1); lv = ln / 10; need_render = 1; }
}

static void np(void) {
    ct = nt; nt = (nt + 1) % 7;
    cr = 0; cq = -1; cc = C / 2 - 2;
    if (!v(ct, cr, cq, cc)) go = 1;
}

static void dr(void) {
    if (go) return;
    if (v(ct, cr, cq + 1, cc)) { cq++; need_render = 1; return; }
    lk(); nlines(); np(); need_render = 1;
}

static void hd(void) {
    if (go) return;
    while (v(ct, cr, cq + 1, cc)) cq++;
    dr();
}

static void draw_fn(int wix);

static void tick_fn(void) {
    if (wi < 0 || wi >= nw) { gui_tick = 0; return; }
    if (wins[wi].draw != draw_fn) { gui_tick = 0; return; }
    if (go) return;
    int speed = drop_interval - lv * (drop_interval / 20);
    if (speed < drop_interval / 20) speed = drop_interval / 20;
    if (++tmr >= speed) { tmr = 0; dr(); }
}

static void key_fn(int k) {
    wi = act;
    if (go) {
        if (k == ' ' || k == '\n') {
            for (int r = 0; r < R; r++)
                for (int c = 0; c < C; c++)
                    g[r][c] = 0;
            sc = 0; ln = 0; lv = 0; tmr = 0; go = 0;
            ct = 0; nt = 1; cr = 0; cq = -1; cc = C / 2 - 2;
            need_render = 1;
        }
        return;
    }
    if (k == KEY_LEFT && v(ct, cr, cq, cc - 1)) { cc--; need_render = 1; }
    else if (k == KEY_RIGHT && v(ct, cr, cq, cc + 1)) { cc++; need_render = 1; }
    else if (k == KEY_UP) { need_render = try_rot(); }
    else if (k == KEY_DOWN) dr();
    else if (k == ' ') hd();
}

static void draw_fn(int wix) {
    Win *w = &wins[wix];
    int ox = w->x + 4, oy = w->y + 22;

    fb_rect(ox, oy, B * C + 4, B * R + 4, 0x000000);
    fb_border(ox, oy, B * C + 4, B * R + 4, 0x505078);

    for (int r = 0; r < R; r++)
        for (int c = 0; c < C; c++) {
            uint32_t col = g[r][c] ? g[r][c] : 0x0A0A1E;
            fb_rect(ox + 2 + c * B + 1, oy + 2 + r * B + 1, B - 2, B - 2, col);
        }

    if (!go)
        for (int b = 0; b < 4; b++) {
            int dr, dc; gb(ct, cr, b, &dr, &dc);
            int rr = cq + dr, cc2 = cc + dc;
            if (rr >= 0)
                fb_rect(ox + 2 + cc2 * B + 1, oy + 2 + rr * B + 1, B - 2, B - 2, pc[ct]);
        }

    /* Ghost piece */
    int gr = cq;
    while (v(ct, cr, gr + 1, cc)) gr++;
    for (int b = 0; b < 4 && gr != cq; b++) {
        int dr, dc; gb(ct, cr, b, &dr, &dc);
        int rr = gr + dr, cc2 = cc + dc;
        if (rr >= 0) {
            int gx = ox + 2 + cc2 * B, gy = oy + 2 + rr * B;
            fb_border(gx + 2, gy + 2, B - 4, B - 4, 0x444466);
        }
    }

    /* Info panel */
    int ix = ox + B * C + 16;
    int iy = oy + 8;

    fb_txt(ix, iy, "NEXT", C_TAC, w->bg);
    for (int b = 0; b < 4; b++) {
        int dr, dc; gb(nt, 0, b, &dr, &dc);
        fb_rect(ix + dc * 18 + 1, iy + 18 + dr * 18 + 1, 16, 16, pc[nt]);
    }

    char buf[16];
    fb_txt(ix, iy + 100, "SCORE", C_TAC, w->bg);
    str_int(buf, sc);
    fb_txt(ix, iy + 116, buf, C_TTT, w->bg);

    fb_txt(ix, iy + 148, "LINES", C_TAC, w->bg);
    str_int(buf, ln);
    fb_txt(ix, iy + 164, buf, C_TTT, w->bg);

    fb_txt(ix, iy + 196, "LEVEL", C_TAC, w->bg);
    str_int(buf, lv);
    fb_txt(ix, iy + 212, buf, C_TTT, w->bg);

    if (go) {
        fb_rect(ix, iy + 240, 120, 40, w->bg);
        fb_txt(ix + 4, iy + 244, "GAME OVER", 0xF04040, w->bg);
    }

    fb_txt(ix, iy + 280, "Arrows: move", C_TAC, w->bg);
    fb_txt(ix, iy + 296, "Up: rotate", C_TAC, w->bg);
    fb_txt(ix, iy + 312, "Space: hard drop", C_TAC, w->bg);
}

static void on_close(int wix) { (void)wix; gui_tick = 0; }

void prog_tetris(void) {
    /* Calibrate speed */
    int h, m, s;
    rtc_read(&h, &m, &s);
    int start = h * 3600 + m * 60 + s;
    volatile int cnt = 0;
    for (int t = 0; t < 10000000; t++) {
        cnt++;
        rtc_read(&h, &m, &s);
        int now = h * 3600 + m * 60 + s;
        if (now != start) break;
    }
    if (cnt < 100) cnt = 100000;
    drop_interval = cnt * 3 / 4;
    if (drop_interval < 1) drop_interval = 1;

    for (int r = 0; r < R; r++)
        for (int c = 0; c < C; c++)
            g[r][c] = 0;
    sc = 0; ln = 0; lv = 0; tmr = 0; go = 0;
    ct = 0; nt = 1; cr = 0; cq = -1; cc = C / 2 - 2;

    wi = gui_wnew("Tetris", (fb_w - 420) / 2, (fb_h - 510) / 2, 420, 510);
    wins[wi].draw = draw_fn;
    wins[wi].on_key = key_fn;
    wins[wi].on_close = on_close;
    gui_tick = tick_fn;
    close_on_esc = 1;
    need_render = 1;
}
