#include "../lunarsnow.h"
#include "../config.h"

static int about_win = -1;

extern uint64_t total_ram;
extern char cpu_vendor[];
extern char cpu_brand[];
extern int boot_sec_total;
extern int cpu_ok;
extern uint8_t *initrd_start;
extern uint32_t initrd_size;

static void draw_logo(int x, int y, uint32_t bg, uint32_t fg)
{
    uint32_t mc = 0xC8C8D2;
    int cx = x + 11, cy = y + 10;
    fb_rect(cx - 4, cy - 9, 9, 2, mc);
    fb_rect(cx - 7, cy - 7, 15, 2, mc);
    fb_rect(cx - 9, cy - 5, 19, 2, mc);
    fb_rect(cx - 10, cy - 3, 21, 2, mc);
    fb_rect(cx - 11, cy - 1, 23, 2, mc);
    fb_rect(cx - 11, cy + 1, 23, 2, mc);
    fb_rect(cx - 10, cy + 3, 21, 2, mc);
    fb_rect(cx - 9, cy + 5, 19, 2, mc);
    fb_rect(cx - 7, cy + 7, 15, 2, mc);
    fb_rect(cx - 4, cy + 9, 9, 2, mc);
    fb_rect(cx - 3, cy - 4, 4, 3, bg);
    fb_rect(cx + 3, cy + 1, 3, 3, bg);

    int sx = x + 30, sy = y + 3;
    fb_rect(sx - 1, sy + 5, 3, 11, fg);
    fb_rect(sx - 5, sy + 7, 11, 3, fg);
    fb_rect(sx - 5, sy + 5, 3, 3, fg);
    fb_rect(sx + 3, sy + 5, 3, 3, fg);
    fb_rect(sx - 5, sy + 11, 3, 3, fg);
    fb_rect(sx + 3, sy + 11, 3, 3, fg);
}

static void draw(int wi)
{
    Win *w = &wins[wi];
    int wx = w->x + 14, wy = w->y + 28;
    char buf[64];

    draw_logo(wx, wy, w->bg, C_LBL);

    fb_txt(wx + 48, wy, OS_NAME " x64 Edition", 0x7AB8E0, w->bg);
    fb_txt(wx + 48, wy + 18, "Version " OS_VER " (" UI_FULL ")", C_LBL, w->bg);

    fb_rect(wx, wy + 38, w->w - 28, 1, 0x3C50A0);

    int dy = wy + 54;

    if (cpu_ok && cpu_brand[0]) {
        int p = 0;
        for (int i = 0; cpu_brand[i] && i < 48; i++) buf[p++] = cpu_brand[i];
        buf[p] = 0;
        fb_txt(wx, dy, buf, C_LBL, w->bg);
        dy += 16;
    }
    if (cpu_ok) {
        int p = 0;
        const char *v = "Vendor: ";
        while (*v) buf[p++] = *v++;
        for (int i = 0; cpu_vendor[i]; i++) buf[p++] = cpu_vendor[i];
        buf[p] = 0;
        fb_txt(wx, dy, buf, C_LBL, w->bg);
        dy += 16;
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
        dy += 16;
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
        dy += 16;
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
        dy += 16;
    }

    if (initrd_start) {
        int p = 0;
        const char *ir = "Initrd: ";
        while (*ir) buf[p++] = *ir++;
        str_int(buf + p, initrd_size);
        while (buf[p]) p++;
        buf[p++] = ' '; buf[p++] = 'b'; buf[p] = 0;
        fb_txt(wx, dy, buf, C_LBL, w->bg);
        dy += 16;
    }

    fb_txt(wx, dy + 6, "Made by: Lesano and Nixxlte :3", 0x5A5A7A, w->bg);
}

void prog_about(void)
{
    about_win = gui_wnew("About", (fb_w - 500) / 2, (fb_h - 250) / 2, 500, 250);
    wins[about_win].draw = draw;
}
