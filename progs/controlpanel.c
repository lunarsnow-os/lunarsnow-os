#include "lunarsnow.h"
#include "config.h"
#include "smbus.h"
#include "fs.h"

static void draw_main(int wi) {
    Win *w = &wins[wi];
    int wx = w->x + 12, wy = w->y + 28;

    fb_rect(wx + 4, wy, 6, 16, 0x3C50A0);
    fb_txt(wx + 16, wy + 1, "Control Panel", C_TTT, w->bg);
    fb_rect(wx, wy + 22, w->w - 24, 1, 0x3C50A0);
    wy += 38;

    fb_txt(wx + 4, wy, "Settings", 0x5A7AC0, w->bg);
    fb_rect(wx + 4, wy + 16, w->w - 32, 1, 0x282845);
}

/* ================================================================
   DISPLAY SETTINGS
   ================================================================ */

static int  res_pending;
static int  res_old_w, res_old_h;
static int  res_confirm_wi;
static int  res_remain;
static int  res_start_sec;
static void (*res_old_tick)(void);

static void res_keep(void)
{
    res_pending = 0;
    gui_tick = res_old_tick;
    if (res_confirm_wi >= 0) {
        int wi = res_confirm_wi;
        res_confirm_wi = -1;
        gui_wclose(wi);
    }
}

static void res_revert(void)
{
    vbe_set_mode(res_old_w, res_old_h, 32);
    res_pending = 0;
    gui_tick = res_old_tick;
    if (res_confirm_wi >= 0) {
        int wi = res_confirm_wi;
        res_confirm_wi = -1;
        gui_wclose(wi);
    }
}

static void res_on_close(int wi)
{
    (void)wi;
    if (res_pending) {
        vbe_set_mode(res_old_w, res_old_h, 32);
        res_pending = 0;
        gui_tick = res_old_tick;
    }
    res_confirm_wi = -1;
}

static void res_draw(int wi)
{
    Win *w = &wins[wi];
    char buf[64];
    int p = 0;
    const char *m = "Keep this resolution? (";
    while (*m) buf[p++] = *m++;
    buf[p++] = '0' + res_remain / 10;
    buf[p++] = '0' + res_remain % 10;
    buf[p++] = ')'; buf[p] = 0;
    int l = s_len(buf);
    fb_txt(w->x + (w->w - l * 8) / 2, w->y + 32, buf, C_LBL, w->bg);
}

static void res_tick(void)
{
    if (!res_pending) return;
    int h, m, s;
    rtc_read(&h, &m, &s);
    int elapsed = s - res_start_sec;
    if (elapsed < 0) elapsed += 60;
    res_remain = 5 - elapsed;
    if (res_remain < 0) res_remain = 0;
    if (elapsed >= 5) { res_revert(); return; }
    need_render = 1;
}

static void start_res_change(int w, int h)
{
    if (res_pending) return;
    res_old_w = fb_w; res_old_h = fb_h;
    vbe_set_mode(w, h, 32);
    {
        int hh, mm, ss;
        rtc_read(&hh, &mm, &ss);
        res_start_sec = ss;
    }
    res_pending = 1;
    res_remain = 5;
    res_old_tick = gui_tick;
    gui_tick = res_tick;
    res_confirm_wi = gui_wnew("Confirm", (fb_w - 300) / 2, (fb_h - 110) / 2, 300, 110);
    gui_wbtn(res_confirm_wi, "Yes", 70, 60, 60, 26, res_keep);
    gui_wbtn(res_confirm_wi, "No",  170, 60, 60, 26, res_revert);
    wins[res_confirm_wi].draw = res_draw;
    wins[res_confirm_wi].on_close = res_on_close;
}

static void ds640(void)  { start_res_change(640, 480); }
static void ds800(void)  { start_res_change(800, 600); }
static void ds1024(void) { start_res_change(1024, 768); }
static void ds720p(void) { start_res_change(1280, 720); }
static void ds1280(void) { start_res_change(1280, 1024); }
static void ds1366(void) { start_res_change(1366, 768); }
static void ds1600(void) { start_res_change(1600, 900); }
static void ds1080(void) { start_res_change(1920, 1080); }

static void draw_display(int wi) {
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
    wins[wi].draw = draw_display;
}

/* ================================================================
   MOUSE SETTINGS
   ================================================================ */

static int mouse_wi;

static void mouse_update_label(void)
{
    if (mouse_wi < 0) return;
    need_render = 1;
}

static void mouse_spd_1(void) { mouse_speed = 1; mouse_update_label(); }
static void mouse_spd_2(void) { mouse_speed = 2; mouse_update_label(); }
static void mouse_spd_3(void) { mouse_speed = 3; mouse_update_label(); }
static void mouse_spd_4(void) { mouse_speed = 4; mouse_update_label(); }
static void mouse_spd_5(void) { mouse_speed = 5; mouse_update_label(); }

static void draw_mouse(int wi) {
    Win *w = &wins[wi];
    int wx = w->x + 12, wy = w->y + 28;

    fb_txt(wx, wy, "Mouse Settings", C_TTT, w->bg);
    fb_rect(wx, wy + 20, w->w - 24, 1, 0x3C50A0);
    wy += 34;

    fb_txt(wx + 8, wy, "Pointer speed:", C_LBL, w->bg); wy += 22;

    const char *labels[] = { "1 - Slow", "2 - Normal", "3 - Fast", "4 - Very Fast", "5 - Turbo" };
    for (int i = 0; i < 5; i++) {
        int sel = (mouse_speed == i + 1);
        fb_txt(wx + 12, wy, sel ? "[x]" : "[ ]", sel ? C_TAC : C_LBL, w->bg);
        fb_txt(wx + 40, wy, labels[i], C_LBL, w->bg);
        wy += 20;
    }
}

void mouse_settings(void) {
    mouse_wi = gui_wnew("Control Panel - Mouse", (fb_w - 360) / 2, 60, 360, 280);
    gui_wbtn(mouse_wi, "1", 140, 58, 36, 20, mouse_spd_1);
    gui_wbtn(mouse_wi, "2", 140, 78, 36, 20, mouse_spd_2);
    gui_wbtn(mouse_wi, "3", 140, 98, 36, 20, mouse_spd_3);
    gui_wbtn(mouse_wi, "4", 140, 118, 36, 20, mouse_spd_4);
    gui_wbtn(mouse_wi, "5", 140, 138, 36, 20, mouse_spd_5);
    gui_wbtn(mouse_wi, "Close", 260, 210, 80, 30, app_close);
    wins[mouse_wi].draw = draw_mouse;
}

/* ================================================================
   POWER / BATTERY
   ================================================================ */

static void draw_power(int wi) {
    Win *w = &wins[wi];
    int wx = w->x + 12, wy = w->y + 28;
    fb_txt(wx, wy, "Power Status", C_TTT, w->bg);
    fb_rect(wx, wy + 20, w->w - 24, 1, 0x3C50A0);
    wy += 34;

    if (!battery_present) {
        fb_txt(wx + 8, wy, "No battery detected.", C_LBL, w->bg);
        return;
    }

    char buf[64]; int p = 0;
    const char *s1 = "Battery: ";
    while (*s1) buf[p++] = *s1++;
    str_int(buf + p, battery_percent);
    while (buf[p]) p++;
    buf[p++] = '%'; buf[p] = 0;
    fb_txt(wx + 8, wy, buf, battery_percent < 10 ? 0xFF3030 : battery_percent < 20 ? 0xFF9030 : C_LBL, w->bg);
    wy += 20;

    p = 0;
    const char *s2 = battery_charging ? "Status: Charging" : "Status: Discharging";
    while (*s2) buf[p++] = *s2++;
    buf[p] = 0;
    fb_txt(wx + 8, wy, buf, battery_charging ? 0x30E030 : 0xFF9030, w->bg);
}

static void power_settings(void) {
    int wi = gui_wnew("Control Panel - Power", (fb_w - 360) / 2, 50, 360, 180);
    gui_wbtn(wi, "Close", 260, 120, 80, 30, app_close);
    wins[wi].draw = draw_power;
}

/* ================================================================
   SYSTEM INFO
   ================================================================ */

static void draw_sysinfo(int wi) {
    Win *w = &wins[wi];
    int wx = w->x + 12, wy = w->y + 28;
    char buf[96]; int p;

    fb_txt(wx, wy, "System Information", C_TTT, w->bg);
    fb_rect(wx, wy + 20, w->w - 24, 1, 0x3C50A0);
    wy += 30;

    /* OS */
    p = 0;
    const char *os = OS_NAME " ";
    while (*os) buf[p++] = *os++;
    const char *ver = OS_VER " " OS_ARCH;
    while (*ver) buf[p++] = *ver++;
    buf[p] = 0;
    fb_txt(wx + 8, wy, buf, C_TTT, w->bg); wy += 18;

    /* CPU vendor */
    p = 0;
    const char *c0 = "CPU: ";
    while (*c0) buf[p++] = *c0++;
    { const char *src = cpu_vendor; while (*src) buf[p++] = *src++; }
    buf[p++] = ' ';
    { const char *src = cpu_brand; while (*src && p < 94) buf[p++] = *src++; }
    buf[p] = 0;
    fb_txt(wx + 8, wy, buf, C_LBL, w->bg); wy += 18;

    /* RAM */
    uint64_t mb = total_ram / (1024 * 1024);
    p = 0;
    const char *r0 = "RAM: ";
    while (*r0) buf[p++] = *r0++;
    str_int(buf + p, (int)mb);
    while (buf[p]) p++;
    buf[p++] = ' '; buf[p++] = 'M'; buf[p++] = 'B'; buf[p] = 0;
    fb_txt(wx + 8, wy, buf, C_LBL, w->bg); wy += 18;

    /* Display */
    extern int fb_w, fb_h, fb_bpp;
    p = 0;
    const char *d0 = "Display: ";
    while (*d0) buf[p++] = *d0++;
    str_int(buf + p, fb_w); while (buf[p]) p++;
    buf[p++] = 'x'; str_int(buf + p, fb_h); while (buf[p]) p++;
    buf[p++] = ' '; buf[p++] = '@'; buf[p++] = ' ';
    str_int(buf + p, fb_bpp); while (buf[p]) p++;
    buf[p++] = 'b'; buf[p++] = 'p'; buf[p++] = 'p'; buf[p] = 0;
    fb_txt(wx + 8, wy, buf, C_LBL, w->bg); wy += 18;

    /* Uptime */
    int h, m, s; rtc_read(&h, &m, &s);
    int sec = h * 3600 + m * 60 + s - boot_sec_total;
    if (sec < 0) sec += 86400;
    p = 0;
    const char *u0 = "Uptime: ";
    while (*u0) buf[p++] = *u0++;
    str_int(buf + p, sec / 3600); while (buf[p]) p++;
    buf[p++] = 'h'; buf[p++] = ' ';
    str_int(buf + p, (sec % 3600) / 60); while (buf[p]) p++;
    buf[p++] = 'm'; buf[p++] = ' ';
    str_int(buf + p, sec % 60); while (buf[p]) p++;
    buf[p++] = 's'; buf[p] = 0;
    fb_txt(wx + 8, wy, buf, C_LBL, w->bg); wy += 18;

    /* Disk */
    p = 0;
    const char *k0 = "Disk: ";
    while (*k0) buf[p++] = *k0++;
    str_int(buf + p, disk_count()); while (buf[p]) p++;
    buf[p++] = ' '; buf[p++] = 'd'; buf[p++] = 'r'; buf[p++] = 'i'; buf[p++] = 'v'; buf[p++] = 'e';
    if (disk_count() != 1) buf[p++] = 's';
    buf[p] = 0;
    fb_txt(wx + 8, wy, buf, C_LBL, w->bg);
}

static void sysinfo_settings(void) {
    int wi = gui_wnew("Control Panel - System", (fb_w - 440) / 2, 40, 440, 300);
    gui_wbtn(wi, "Close", 340, 240, 80, 30, app_close);
    wins[wi].draw = draw_sysinfo;
}

/* ================================================================
   ENTRY
   ================================================================ */

void prog_controlpanel(void) {
    int wi = gui_wnew("Control Panel", (fb_w - 420) / 2, 50, 420, 340);
    gui_wbtn(wi, "System", 14, 72, 130, 26, sysinfo_settings);
    gui_wbtn(wi, "Mouse", 14, 104, 130, 26, mouse_settings);
    gui_wbtn(wi, "Display", 148, 72, 130, 26, display_settings);
    gui_wbtn(wi, "Power", 148, 104, 130, 26, power_settings);
    wins[wi].draw = draw_main;
}
