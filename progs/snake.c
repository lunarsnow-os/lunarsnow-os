#include "../lunarsnow.h"

#define COLS 40
#define ROWS 30
#define MAXLEN (COLS * ROWS)

static int sb[MAXLEN][2], sl, sd, snd;
static int fx, fy, alive, score, speed, tick;
static int cell, win;
static int gw, gh, ww, wh;

static void spawn(void)
{
    for (int a = 0; a < 10000; a++) {
        fx = (fx * 7 + 13) % COLS;
        fy = (fy * 5 + 7) % ROWS;
        int ok = 1;
        for (int i = 0; i < sl; i++)
            if (sb[i][0] == fx && sb[i][1] == fy) { ok = 0; break; }
        if (ok) return;
    }
}

static void over(void)
{
    alive = 0;
    need_render = 1;
}

static void advance(void)
{
    sd = snd;
    int hx = sb[0][0], hy = sb[0][1];
    if (sd == 0) hy--;
    else if (sd == 1) hx++;
    else if (sd == 2) hy++;
    else hx--;

    if (hx < 0 || hx >= COLS || hy < 0 || hy >= ROWS) { over(); return; }
    for (int i = 0; i < sl; i++)
        if (sb[i][0] == hx && sb[i][1] == hy) { over(); return; }

    for (int i = sl; i > 0; i--)
        { sb[i][0] = sb[i - 1][0]; sb[i][1] = sb[i - 1][1]; }
    sb[0][0] = hx; sb[0][1] = hy;

    if (hx == fx && hy == fy) {
        sl++; score++; spawn();
    }
}

static void draw(int wi)
{
    Win *w = &wins[wi];
    int ox = w->x + 8, oy = w->y + 30;

    fb_rect(ox, oy, gw, gh, 0x0A0A1A);
    for (int x = 0; x <= COLS; x++)
        fb_rect(ox + x * cell - 1, oy, 1, gh, 0x151525);
    for (int y = 0; y <= ROWS; y++)
        fb_rect(ox, oy + y * cell - 1, gw, 1, 0x151525);

    fb_rect(ox + fx * cell + 2, oy + fy * cell + 2, cell - 4, cell - 4, 0xE03030);

    for (int i = 0; i < sl; i++) {
        uint32_t col = (i == 0) ? 0x30E030 : 0x209020;
        fb_rect(ox + sb[i][0] * cell + 1, oy + sb[i][1] * cell + 1, cell - 2, cell - 2, col);
    }

    if (!alive) {
        fb_rect(ox, oy, gw, gh, 0x40000050);
        fb_txt(w->x + ww / 2 - 5 * 4, w->y + wh / 2 - 4, "GAME OVER", 0xFF6060, 0);
    }

    char buf[32]; int p = 0;
    const char *s = "Score: ";
    while (*s) buf[p++] = *s++;
    str_int(buf + p, score);
    fb_txt(w->x + 10, w->y + wh - 18, buf, 0xC0C0C0, w->bg);
}

static void tick_fn(void)
{
    if (!alive || win < 0 || win >= nw) { gui_tick = 0; return; }
    if (wins[win].draw != draw) { gui_tick = 0; return; }
    tick++;
    if (tick >= speed) {
        tick = 0;
        advance();
        need_render = 1;
    }
}

static void key_fn(int k)
{
    win = act;
    if (!alive) {
        if (k == ' ' || k == '\n') {
            alive = 1; sl = 3; sd = 1; snd = 1; score = 0;
            sb[0][0] = COLS / 2; sb[0][1] = ROWS / 2;
            sb[1][0] = COLS / 2 - 1; sb[1][1] = ROWS / 2;
            sb[2][0] = COLS / 2 - 2; sb[2][1] = ROWS / 2;
            spawn(); need_render = 1;
        }
        return;
    }
    if (k == KEY_UP && sd != 2) snd = 0;
    else if (k == KEY_DOWN && sd != 0) snd = 2;
    else if (k == KEY_LEFT && sd != 1) snd = 3;
    else if (k == KEY_RIGHT && sd != 3) snd = 1;
    else if (k == '+' || k == '=') { if (speed > 1000) speed -= 1000; }
    else if (k == '-') { speed += 1000; }
}

static void on_close(int wi) { (void)wi; gui_tick = 0; }

void prog_snake(void)
{
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
    speed = cnt / 6;
    if (speed < 1) speed = 1;

    int max_cell_x = (fb_w - 16) / COLS;
    int max_cell_y = (fb_h - 38) / ROWS;
    cell = max_cell_x < max_cell_y ? max_cell_x : max_cell_y;
    if (cell < 4) cell = 4;
    if (cell > 20) cell = 20;
    int mw = fb_w * 4 / 5, mh = fb_h * 4 / 5;
    while (COLS * cell + 16 > mw || ROWS * cell + 16 > mh)
        cell--;
    if (cell < 4) cell = 4;
    gw = COLS * cell;
    gh = ROWS * cell;
    ww = gw + 16;
    wh = gh + 56;

    win = gui_wnew("Snake", (fb_w - ww) / 2, (fb_h - wh) / 2, ww, wh);
    alive = 1; sl = 3; sd = 1; snd = 1; score = 0; tick = 0;
    sb[0][0] = COLS / 2; sb[0][1] = ROWS / 2;
    sb[1][0] = COLS / 2 - 1; sb[1][1] = ROWS / 2;
    sb[2][0] = COLS / 2 - 2; sb[2][1] = ROWS / 2;
    spawn();
    gui_tick = tick_fn;
    wins[win].draw = draw;
    wins[win].on_key = key_fn;
    wins[win].on_close = on_close;
}
