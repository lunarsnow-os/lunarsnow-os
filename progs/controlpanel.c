#include "../lunarsnow.h"

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

static void drawMain(int wi) {
    Win *w = &wins[wi];
    int wx = w->x + 12, wy = w->y + 28;
    char buf[64]; int p;

    fb_txt(wx, wy, "Control Panel", C_TTT, w->bg);
    fb_rect(wx, wy + 20, w->w - 24, 1, 0x3C50A0);
    wy += 34;

    fb_txt(wx, wy, "System", 0x5A7AC0, w->bg); wy += 18;

    extern int cpu_ok;
    if (cpu_ok) {
        extern char cpu_brand[];
        if (cpu_brand[0]) {
            p = 0;
            for (int i = 0; cpu_brand[i] && i < 48; i++) buf[p++] = cpu_brand[i];
            buf[p] = 0;
            fb_txt(wx + 8, wy, buf, C_LBL, w->bg); wy += 16;
        }
        extern char cpu_vendor[];
        p = 0;
        const char *vn = "Vendor: ";
        while (*vn) buf[p++] = *vn++;
        for (int i = 0; cpu_vendor[i]; i++) buf[p++] = cpu_vendor[i];
        buf[p] = 0;
        fb_txt(wx + 8, wy, buf, C_LBL, w->bg); wy += 16;
    }

    extern uint64_t total_ram;
    if (total_ram) {
        p = 0;
        const char *r = "RAM: ";
        while (*r) buf[p++] = *r++;
        str_int(buf + p, (int)(total_ram / (1024 * 1024)));
        while (buf[p]) p++;
        buf[p++] = ' '; buf[p++] = 'M'; buf[p++] = 'B'; buf[p] = 0;
        fb_txt(wx + 8, wy, buf, C_LBL, w->bg); wy += 16;
    }

    extern int boot_sec_total;
    {
        int h, m, s;
        extern void rtc_read(int *h, int *m, int *s);
        rtc_read(&h, &m, &s);
        int secs = (h * 3600 + m * 60 + s) - boot_sec_total;
        if (secs < 0) secs += 86400;
        h = secs / 3600; m = (secs % 3600) / 60; s = secs % 60;
        p = 0;
        const char *u = "Uptime: ";
        while (*u) buf[p++] = *u++;
        buf[p++] = '0' + h / 10; buf[p++] = '0' + h % 10;
        buf[p++] = ':'; buf[p++] = '0' + m / 10; buf[p++] = '0' + m % 10;
        buf[p++] = ':'; buf[p++] = '0' + s / 10; buf[p++] = '0' + s % 10;
        buf[p] = 0;
        fb_txt(wx + 8, wy, buf, C_LBL, w->bg); wy += 16;
    }

    /* Initrd section */
    wy += 6;
    fb_txt(wx, wy, "Initrd", 0x5A7AC0, w->bg); wy += 18;

    extern uint8_t *initrd_start;
    extern uint32_t initrd_size;
    if (initrd_start) {
        p = 0;
        const char *ir = "Size: ";
        while (*ir) buf[p++] = *ir++;
        str_int(buf + p, initrd_size);
        while (buf[p]) p++;
        buf[p++] = ' '; buf[p++] = 'b'; buf[p] = 0;
        fb_txt(wx + 8, wy, buf, C_LBL, w->bg); wy += 18;
    } else {
        fb_txt(wx + 8, wy, "Not loaded", C_LBL, w->bg); wy += 18;
    }

    /* Video section */
    fb_txt(wx, wy, "Video", 0x5A7AC0, w->bg); wy += 18;

    extern int fb_w, fb_h, fb_bpp;
    p = 0;
    const char *res = "Resolution: ";
    while (*res) buf[p++] = *res++;
    str_int(buf + p, fb_w); while (buf[p]) p++;
    buf[p++] = 'x'; str_int(buf + p, fb_h); while (buf[p]) p++;
    buf[p++] = ' '; buf[p++] = '@'; buf[p++] = ' ';
    str_int(buf + p, fb_bpp); while (buf[p]) p++;
    buf[p++] = 'b'; buf[p++] = 'p'; buf[p++] = 'p'; buf[p] = 0;
    fb_txt(wx + 8, wy, buf, C_LBL, w->bg); wy += 16;

    wy += 6;
    fb_txt(wx, wy, "About", 0x5A7AC0, w->bg); wy += 18;
    fb_txt(wx + 8, wy, "LunarSnow OS v0.2-alpha x64", C_LBL, w->bg); wy += 16;
    fb_txt(wx + 8, wy, "LunarUI v0.0-3", C_LBL, w->bg);
}

void drawMouse(int wi) {
    Win *w = &wins[wi];
    int wx = w->x + 12, wy = w->y + 28;
    char buf[64]; int p;

    fb_txt(wx, wy, "Mouse Settings", C_TTT, w->bg);
    fb_rect(wx, wy + 20, w->w - 24, 1, 0x3C50A0);
    wy += 34;
}

void mouse_settings() {
    int wi = gui_wnew("Control Panel - Mouse", (fb_w - 420) / 2, 50, 420, 380);
    msgbox("Message", "This section is under construction.");
    wins[wi].draw = drawMouse;
}

// actual window ig (Nixs comment)
void prog_controlpanel(void) {
    int wi = gui_wnew("Control Panel", (fb_w - 420) / 2, 50, 420, 380);
    gui_wbtn(wi, "Mouse settings", 2, 260, 130, 26, mouse_settings);
    wins[wi].draw = drawMain;
}
