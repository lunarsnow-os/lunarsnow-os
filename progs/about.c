#include "../lunarsnow.h"
#include "../config.h"

static int about_win = -1;

extern uint64_t total_ram;
extern char cpu_vendor[];
extern char cpu_brand[];
extern int boot_sec_total;
extern int cpu_ok;

static void draw(int wi)
{
    Win *w = &wins[wi];
    int wx = w->x + 10, wy = w->y + 28;
    char buf[64];

    fb_txt(wx, wy, "Made by: Lesano and Nixxlte :3", C_TTT, w->bg);
    fb_txt(wx, wy + 18, "OS Version: " OS_VER " " OS_ARCH " edition", C_LBL, w->bg);
    fb_txt(wx, wy + 35, UI_FULL, C_LBL, w->bg);

    int dy = wy + 60;
    if (cpu_ok) {
        if (cpu_brand[0]) {
            int p = 0;
            for (int i = 0; cpu_brand[i] && i < 55; i++) buf[p++] = cpu_brand[i];
            buf[p] = 0;
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
        int p = 0;
        const char *u = "Uptime: ";
        while (*u) buf[p++] = *u++;
        int h, m, s;
        rtc_read(&h, &m, &s);
        int secs = (h * 3600 + m * 60 + s) - boot_sec_total;
        if (secs < 0) secs += 86400;
        h = secs / 3600; m = (secs % 3600) / 60; s = secs % 60;
        buf[p++] = '0' + h / 10; buf[p++] = '0' + h % 10;
        buf[p++] = ':'; buf[p++] = '0' + m / 10; buf[p++] = '0' + m % 10;
        buf[p++] = ':'; buf[p++] = '0' + s / 10; buf[p++] = '0' + s % 10;
        buf[p] = 0;
        fb_txt(wx, dy, buf, C_LBL, w->bg);
        dy += 18;
    }

    {
        int p = 0, i;
        const char *u = "Display: ";
        while (*u) buf[p++] = *u++;
        i = fb_w;
        if (i >= 1000) buf[p++] = '0' + (i/1000)%10;
        if (i >= 100)  buf[p++] = '0' + (i/100)%10;
        if (i >= 10)   buf[p++] = '0' + (i/10)%10;
        buf[p++] = '0' + i%10;
        buf[p++] = 'x';
        i = fb_h;
        if (i >= 1000) buf[p++] = '0' + (i/1000)%10;
        if (i >= 100)  buf[p++] = '0' + (i/100)%10;
        if (i >= 10)   buf[p++] = '0' + (i/10)%10;
        buf[p++] = '0' + i%10;
        buf[p++] = '@'; buf[p++] = '3'; buf[p++] = '2';
        buf[p++] = 'b'; buf[p++] = 'p'; buf[p++] = 'p';
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
