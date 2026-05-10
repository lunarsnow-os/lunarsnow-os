#include <stdint.h>
#include "lunarsnow.h"
#include "progs.h"
#include "font8x16.h"

/* ================================================================
   PORT I/O
   ================================================================ */

static inline void outb(uint16_t p, uint8_t v)
{ asm volatile("outb %0, %1" : : "a"(v), "Nd"(p)); }

static inline uint8_t inb(uint16_t p)
{ uint8_t v; asm volatile("inb %1, %0" : "=a"(v) : "Nd"(p)); return v; }

static inline void outw(uint16_t p, uint16_t v)
{ asm volatile("outw %0, %1" : : "a"(v), "Nd"(p)); }

static inline uint16_t inw(uint16_t p)
{ uint16_t v; asm volatile("inw %1, %0" : "=a"(v) : "Nd"(p)); return v; }

static inline void outl(uint16_t p, uint32_t v)
{ asm volatile("outl %0, %1" : : "a"(v), "Nd"(p)); }

static inline uint32_t inl(uint16_t p)
{ uint32_t v; asm volatile("inl %1, %0" : "=a"(v) : "Nd"(p)); return v; }

/* ================================================================
   CMOS / RTC
   ================================================================ */

#define CMOS_IDX 0x70
#define CMOS_DAT 0x71

static uint8_t cmos_r(uint8_t reg)
{
    outb(CMOS_IDX, reg);
    return inb(CMOS_DAT);
}

static int bcd2bin(uint8_t v)
{
    return ((v >> 4) * 10) + (v & 0xF);
}

void rtc_read(int *h, int *m, int *s)
{
    uint8_t sec, min, hour;
    while (cmos_r(0x0A) & 0x80);
    sec  = cmos_r(0x00);
    min  = cmos_r(0x02);
    hour = cmos_r(0x04);
    if (cmos_r(0x0A) & 0x80) { rtc_read(h, m, s); return; }
    *h = bcd2bin(hour);
    *m = bcd2bin(min);
    *s = bcd2bin(sec);
}

/* ================================================================
   PCI
   ================================================================ */

#define PCI_ADDR(bus, dev, func, reg) \
    (0x80000000 | (bus << 16) | (dev << 11) | (func << 8) | (reg & 0xFC))

static uint32_t pci_read(uint32_t addr)
{
    outl(0xCF8, addr);
    return inl(0xCFC);
}

/* ================================================================
   BOCHS VBE
   ================================================================ */

#define VBE_IDX 0x1CE
#define VBE_DAT 0x1CF

static void vbe_w(uint16_t idx, uint16_t v)
{
    outw(VBE_IDX, idx);
    outw(VBE_DAT, v);
}

static void vbe_set(int w, int h, int bpp)
{
    vbe_w(4, 0x00);
    vbe_w(1, w);
    vbe_w(2, h);
    vbe_w(3, bpp);
    vbe_w(4, 0x41);
}

/* ================================================================
   FRAMEBUFFER INIT
   ================================================================ */

static int fb_init(void)
{
    uint32_t fb_addr = 0;
    for (int dev = 0; dev < 32; dev++) {
        uint32_t id = pci_read(PCI_ADDR(0, dev, 0, 0));
        uint32_t vid = id & 0xFFFF;
        uint32_t did = (id >> 16) & 0xFFFF;
        if (vid == 0x1234 && did == 0x1111) {
            fb_addr = pci_read(PCI_ADDR(0, dev, 0, 0x10));
            if (fb_addr & 1)
                fb_addr = pci_read(PCI_ADDR(0, dev, 0, 0x18));
            fb_addr &= ~0xF;
            break;
        }
    }

    if (!fb_addr)
        fb_addr = 0xE0000000;

    vbe_set(800, 600, 32);
    fb_init_ptr((uint32_t*)fb_addr, 800, 600, 800 * 4);
    return 0;
}

/* ================================================================
   BOOT SCREEN
   ================================================================ */

static void boot_screen(void)
{
    uint32_t bg = 0x0A0A1A;
    fb_clear(bg);

    char *title = "LunarSnow OS";
    int tx = (fb_w - s_len(title) * 8) / 2;
    fb_txt(tx, fb_h / 2 - 50, title, 0xE6E6F0, bg);

    char *sub = "Booting...";
    int sx = (fb_w - s_len(sub) * 8) / 2;
    fb_txt(sx, fb_h / 2 - 20, sub, 0x8888AA, bg);

    int pbx = fb_w / 2 - 100, pby = fb_h / 2 + 10;
    fb_rect(pbx, pby, 200, 14, 0x1A1A3A);
    fb_border(pbx, pby, 200, 14, 0x3C50A0);
    fb_flip();

    for (int p = 0; p <= 100; p += 2) {
        fb_rect(pbx + 2, pby + 2, (196 * p) / 100, 10, 0x3C50A0);
        fb_flip();
        for (volatile int d = 0; d < 80000; d++);
    }

    for (volatile int d = 0; d < 500000; d++);
}

/* ================================================================
   ENTRY
   ================================================================ */

void kmain(uint32_t magic, void *mbinfo)
{
    (void)magic; (void)mbinfo;
    if (fb_init() < 0) return;

    boot_screen();
    mouse_init();

    /* Build dynamic menu */
    gui_menu_add("New Window", cb_new);
    gui_menu_add("Terminal",   cb_term);
    gui_menu_add("About",      cb_about);
    gui_menu_add("Calculator", cb_calc);
    for (int i = 0; i < progs_n; i++) gui_menu_add(progs[i].name, progs[i].init);
    gui_menu_add("Shutdown",   cb_shutdown);

    run = 1;

    while (run) {
        int prev_btn = mouse_btn;
        kb_poll();

        /* Mouse button transition */
        if ((mouse_btn & 1) && !(prev_btn & 1))
            gui_mouse_click();
        if (mouse_drag && !(mouse_btn & 1))
            mouse_drag = 0;
        if (mouse_drag)
            wins[mouse_drag_win].x = mouse_x - mouse_drag_ox,
            wins[mouse_drag_win].y = mouse_y - mouse_drag_oy;

        int key = kb_pop();
        if (key < 0) goto render_frame;

        /* App key routing */
        if (act == app_win && app_on_key && key != '\t' && key != 27) {
            app_on_key(key);
            goto render_frame;
        }

        /* Keyboard navigation */
        if (key == '\t') {
            if (menu_open) {
                menu_focus = (menu_focus + 1) % gui_menu_count();
            } else if (focus_mode == 0) {
                if (nw > 0 && wins[act].nb > 0)
                    wins[act].fc = (wins[act].fc + 1) % wins[act].nb;
                else
                    focus_mode = 1;
            } else {
                focus_mode = 0;
                if (nw > 0) act = (act + 1) % nw;
            }
        }
        if (key == '\n' || key == ' ') {
            if (menu_open) {
                gui_menu_exec(menu_focus);
                menu_open = 0; focus_mode = 0;
            } else if (focus_mode == 1) {
                menu_open = 1; menu_focus = 0; focus_mode = 2;
            } else if (focus_mode == 0 && nw > 0 && wins[act].nb > 0) {
                Btn *b = &wins[act].btns[wins[act].fc];
                if (b->cb) b->cb();
            }
        }
        if (key == 27) {
            if (menu_open) { menu_open = 0; focus_mode = 1; }
            else if (focus_mode != 0) { focus_mode = 0; }
            else if (nw > 1) gui_wclose(act);
        }
        if (key == KEY_UP && menu_open)
            menu_focus = (menu_focus - 1 + gui_menu_count()) % gui_menu_count();
        if (key == KEY_DOWN && menu_open)
            menu_focus = (menu_focus + 1) % gui_menu_count();
        if (key == KEY_SUPER) {
            if (menu_open) { menu_open = 0; focus_mode = 0; }
            else { menu_open = 1; menu_focus = 0; focus_mode = 2; }
        }

    render_frame:
        gui_render();
    }

    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    for (;;) asm("hlt");
}
