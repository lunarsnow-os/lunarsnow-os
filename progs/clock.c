#include "lunarsnow.h"
#include "config.h"

static int last_sec = -1;

static int dow(int y, int m, int d)
{
    static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    y -= m < 3;
    return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

static int dim(int m, int y)
{
    static int dom[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0))
        return 29;
    return dom[m - 1];
}

static const char *mn[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static void tick_fn(void)
{
    int h, m, s;
    rtc_read(&h, &m, &s);
    if (s != last_sec) need_render = 1;
}

static void on_close(int wi)
{
    (void)wi;
    gui_tick = 0;
}

static void draw(int wi)
{
    Win *w = &wins[wi];
    int wx = w->x + 10, wy = w->y + 24;
    int hh, mm, ss, d, mo, y;
    char buf[32];
    int i;

    rtc_read(&hh, &mm, &ss);
    rtc_read_date(&d, &mo, &y);
    last_sec = ss;

    i = 0;
    buf[i++] = '0' + hh / 10; buf[i++] = '0' + hh % 10;
    buf[i++] = ':'; buf[i++] = '0' + mm / 10; buf[i++] = '0' + mm % 10;
    buf[i++] = ':'; buf[i++] = '0' + ss / 10; buf[i++] = '0' + ss % 10;
    buf[i] = 0;

    int cw = s_len(buf) * 8;
    int cx = wx + (w->w - 20 - cw) / 2;
    fb_rect(cx - 4, wy - 2, cw + 8, 20, 0x141428);
    fb_txt(cx, wy, buf, C_TTT, 0x141428);

    i = 0;
    static const char *dn[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    const char *sdn = dn[dow(y, mo, d)];
    while (*sdn) buf[i++] = *sdn++;
    buf[i++] = ','; buf[i++] = ' ';
    const char *smn = mn[mo - 1];
    while (*smn) buf[i++] = *smn++;
    buf[i++] = ' ';
    if (d >= 10) buf[i++] = '0' + d / 10;
    buf[i++] = '0' + d % 10;
    buf[i++] = ','; buf[i++] = ' ';
    int t = y;
    if (t >= 1000) buf[i++] = '0' + (t / 1000) % 10;
    if (t >= 100)  buf[i++] = '0' + (t / 100) % 10;
    if (t >= 10)   buf[i++] = '0' + (t / 10) % 10;
    buf[i++] = '0' + t % 10;
    buf[i] = 0;

    cw = s_len(buf) * 8;
    cx = wx + (w->w - 20 - cw) / 2;
    fb_txt(cx, wy + 22, buf, C_LBL, w->bg);

    int cal_y = wy + 50;
    int cell_w = 28, cell_h = 16;
    int cal_w = cell_w * 7;
    int cal_x = wx + (w->w - 20 - cal_w) / 2;

    i = 0;
    smn = mn[mo - 1];
    while (*smn) buf[i++] = *smn++;
    buf[i++] = ' ';
    t = y;
    if (t >= 1000) buf[i++] = '0' + (t / 1000) % 10;
    if (t >= 100)  buf[i++] = '0' + (t / 100) % 10;
    if (t >= 10)   buf[i++] = '0' + (t / 10) % 10;
    buf[i++] = '0' + t % 10;
    buf[i] = 0;

    cw = s_len(buf) * 8;
    cx = wx + (w->w - 20 - cw) / 2;
    fb_txt(cx, cal_y, buf, C_TAC, w->bg);

    cal_y += 18;

    for (i = 0; i < 7; i++)
        fb_txt(cal_x + i * cell_w, cal_y, dn[i], 0x6060A0, w->bg);

    cal_y += 18;

    int start_dow = dow(y, mo, 1);
    int days = dim(mo, y);
    int day = 1;
    int row = 0;
    while (day <= days) {
        for (int col = 0; col < 7 && day <= days; col++) {
            if (row == 0 && col < start_dow) continue;
            uint32_t colr = (day == d) ? C_TAC : C_TTT;
            int xx = col * cell_w;
            if (day == d)
                fb_rect(cal_x + xx, cal_y - 1, cell_w, cell_h, 0x2A3A6A);
            char db[4];
            db[0] = '0' + day / 10; db[1] = '0' + day % 10; db[2] = 0;
            fb_txt(cal_x + xx + 2, cal_y, db, colr, day == d ? 0x2A3A6A : w->bg);
            day++;
        }
        cal_y += cell_h;
        row++;
    }
}

void prog_clock(void)
{
    int wi = gui_wnew("Clock & Calendar", (fb_w - 260) / 2, (fb_h - 300) / 2, 260, 300);
    wins[wi].draw = draw;
    wins[wi].on_close = on_close;
    gui_tick = tick_fn;
}
