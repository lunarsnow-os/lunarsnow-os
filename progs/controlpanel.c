#include "../lunarsnow.h"
#include "../config.h"

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
    /*fb_txt(wx, wy, "Video", 0x5A7AC0, w->bg); wy += 18;

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
    fb_txt(wx + 8, wy, OS_FULL " edition", C_LBL, w->bg); wy += 16;
    fb_txt(wx + 8, wy, UI_FULL, C_LBL, w->bg);*/
}

static void disp_set(int w, int h) { vbe_set_mode(w, h, 32); }
static void ds640(void)  { disp_set(640, 480); }
static void ds800(void)  { disp_set(800, 600); }
static void ds1024(void) { disp_set(1024, 768); }
static void ds720p(void) { disp_set(1280, 720); }
static void ds1280(void) { disp_set(1280, 1024); }
static void ds1366(void) { disp_set(1366, 768); }
static void ds1600(void) { disp_set(1600, 900); }
static void ds1080(void) { disp_set(1920, 1080); }

void drawMouse(int wi) {
    Win *w = &wins[wi];
    int wx = w->x + 12;

    fb_txt(wx, w->y + 28, "Mouse Settings", C_TTT, w->bg);
    fb_rect(wx, w->y + 48, w->w - 24, 1, 0x3C50A0);
}

void drawDisplay(int wi) {
    Win *w = &wins[wi];
    int wx = w->x + 12, wy = w->y + 28;
    char buf[64]; int p;

    fb_txt(wx, wy, "Display Settings", C_TTT, w->bg);
    fb_rect(wx, wy + 20, w->w - 24, 1, 0x3C50A0);
    wy += 34;

    extern int fb_w, fb_h, fb_bpp;
    p = 0;
    const char *res = "Current: ";
    while (*res) buf[p++] = *res++;
    str_int(buf + p, fb_w); while (buf[p]) p++;
    buf[p++] = 'x'; str_int(buf + p, fb_h); while (buf[p]) p++;
    buf[p++] = ' '; buf[p++] = '@'; buf[p++] = ' ';
    str_int(buf + p, fb_bpp); while (buf[p]) p++;
    buf[p++] = 'b'; buf[p++] = 'p'; buf[p++] = 'p'; buf[p] = 0;
    fb_txt(wx + 8, wy, buf, C_LBL, w->bg); wy += 20;

    if (!vbe_available()) {
        fb_txt(wx + 8, wy, "VBE not available. Change via GRUB (reboot).", 0xCC4444, w->bg);
        return;
    }

    fb_txt(wx + 8, wy, "Available resolutions:", C_TTT, w->bg); wy += 22;
}

void mouse_settings() {
    int wi = gui_wnew("Control Panel - Mouse", (fb_w - 420) / 2, 50, 420, 380);
    msgbox("Mouse Settings", "This section is under construction.");
    wins[wi].draw = drawMouse;
}

void display_settings(void) {
    int wi = gui_wnew("Control Panel - Display", (fb_w - 420) / 2, 50, 420, 380);
    gui_wbtn(wi, "640x480",   10,  82, 86, 28, ds640);
    gui_wbtn(wi, "800x600",   104, 82, 86, 28, ds800);
    gui_wbtn(wi, "1024x768",  198, 82, 86, 28, ds1024);
    gui_wbtn(wi, "1280x720",  302, 82, 86, 28, ds720p);
    gui_wbtn(wi, "1280x1024", 10,  118, 86, 28, ds1280);
    gui_wbtn(wi, "1366x768",  104, 118, 86, 28, ds1366);
    gui_wbtn(wi, "1600x900",  198, 118, 86, 28, ds1600);
    gui_wbtn(wi, "1920x1080", 302, 118, 86, 28, ds1080);
    gui_wbtn(wi, "Close", 320, 320, 80, 30, app_close);
    wins[wi].draw = drawDisplay;
}

// actual window ig (Nixs comment)
void prog_controlpanel(void) {
    int wi = gui_wnew("Control Panel", (fb_w - 420) / 2, 50, 420, 380);
    gui_wbtn(wi, "Mouse", 2, 170, 130, 26, mouse_settings);
    gui_wbtn(wi, "Display", 132, 170, 130, 26, display_settings);
    wins[wi].draw = drawMain;
}
