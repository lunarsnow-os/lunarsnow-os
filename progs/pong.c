#include "lunarsnow.h"
#include "config.h"

static int pong_win = -1;
static int ball_x, ball_y, ball_dx, ball_dy;
static int pad1, pad2;
static int pw = 6, ph = 50, bs = 6;
static int po = 30;
static int cw, ch;
static int score1, score2;
static int px, py;
static int tick_div, tick_cnt;
static int ticks_this_sec;
static int last_h, last_m, last_s;

static void reset_ball(void)
{
    ball_x = cw / 2 - bs / 2;
    ball_y = ch / 2 - bs / 2;
    ball_dx = (ball_dx > 0) ? -1 : 1;
    ball_dy = (ball_dy > 0) ? -1 : 1;
}

static void tick_fn(void)
{
    tick_cnt++;
    ticks_this_sec++;

    /* Recalibrate once per RTC second (accounts for rendering overhead) */
    int h, m, s;
    rtc_read(&h, &m, &s);
    int now = h * 3600 + m * 60 + s;
    int prev = last_h * 3600 + last_m * 60 + last_s;
    if (now != prev) {
        tick_div = ticks_this_sec / 60;
        if (tick_div < 1) tick_div = 1;
        ticks_this_sec = 0;
        last_h = h; last_m = m; last_s = s;
    }

    /* AI on every tick */
    if (pad2 + ph / 2 < ball_y + bs / 2 - 3) pad2 += 3;
    else if (pad2 + ph / 2 > ball_y + bs / 2 + 3) pad2 -= 3;
    if (pad2 < 0) pad2 = 0;
    if (pad2 + ph > ch) pad2 = ch - ph;

    /* Player paddle follows mouse */
    Win *w = &wins[pong_win];
    pad1 = mouse_y - w->y - 28 - ph / 2;
    if (pad1 < 0) pad1 = 0;
    if (pad1 + ph > ch) pad1 = ch - ph;

    if (tick_cnt % tick_div != 0) return;

    /* Move ball */
    ball_x += ball_dx * 2;
    ball_y += ball_dy * 2;

    /* Top/bottom bounce */
    if (ball_y < 0) { ball_y = 0; ball_dy = 1; }
    if (ball_y + bs > ch) { ball_y = ch - bs; ball_dy = -1; }

    /* Left paddle (player) */
    if (ball_dx < 0 && ball_x <= po + pw && ball_x > po) {
        if (ball_y + bs > pad1 && ball_y < pad1 + ph) {
            ball_x = po + pw;
            ball_dx = 1;
        }
    }

    /* Right paddle (AI) */
    if (ball_dx > 0 && ball_x + bs >= cw - po - pw && ball_x + bs < cw - po) {
        if (ball_y + bs > pad2 && ball_y < pad2 + ph) {
            ball_x = cw - po - pw - bs;
            ball_dx = -1;
        }
    }

    /* Scoring */
    if (ball_x < -bs) { score2++; reset_ball(); }
    if (ball_x > cw) { score1++; reset_ball(); }

    need_render = 1;
}

static void on_close(int wi)
{
    (void)wi;
    gui_tick = 0;
}

static void on_key(int k)
{
    if (k == 'w' || k == 'W' || k == KEY_UP) {
        pad1 -= 12;
        if (pad1 < 0) pad1 = 0;
    }
    if (k == 's' || k == 'S' || k == KEY_DOWN) {
        pad1 += 12;
        if (pad1 + ph > ch) pad1 = ch - ph;
    }
}

static void draw(int wi)
{
    Win *w = &wins[wi];
    int wx = w->x, wy = w->y;

    fb_rect(wx + px, wy + py, cw, ch, 0x0A0A14);

    int cx = wx + px + cw / 2 - 1;
    for (int y = wy + py + 4; y < wy + py + ch; y += 12) {
        int hh = 6;
        if (y + hh > wy + py + ch) hh = wy + py + ch - y;
        fb_rect(cx, y, 2, hh, 0x2A2A44);
    }

    fb_rect(wx + px + po,          wy + py + pad1, pw, ph, C_TTT);
    fb_rect(wx + px + cw - po - pw, wy + py + pad2, pw, ph, C_TTT);
    fb_rect(wx + px + ball_x, wy + py + ball_y, bs, bs, C_TAC);

    char buf[8];
    int ti;
    ti = 0; buf[ti++] = '0' + score1; buf[ti] = 0;
    fb_txt(wx + cw / 2 - 40, wy + 24, buf, C_TTT, w->bg);
    ti = 0; buf[ti++] = '0' + score2; buf[ti] = 0;
    fb_txt(wx + cw / 2 + 32, wy + 24, buf, C_TTT, w->bg);
}

void prog_pong(void)
{
    tick_div = 10;
    tick_cnt = 0;
    ticks_this_sec = 0;

    int ww = 500, wh = 360;
    int mw = fb_w * 4 / 5, mh = fb_h * 4 / 5;
    if (ww > mw) ww = mw;
    if (wh > mh) wh = mh;

    px = 8; py = 26;
    cw = ww - 16;
    ch = wh - py - 8;

    pong_win = gui_wnew("Pong", (fb_w - ww) / 2, (fb_h - wh) / 2, ww, wh);

    rtc_read(&last_h, &last_m, &last_s);

    pad1 = ch / 2 - ph / 2;
    pad2 = ch / 2 - ph / 2;
    score1 = 0; score2 = 0;
    ball_x = cw / 2 - bs / 2;
    ball_y = ch / 2 - bs / 2;
    ball_dx = 1; ball_dy = 1;

    wins[pong_win].draw = draw;
    wins[pong_win].on_key = on_key;
    wins[pong_win].on_close = on_close;
    gui_tick = tick_fn;
}
