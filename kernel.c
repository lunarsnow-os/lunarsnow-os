#include <stdint.h>
#include "lunarsnow.h"
#include "progs.h"
#include "config.h"
#include "fat.h"

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
    int timeout = 0;
    while (cmos_r(0x0A) & 0x80) {
        if (++timeout > 10000) { *h = 0; *m = 0; *s = 0; return; }
    }
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
   BOCHS VBE (BGA)
   ================================================================ */

#define VBE_IDX 0x01CE
#define VBE_DAT 0x01CF

#define VBE_DISPI_INDEX_ID      0
#define VBE_DISPI_INDEX_XRES    1
#define VBE_DISPI_INDEX_YRES    2
#define VBE_DISPI_INDEX_BPP     3
#define VBE_DISPI_INDEX_ENABLE  4

int vbe_available(void)
{
    outw(VBE_IDX, VBE_DISPI_INDEX_ID);
    outw(VBE_DAT, 0xB0C0);
    outw(VBE_IDX, VBE_DISPI_INDEX_ID);
    return inw(VBE_DAT) >= 0xB0C0;
}

int vbe_set_mode(int w, int h, int bpp)
{
    if (!vbe_available()) return -1;
    if (w > 1920 || h > 1080) return -1;
    if (bpp != 16 && bpp != 24 && bpp != 32) return -1;

    outw(VBE_IDX, VBE_DISPI_INDEX_ENABLE);
    outw(VBE_DAT, 0);

    outw(VBE_IDX, VBE_DISPI_INDEX_XRES);
    outw(VBE_DAT, w);
    outw(VBE_IDX, VBE_DISPI_INDEX_YRES);
    outw(VBE_DAT, h);
    outw(VBE_IDX, VBE_DISPI_INDEX_BPP);
    outw(VBE_DAT, bpp);

    outw(VBE_IDX, VBE_DISPI_INDEX_ENABLE);
    outw(VBE_DAT, 0x41);

    uint32_t *addr = fb_get_addr();
    if (!addr) return -1;

    int pitch = w * (bpp / 8);
    fb_init_ptr(addr, w, h, pitch, bpp);
    fb_clear(0);
    gui_reset_cursor();
    if (mouse_x >= fb_w) mouse_x = fb_w - 1;
    if (mouse_y >= fb_h) mouse_y = fb_h - 1;
    need_render = 1;
    return 0;
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

uint64_t total_ram;
char cpu_vendor[16];
char cpu_brand[64];
int boot_sec_total;
int cpu_ok;
static uint32_t mb_magic;
static void *mb_info;

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

    /* Verify CPUID leaf 1 exists */
    asm volatile("cpuid" : "=a"(a) : "a"(1));

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
    if (magic == 0x36D76289) {
        /* Try memory map tag (type 6) first — supports >4GB via E820 */
        uint8_t *tag = mb2_find_tag(mbinfo, 6);
        if (tag) {
            uint32_t entry_size = *(uint32_t*)(tag + 8);
            uint32_t total_size = *(uint32_t*)(tag + 4);
            uint8_t *p = tag + 16;
            while ((uint32_t)(p - tag) < total_size) {
                uint64_t len   = *(uint64_t*)(p + 8);
                uint32_t type  = *(uint32_t*)(p + 16);
                if (type == 1) total_ram += len;
                p += entry_size;
            }
            return;
        }
        /* Fallback to basic memory info (type 4) */
        tag = mb2_find_tag(mbinfo, 4);
        if (tag) {
            uint32_t lower = *(uint32_t*)(tag + 8);
            uint32_t upper = *(uint32_t*)(tag + 12);
            total_ram = (uint64_t)(lower + upper) * 1024;
        }
    }
}

/* ================================================================
   INITRD (tar archive loaded via multiboot2 module)
   ================================================================ */

uint8_t *initrd_start;
uint32_t initrd_size;

typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
} tar_hdr_t;

static int oct2int(const char *s, int n)
{
    int r = 0;
    for (int i = 0; i < n && s[i]; i++) {
        if (s[i] < '0' || s[i] > '7') break;
        r = (r << 3) | (s[i] - '0');
    }
    return r;
}

static void parse_initrd(uint32_t magic, void *mbinfo)
{
    if (magic != 0x36D76289) return;
    uint8_t *tag = mb2_find_tag(mbinfo, 3);
    if (!tag) return;
    uint32_t start = *(uint32_t*)(tag + 8);
    uint32_t end   = *(uint32_t*)(tag + 12);
    initrd_start   = (uint8_t*)(uintptr_t)start;
    initrd_size    = end - start;
}

/* Find file by name in initrd tar archive.
   Returns pointer to file data, writes size to *size_out.
   Returns 0 if not found. */
uint8_t *file_read(const char *name, uint32_t *size_out)
{
    if (!initrd_start) return 0;
    uint8_t *p = initrd_start;
    uint8_t *end = initrd_start + initrd_size;

    while (p + 512 <= end) {
        tar_hdr_t *h = (tar_hdr_t *)p;
        if (h->name[0] == 0) break;
        /* magic is "ustar" (may be padded with spaces instead of null) */
        if (h->magic[0] != 'u' || h->magic[1] != 's' || h->magic[2] != 't' ||
            h->magic[3] != 'a' || h->magic[4] != 'r') break;

        int fsize = oct2int(h->size, 12);
        int padded = (fsize + 511) & ~511;

        const char *entry = h->name;
        /* Strip leading ./ from tar entry names */
        if (entry[0] == '.' && entry[1] == '/') entry += 2;

        if (s_cmp(entry, name) == 0) {
            if (size_out) *size_out = fsize;
            return p + 512;
        }
        p += 512 + padded;
    }
    return 0;
}

void file_iterate(void (*cb)(const char *name, uint32_t size))
{
    if (!initrd_start) return;
    uint8_t *p = initrd_start;
    uint8_t *end = initrd_start + initrd_size;
    while (p + 512 <= end) {
        tar_hdr_t *h = (tar_hdr_t *)p;
        if (h->name[0] == 0) break;
        if (h->magic[0] != 'u' || h->magic[1] != 's' || h->magic[2] != 't' ||
            h->magic[3] != 'a' || h->magic[4] != 'r') break;
        int fsize = oct2int(h->size, 12);
        int padded = (fsize + 511) & ~511;
        const char *entry = h->name;
        if (entry[0] == '.' && entry[1] == '/') entry += 2;
        cb(entry, fsize);
        p += 512 + padded;
    }
}

/* ================================================================
   FRAMEBUFFER INIT
   ================================================================ */

static int fb_init(uint32_t magic, void *mbinfo)
{
    /* Multiboot2 framebuffer */
    if (magic == 0x36D76289) {
        uint8_t *tag = mb2_find_tag(mbinfo, 8);
        if (tag) {
            uint64_t a = *(uint64_t*)(tag + 8);
            uint32_t pitch = *(uint32_t*)(tag + 16);
            uint32_t w = *(uint32_t*)(tag + 20);
            uint32_t h = *(uint32_t*)(tag + 24);
            uint8_t bpp = *(tag + 28);
            if ((bpp == 16 || bpp == 24 || bpp == 32) && w >= 640 && h >= 480) {
                fb_init_ptr((uint32_t*)(uintptr_t)a, (int)w, (int)h, (int)pitch, bpp);
                return 0;
            }
        }
    }

    /* Multiboot v1 framebuffer */
    if (magic == 0x2BADB002) {
        uint32_t flags = *(uint32_t*)mbinfo;
        if (flags & (1 << 12)) {
            uint32_t a_lo = *(uint32_t*)((uint8_t*)mbinfo + 104);
            uint32_t a_hi = *(uint32_t*)((uint8_t*)mbinfo + 108);
            uint64_t a = ((uint64_t)a_hi << 32) | a_lo;
            uint32_t pitch = *(uint32_t*)((uint8_t*)mbinfo + 112);
            uint32_t w = *(uint32_t*)((uint8_t*)mbinfo + 116);
            uint32_t h = *(uint32_t*)((uint8_t*)mbinfo + 120);
            uint8_t bpp = *(uint8_t*)((uint8_t*)mbinfo + 124);
            if ((bpp == 16 || bpp == 24 || bpp == 32) && w >= 640 && h >= 480) {
                fb_init_ptr((uint32_t*)(uintptr_t)a, (int)w, (int)h, (int)pitch, bpp);
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
    uint32_t bg = 0x000000;
    fb_clear(bg);

    uint32_t panel_c = 0x0F0F24;
    int cx = fb_w / 2;
    int pw = 380, ph = 340;
    int px = cx - pw / 2, py = fb_h / 2 - ph / 2;
    fb_rect(px, py, pw, ph, panel_c);
    fb_border(px, py, pw, ph, 0x3C50A0);

    char *title = OS_NAME " x64 Edition";
    int tx = cx - s_len(title) * 4;
    fb_txt(tx, py + 30, title, 0xE6E6F0, panel_c);

    char *ver = OS_VER " " OS_ARCH;
    int vx = cx - s_len(ver) * 4;
    fb_txt(vx, py + 54, ver, 0x5A5A7A, panel_c);

    int pbx = px + 30, pby = py + 85;
    int pbw = pw - 60;
    fb_rect(pbx, pby, pbw, 14, 0x1A1A3A);
    fb_border(pbx, pby, pbw, 14, 0x3C50A0);

    int lx = px + 20, ly = py + 120;
    int next_msg = 0;
    const char *msgs[] = {
        "Bootloader (multiboot)",
        "Framebuffer",
        "Memory map",
        "PCI bus",
        "PS/2 mouse",
        "CPU detection",
        "RTC",
        "Initrd",
    };
    int pcts[] = { 10, 25, 40, 55, 70, 85, 95, 100 };
    int n_msgs = 8;

    for (int p = 0; p <= 100; p++) {
        int bw = ((pbw - 4) * p) / 100;
        fb_rect(pbx + 2, pby + 2, bw, 10, 0x3C50A0);
        if (next_msg < n_msgs && p >= pcts[next_msg]) {
            fb_txt(lx + 10, ly, msgs[next_msg], 0x8888AA, panel_c);
            fb_txt(lx - 10, ly, ">", 0x3C50A0, panel_c);
            ly += 18;
            next_msg++;
        }
        /* Show current resolution */
        char res[32]; int ri = 0;
        int w = fb_w, h = fb_h;
        if (w >= 1000) { res[ri++] = '0' + (w/1000)%10; }
        if (w >= 100)  { res[ri++] = '0' + (w/100)%10; }
        if (w >= 10)   { res[ri++] = '0' + (w/10)%10; }
        res[ri++] = '0' + w%10;
        res[ri++] = 'x';
        if (h >= 1000) { res[ri++] = '0' + (h/1000)%10; }
        if (h >= 100)  { res[ri++] = '0' + (h/100)%10; }
        if (h >= 10)   { res[ri++] = '0' + (h/10)%10; }
        res[ri++] = '0' + h%10;
        res[ri] = 0;
        fb_txt(cx - s_len(res) * 4, py + ph - 30, res, 0x5A5A7A, panel_c);
        fb_flip();
        for (volatile int d = 0; d < 40000; d++);
    }

    int h, m, s, t0;
    rtc_read(&h, &m, &s);
    t0 = h * 3600 + m * 60 + s;
    for (int wait = 0; wait < 400; wait++) {
        rtc_read(&h, &m, &s);
        int t = h * 3600 + m * 60 + s;
        if (t < t0) t += 86400;
        if (t - t0 >= 2) break;
        for (volatile int d = 0; d < 100000; d++);
    }
}

/* ================================================================
   ANIMATION
   ================================================================ */

static void curtain_close(void)
{
    if (fb_w <= 0 || fb_h <= 0) return;
    for (int i = 0; i < fb_h / 2; i += 4) {
        fb_rect(0, i, fb_w, 4, 0x000000);
        fb_rect(0, fb_h - i - 4, fb_w, 4, 0x000000);
        fb_flip();
        for (volatile int d = 0; d < 15000; d++);
    }
    fb_clear(0);
    fb_flip();
}

/* ================================================================
   ACPI S5 + PIIX4 poweroff + ACPI Reset
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
                     did == 0x2440 || did == 0x2480 || did == 0x24C0 ||
                     did == 0x27B0 || did == 0x27B8 || did == 0x27B9 ||
                     did == 0x2810 || did == 0x2811 || did == 0x2812 ||
                     did == 0x2814 || did == 0x2815 ||
                     did == 0x2910 || did == 0x2912 || did == 0x2914 ||
                     did == 0x2916 || did == 0x2918 || did == 0x8119);
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

    /* Try multiboot2 ACPI RSDP tag first */
    if (mb_magic == 0x36D76289) {
        uint8_t *tag = (uint8_t*)mb2_find_tag(mb_info, 20);
        if (tag) {
            uint8_t *rsdp = tag + 8;
            uint8_t sum = 0;
            for (int i = 0; i < 20; i++) sum += rsdp[i];
            if (sum == 0) {
                addr = (uint32_t)(uintptr_t)rsdp;
                goto rsdp_found;
            }
        }
    }

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
    if (rsdt_addr < 0x1000 || rsdt_addr > 0xFFF00000) return;

    volatile uint32_t *rsdt = (volatile uint32_t*)(uintptr_t)rsdt_addr;
    uint32_t rsdt_len = rsdt[1];
    if (rsdt_len < 36 || rsdt_len > 0x10000) return;
    uint32_t entries = (rsdt_len - 36) / 4;

    uint32_t fadt_addr = 0;
    for (uint32_t i = 0; i < entries; i++) {
        uint32_t ptr = rsdt[9 + i];
        if (ptr < 0x1000 || ptr > 0xFFF00000) continue;
        volatile uint32_t *hdr = (volatile uint32_t*)(uintptr_t)ptr;
        if (hdr[0] == 0x50434146) { fadt_addr = ptr; break; }
    }
    if (fadt_addr == 0) return;

    uint32_t fadt_len = *(volatile uint32_t*)(uintptr_t)(fadt_addr + 4);
    if (fadt_len < 72) return;

    uint32_t pm1a_cnt = *(volatile uint32_t*)(uintptr_t)(fadt_addr + 68);
    if (pm1a_cnt < 0x400 || pm1a_cnt > 0x1000) return;

    uint32_t pm1b_cnt = *(volatile uint32_t*)(uintptr_t)(fadt_addr + 72);

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
   ACPI RESET (for reboot)
   ================================================================ */

static void acpi_reset(void)
{
    uint32_t sig0 = 0x20445352;
    uint32_t sig1 = 0x20525450;
    uint32_t addr = 0;

    if (mb_magic == 0x36D76289) {
        uint8_t *tag = (uint8_t*)mb2_find_tag(mb_info, 20);
        if (tag) {
            uint8_t *rsdp = tag + 8;
            uint8_t sum = 0;
            for (int i = 0; i < 20; i++) sum += rsdp[i];
            if (sum == 0) { addr = (uint32_t)(uintptr_t)rsdp; goto rf; }
        }
    }

    uint16_t ebda_seg;
    asm volatile("movw 0x40E, %0" : "=r"(ebda_seg));
    uint32_t ebda = (uint32_t)ebda_seg << 4;
    if (ebda >= 0x80000 && ebda <= 0x9F000) {
        for (uint32_t p = ebda; p < ebda + 0x400; p += 16) {
            volatile uint32_t *pp = (volatile uint32_t*)(uintptr_t)p;
            if (pp[0] == sig0 && pp[1] == sig1) {
                uint8_t sum = 0;
                for (int i = 0; i < 20; i++) sum += ((volatile uint8_t*)(uintptr_t)p)[i];
                if (sum == 0) { addr = p; goto rf; }
            }
        }
    }

    for (uint32_t p = 0xE0000; p < 0xFFFFF; p += 16) {
        volatile uint32_t *pp = (volatile uint32_t*)(uintptr_t)p;
        if (pp[0] == sig0 && pp[1] == sig1) {
            uint8_t sum = 0;
            for (int i = 0; i < 20; i++) sum += ((volatile uint8_t*)(uintptr_t)p)[i];
            if (sum == 0) { addr = p; goto rf; }
        }
    }
    return;

rf:;
    uint32_t rsdt_addr = *(volatile uint32_t*)(uintptr_t)(addr + 16);
    if (rsdt_addr < 0x1000 || rsdt_addr > 0xFFF00000) return;

    volatile uint32_t *rsdt = (volatile uint32_t*)(uintptr_t)rsdt_addr;
    uint32_t rsdt_len = rsdt[1];
    if (rsdt_len < 36 || rsdt_len > 0x10000) return;
    uint32_t entries = (rsdt_len - 36) / 4;

    uint32_t fadt_addr = 0;
    for (uint32_t i = 0; i < entries; i++) {
        uint32_t ptr = rsdt[9 + i];
        if (ptr < 0x1000 || ptr > 0xFFF00000) continue;
        volatile uint32_t *hdr = (volatile uint32_t*)(uintptr_t)ptr;
        if (hdr[0] == 0x50434146) { fadt_addr = ptr; break; }
    }
    if (fadt_addr == 0) return;

    uint32_t fadt_len = *(volatile uint32_t*)(uintptr_t)(fadt_addr + 4);
    if (fadt_len < 132) return;

    uint8_t reset_as = *(volatile uint8_t*)(uintptr_t)(fadt_addr + 116);
    uint64_t reset_addr = *(volatile uint64_t*)(uintptr_t)(fadt_addr + 120);
    uint8_t reset_val = *(volatile uint8_t*)(uintptr_t)(fadt_addr + 128);

    if (reset_addr == 0 || reset_val == 0) return;

    if (reset_as == 1) {
        outb((uint16_t)reset_addr, reset_val);
    } else if (reset_as == 0) {
        volatile uint8_t *p = (volatile uint8_t*)(uintptr_t)reset_addr;
        *p = reset_val;
    }
}

/* ================================================================
   ENTRY
   ================================================================ */

void kmain(uint32_t magic, void *mbinfo)
{
    mb_magic = magic; mb_info = mbinfo;
    if (fb_init(magic, mbinfo) < 0) {
        /* No framebuffer available — halt */
        for (;;) asm("hlt");
    }

    boot_screen();
    mouse_init();

    detect_cpu();
    parse_memory(magic, mbinfo);
    parse_initrd(magic, mbinfo);
    fat_mount();
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
        if ((mouse_btn & 2) && !(prev_btn & 2)) {
            if (act >= 0 && act < nw && wins[act].on_rclick)
                wins[act].on_rclick(act);
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

            /* Global shortcuts */
            if (key == KEY_F4 && kb_mod_alt) {
                if (nw > 0) gui_wclose(act);
                goto render_choice;
            }

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

        if (gui_tick) gui_tick();
    }

    if (run == -1) {
        curtain_close();
        int dy = 10;
        #define DBG(s) do { fb_txt(10, dy, s, 0xFFFFFF, 0x000000); fb_flip(); dy += 18; } while(0)
        DBG("Reboot: ACPI reset...");
        acpi_reset();
        for (volatile int d = 0; d < 30000; d++);
        DBG("Reboot: keyboard reset...");
        outb(0x64, 0xFE);
        for (volatile int d = 0; d < 30000; d++);
        DBG("Reboot: triple fault...");
        __asm__ __volatile__(
            "pushq $0\n\t"
            "pushq $0\n\t"
            "lidt (%%rsp)\n\t"
            "ud2\n\t"
            : : : "memory"
        );
        for (volatile int d = 0; d < 30000; d++);
        DBG("Reboot: ALL FAILED, halting");
        for (;;) asm("hlt");
        #undef DBG
    }

    /* Shutdown */
    curtain_close();
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
