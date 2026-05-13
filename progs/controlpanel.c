#include "../lunarsnow.h"
#include "../config.h"

static void drawMain(int wi) {
    Win *w = &wins[wi];
    int wx = w->x + 12, wy = w->y + 28;

    fb_rect(wx + 4, wy, 6, 16, 0x3C50A0);
    fb_txt(wx + 16, wy + 1, "Control Panel", C_TTT, w->bg);
    fb_rect(wx, wy + 22, w->w - 24, 1, 0x3C50A0);
    wy += 38;

    fb_txt(wx + 4, wy, "Settings", 0x5A7AC0, w->bg);
    fb_rect(wx + 4, wy + 16, w->w - 32, 1, 0x282845);
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

static void drawDisplay(int wi) {
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

    fb_txt(wx + 8, wy, "Available resolutions:", C_TTT, w->bg);
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

static void mouse_settings(void) {
    msgbox("Mouse", "Not implemented yet.");
}

void prog_controlpanel(void) {
    int wi = gui_wnew("Control Panel", (fb_w - 420) / 2, 50, 420, 260);
    gui_wbtn(wi, "Mouse", 14, 72, 130, 26, mouse_settings);
    gui_wbtn(wi, "Display", 148, 72, 130, 26, display_settings);
    wins[wi].draw = drawMain;
}
