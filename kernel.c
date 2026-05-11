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
   MULTIBOOT2 TAG PARSING
   ================================================================ */

static void *mb2_find_tag(void *info, uint32_t type)
{
    uint8_t *p = (uint8_t*)info + 8;
    uint32_t total = *(uint32_t*)info;
    while ((uint32_t)(p - (uint8_t*)info) < total) {
        uint32_t t = *(uint32_t*)p;
        uint32_t s = *(uint32_t*)(p + 4);
        if (t == type) return p;
        s = (s + 7) & ~7;
        p += s;
    }
    return 0;
}

/* ================================================================
   GLOBALS for About window
   ================================================================ */

uint32_t total_ram;
char cpu_vendor[16];
char cpu_brand[64];
int boot_sec_total;
int cpu_ok;
int cpu_family, cpu_model, cpu_stepping;
char cpu_name[32];

static const char *cpu_model_name(void)
{
    switch (cpu_family) {
    case 3:  return "Intel 386";
    case 4:  return "Intel 486";
    case 5:
        if (cpu_model >= 4) return "Pentium MMX";
        return "Pentium";
    case 6:
        switch (cpu_model) {
        case 1:  return "Pentium Pro";
        case 3:  return "Pentium II";
        case 5:  return "Pentium II / Xeon";
        case 6:  return "Celeron";
        case 7:  return "Pentium III";
        case 8:  return "Pentium III (Coppermine)";
        case 10: return "Pentium III Xeon";
        case 11: return "Pentium III (Tualatin)";
        case 13: return "Celeron M";
        case 14: return "Pentium M";
        default: return "P6 family";
        }
    case 15: return "Pentium 4";
    default: return 0;
    }
}

/* ================================================================
   CPU DETECTION (CPUID)
   ================================================================ */

static void detect_cpu(void)
{
    cpu_ok = 0;
    cpu_vendor[0] = 0;
    cpu_brand[0] = 0;

    uint32_t a, b, c, d;
    asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));

    if (b == 0 && c == 0 && d == 0) return;

    *(uint32_t*)(cpu_vendor + 0) = b;
    *(uint32_t*)(cpu_vendor + 4) = d;
    *(uint32_t*)(cpu_vendor + 8) = c;
    cpu_vendor[12] = 0;

    /* Get Family/Model/Stepping from standard leaf 1 */
    asm volatile("cpuid" : "=a"(a) : "a"(1));
    cpu_stepping = a & 0xF;
    cpu_model = (a >> 4) & 0xF;
    cpu_family = (a >> 8) & 0xF;

    /* Look up CPU model name */
    cpu_name[0] = 0;
    { const char *n = cpu_model_name(); if (n) s_cpy(cpu_name, n, 31); }

    cpu_ok = 1;

    asm volatile("cpuid" : "=a"(a) : "a"(0x80000000));
    if (a < 0x80000004) return;

    uint32_t brand[12];
    asm volatile("cpuid" : "=a"(brand[0]), "=b"(brand[1]), "=c"(brand[2]), "=d"(brand[3]) : "a"(0x80000002));
    asm volatile("cpuid" : "=a"(brand[4]), "=b"(brand[5]), "=c"(brand[6]), "=d"(brand[7]) : "a"(0x80000003));
    asm volatile("cpuid" : "=a"(brand[8]), "=b"(brand[9]), "=c"(brand[10]), "=d"(brand[11]) : "a"(0x80000004));
    mcpy(cpu_brand, brand, 48);
    cpu_brand[48] = 0;
}

/* ================================================================
   MEMORY INFO (via multiboot)
   ================================================================ */

static void parse_memory(uint32_t magic, void *mbinfo)
{
    total_ram = 0;
    if (magic == 0x2BADB002) {
        uint32_t *info = (uint32_t*)mbinfo;
        if (info[0] & 1)
            total_ram = (info[1] + info[2]) * 1024;
    } else if (magic == 0x36D76289) {
        uint8_t *tag = mb2_find_tag(mbinfo, 4);
        if (tag) {
            uint32_t lower = *(uint32_t*)(tag + 8);
            uint32_t upper = *(uint32_t*)(tag + 12);
            total_ram = (lower + upper) * 1024;
        }
    }
}

/* ================================================================
   FRAMEBUFFER INIT
   ================================================================ */

static int fb_init(uint32_t magic, void *mbinfo)
{
    /* Try Bochs VBE first (works on QEMU with -kernel) */
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

    if (fb_addr) {
        vbe_set(800, 600, 32);
        fb_init_ptr((uint32_t*)fb_addr, 800, 600, 800 * 4, 32);
        return 0;
    }

    /* Fall back to multiboot2 framebuffer (real hardware via GRUB) */
    if (magic == 0x36D76289) {
        uint8_t *tag = mb2_find_tag(mbinfo, 8);
        if (tag) {
            uint64_t a = *(uint64_t*)(tag + 8);
            uint32_t pitch = *(uint32_t*)(tag + 16);
            uint32_t w = *(uint32_t*)(tag + 20);
            uint32_t h = *(uint32_t*)(tag + 24);
            uint8_t bpp = *(tag + 28);
            if ((bpp == 16 || bpp == 24 || bpp == 32) && w <= 800 && h <= 600) {
                fb_init_ptr((uint32_t*)(uint32_t)a, (int)w, (int)h, (int)pitch, bpp);
                return 0;
            }
        }
    }

    return -1;
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
   ACPI S5 + PIIX4 poweroff
   ================================================================ */

static void piix4_poweroff(void)
{
    for (int dev = 0; dev < 32; dev++) {
        uint32_t vid_did = pci_read(PCI_ADDR(0, dev, 0, 0));
        uint16_t vid = vid_did & 0xFFFF;
        if (vid != 0x8086) continue;
        uint16_t did = (vid_did >> 16) & 0xFFFF;
        int known = (did == 0x7113 || did == 0x7000 || did == 0x7110 ||
                     did == 0x122E || did == 0x2410 || did == 0x2420 ||
                     did == 0x2440 || did == 0x2480 || did == 0x24C0);
        if (!known) continue;
        for (int func = 0; func < 4; func++) {
            uint32_t vd = pci_read(PCI_ADDR(0, dev, func, 0));
            if ((vd & 0xFFFF) != vid) continue;
            uint32_t pmbase_raw = pci_read(PCI_ADDR(0, dev, func, 0x40));
            uint16_t pmbase = pmbase_raw & 0xFFF8;
            if (pmbase < 0x400 || pmbase > 0x1000) continue;
            uint16_t pm1a_cnt = pmbase + 4;
            uint16_t cur = inw(pm1a_cnt);
            outw(pm1a_cnt, (cur & 1) | 0x3400);
            for (volatile int d = 0; d < 30000; d++);
            cur = inw(pm1a_cnt);
            outw(pm1a_cnt, (cur & 1) | 0x3C00);
            for (volatile int d = 0; d < 30000; d++);
        }
    }
}

/* ACPI S5 via RSDP table walk (with checksum validation) */
static void acpi_poweroff(void)
{
    uint32_t sig0 = 0x20445352;
    uint32_t sig1 = 0x20525450;
    uint32_t addr = 0;

    uint16_t ebda_seg;
    asm volatile("movw 0x40E, %0" : "=r"(ebda_seg));
    uint32_t ebda = (uint32_t)ebda_seg << 4;
    if (ebda >= 0x80000 && ebda <= 0x9F000) {
        for (uint32_t p = ebda; p < ebda + 0x400; p += 16) {
            volatile uint32_t *pp = (volatile uint32_t*)(uintptr_t)p;
            if (pp[0] == sig0 && pp[1] == sig1) {
                /* Validate v1 checksum: bytes 0-19 sum to 0 */
                uint8_t sum = 0;
                for (int i = 0; i < 20; i++) sum += ((volatile uint8_t*)(uintptr_t)p)[i];
                if (sum == 0) { addr = p; goto rsdp_found; }
            }
        }
    }

    for (uint32_t p = 0xE0000; p < 0xFFFFF; p += 16) {
        volatile uint32_t *pp = (volatile uint32_t*)(uintptr_t)p;
        if (pp[0] == sig0 && pp[1] == sig1) {
            uint8_t sum = 0;
            for (int i = 0; i < 20; i++) sum += ((volatile uint8_t*)(uintptr_t)p)[i];
            if (sum == 0) { addr = p; goto rsdp_found; }
        }
    }
    return;

rsdp_found:;
    uint32_t rsdt_addr = *(volatile uint32_t*)(uintptr_t)(addr + 16);
    if (rsdt_addr < 0xE0000 || rsdt_addr > 0xFFFFF) return;

    volatile uint32_t *rsdt = (volatile uint32_t*)(uintptr_t)rsdt_addr;
    uint32_t rsdt_len = rsdt[1];
    if (rsdt_len < 36 || rsdt_len > 0x10000) return;
    uint32_t entries = (rsdt_len - 36) / 4;

    uint32_t fadt_addr = 0;
    for (uint32_t i = 0; i < entries; i++) {
        uint32_t ptr = rsdt[9 + i];
        if (ptr < 0xE0000 || ptr > 0xFFFFF) continue;
        volatile uint32_t *hdr = (volatile uint32_t*)(uintptr_t)ptr;
        if (hdr[0] == 0x50434146) { fadt_addr = ptr; break; }
    }
    if (fadt_addr == 0) return;

    uint32_t fadt_len = *(volatile uint32_t*)(uintptr_t)(fadt_addr + 4);
    if (fadt_len < 72) return;

    uint32_t pm1a_cnt = *(volatile uint32_t*)(uintptr_t)(fadt_addr + 64);
    if (pm1a_cnt < 0x400 || pm1a_cnt > 0x1000) return;

    uint32_t pm1b_cnt = *(volatile uint32_t*)(uintptr_t)(fadt_addr + 68);

    uint16_t pm1a_cur = inw((uint16_t)pm1a_cnt);
    if (!(pm1a_cur & 1)) {
        uint32_t smi_cmd = *(volatile uint32_t*)(uintptr_t)(fadt_addr + 48);
        uint8_t acpi_enable = *(volatile uint8_t*)(uintptr_t)(fadt_addr + 52);
        if (smi_cmd && smi_cmd <= 0xFFFF && acpi_enable) {
            outb((uint16_t)smi_cmd, acpi_enable);
            for (int t = 0; t < 30000; t++) {
                if (inw((uint16_t)pm1a_cnt) & 1) break;
                asm("pause");
            }
            pm1a_cur = inw((uint16_t)pm1a_cnt);
        }
    }

    uint16_t sci_en = pm1a_cur & 1;

    uint16_t val = sci_en | 0x3400;
    outw((uint16_t)pm1a_cnt, val);
    if (pm1b_cnt && pm1b_cnt <= 0xFFFF) outw((uint16_t)pm1b_cnt, val);
    for (volatile int d = 0; d < 30000; d++);

    val = sci_en | 0x3C00;
    outw((uint16_t)pm1a_cnt, val);
    if (pm1b_cnt && pm1b_cnt <= 0xFFFF) outw((uint16_t)pm1b_cnt, val);
    for (volatile int d = 0; d < 30000; d++);
}

/* ================================================================
   ENTRY
   ================================================================ */

void kmain(uint32_t magic, void *mbinfo)
{
    if (fb_init(magic, mbinfo) < 0) return;

    boot_screen();
    mouse_init();

    detect_cpu();
    parse_memory(magic, mbinfo);
    {
        int h, m, s;
        rtc_read(&h, &m, &s);
        boot_sec_total = h * 3600 + m * 60 + s;
    }

    /* Build dynamic menu */
    gui_menu_add("New Window", cb_new);
    gui_menu_add("Terminal",   cb_term);
    gui_menu_add("Calculator", cb_calc);
    for (int i = 0; i < progs_n; i++) gui_menu_add(progs[i].name, progs[i].init);
    gui_menu_add("Reboot",     cb_reboot);
    gui_menu_add("Shutdown",   cb_shutdown);

    run = 1;

    int clock_ticks = 0;
    while (run > 0) {
        int prev_btn = mouse_btn;
        int prev_mx = mouse_x, prev_my = mouse_y;
        kb_poll();

        /* Mouse button transition */
        if ((mouse_btn & 1) && !(prev_btn & 1)) {
            gui_mouse_click();
            need_render = 1;
        }
        if (mouse_drag && !(mouse_btn & 1))
            mouse_drag = 0;
        if (mouse_drag) {
            wins[mouse_drag_win].x = mouse_x - mouse_drag_ox;
            wins[mouse_drag_win].y = mouse_y - mouse_drag_oy;
            need_render = 1;
        }

        int key = kb_pop();
        if (key >= 0) {
            need_render = 1;

            /* App key routing — per-window */
            if (act >= 0 && act < nw && wins[act].on_key && key != '\t' && key != 27) {
                wins[act].on_key(key);
                goto render_choice;
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
        }

    render_choice:
        /* Force full render every ~1000 polls for clock update */
        clock_ticks++;
        if (clock_ticks >= 1000) {
            need_render = 1;
            clock_ticks = 0;
        }

        int mouse_moved = (mouse_x != prev_mx || mouse_y != prev_my);

        if (need_render) {
            gui_render();
            need_render = 0;
        } else if (mouse_moved) {
            gui_update_cursor();
        }
    }

    if (run == -1) {
        fb_clear(0x000000);
        int dy = 10;
        #define DBG(s) do { fb_txt(10, dy, s, 0xFFFFFF, 0x000000); fb_flip(); dy += 18; } while(0)
        DBG("Reboot: 0x92...");
        outb(0x92, 0x02); outb(0x92, 0x03);
        DBG("Reboot: lidt+ud2...");
        __asm__ __volatile__(
            "pushl $0\n\t"
            "pushl $0\n\t"
            "lidt (%%esp)\n\t"
            "ud2\n\t"
            : : : "memory"
        );
        DBG("Reboot: ALL FAILED, halting");
        for (;;) asm("hlt");
        #undef DBG
    }
    fb_clear(0x000000);
    int dy = 10;
    #define DBG(s) do { fb_txt(10, dy, s, 0xFFFFFF, 0x000000); fb_flip(); dy += 18; } while(0)
    DBG("Shutdown: PIIX4...");
    piix4_poweroff();
    DBG("Shutdown: ACPI...");
    acpi_poweroff();
    DBG("Shutdown: APM...");
    outb(0xB3, 0x00); outb(0xB2, 0x01);
    for (volatile int d = 0; d < 30000; d++);
    outb(0xB3, 0x00); outb(0xB2, 0x02);
    for (volatile int d = 0; d < 30000; d++);
    outb(0xB3, 0x00); outb(0xB2, 0x03);
    for (volatile int d = 0; d < 30000; d++);
    outb(0xB3, 0x01); outb(0xB2, 0x53);
    for (volatile int d = 0; d < 30000; d++);
    DBG("Shutdown: QEMU ports...");
    outw(0x604, 0x2000); outw(0xB004, 0x2000);
    outw(0x600, 0x34); outb(0xB2, 0x00);
    DBG("Shutdown: Safe to power off");
    for (;;) asm("hlt");
    #undef DBG
}
