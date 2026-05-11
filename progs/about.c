#include "../lunarsnow.h"

static int about_win = -1;

extern uint64_t total_ram;
extern char cpu_vendor[];
extern char cpu_brand[];
extern int boot_sec_total;
extern int cpu_ok;
extern int cpu_family, cpu_model, cpu_stepping;
extern char cpu_name[];

static void str_int(char *buf, int val)
{
    if (val == 0) { buf[0] = '0'; buf[1] = 0; return; }
    if (val < 0) { buf[0] = '-'; str_int(buf + 1, -val); return; }
    char tmp[16]; int i = 0;
    while (val > 0) { tmp[i++] = '0' + (val % 10); val /= 10; }
    int j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = 0;
}

static void draw(int wi)
{
    Win *w = &wins[wi];
    int wx = w->x + 10, wy = w->y + 28;
    char buf[64];

    fb_txt(wx, wy, "Made by: Lesano and Nixxlte :3", C_TTT, w->bg);
    fb_txt(wx, wy + 18, "OS Version: Alpha 0.2", C_LBL, w->bg);
    fb_txt(wx, wy + 35, "LunarUI Version: 0.0-3", C_LBL, w->bg);

    int dy = wy + 60;
    if (cpu_ok) {
        if (cpu_brand[0]) {
            int p = 0;
            for (int i = 0; cpu_brand[i] && i < 55; i++) buf[p++] = cpu_brand[i];
            buf[p] = 0;
            fb_txt(wx, dy, buf, C_LBL, w->bg);
            dy += 18;
        } else if (cpu_name[0]) {
            int pp = 0;
            for (int i = 0; cpu_name[i]; i++) buf[pp++] = cpu_name[i];
            buf[pp] = 0;
            fb_txt(wx, dy, buf, C_LBL, w->bg);
            dy += 18;
        }
        int pp = 0;
        const char *v = "Vendor: ";
        while (*v) buf[pp++] = *v++;
        for (int i = 0; cpu_vendor[i]; i++) buf[pp++] = cpu_vendor[i];
        buf[pp] = 0;
        fb_txt(wx, dy, buf, C_LBL, w->bg);
        dy += 18;
    }

    if (total_ram) {
        uint64_t mb = total_ram / (1024 * 1024);
        int p = 0;
        const char *r = "RAM: ";
        while (*r) buf[p++] = *r++;
        str_int(buf + p, (int)mb);
        while (buf[p]) p++;
        buf[p++] = ' '; buf[p++] = 'M'; buf[p++] = 'B'; buf[p] = 0;
        fb_txt(wx, dy, buf, C_LBL, w->bg);
        dy += 18;
    }

    {
        int h, m, s, p = 0;
        extern void rtc_read(int *h, int *m, int *s);
        rtc_read(&h, &m, &s);
        int secs = (h * 3600 + m * 60 + s) - boot_sec_total;
        if (secs < 0) secs += 86400;
        h = secs / 3600; m = (secs % 3600) / 60; s = secs % 60;
        const char *u = "Uptime: ";
        while (*u) buf[p++] = *u++;
        buf[p++] = '0' + h / 10; buf[p++] = '0' + h % 10;
        buf[p++] = ':'; buf[p++] = '0' + m / 10; buf[p++] = '0' + m % 10;
        buf[p++] = ':'; buf[p++] = '0' + s / 10; buf[p++] = '0' + s % 10;
        buf[p] = 0;
        fb_txt(wx, dy, buf, C_LBL, w->bg);
    }
}

void prog_about(void)
{
    about_win = gui_wnew("About", (fb_w - 500) / 2, (fb_h - 220) / 2, 500, 220);
    gui_wbtn(about_win, "OK", 220, 160, 60, 26, app_close);
    wins[about_win].draw = draw;
}
