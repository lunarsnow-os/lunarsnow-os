#include <stdint.h>
#include "font8x16.h"

/* ================================================================
   UTILITY
   ================================================================ */

static int s_len(const char *s)
{
    int n = 0; while (s[n]) n++; return n;
}

static void s_cpy(char *d, const char *s, int max)
{
    int i;
    for (i = 0; i < max - 1 && s[i]; i++) d[i] = s[i];
    d[i] = 0;
}

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

static void rtc_read(int *h, int *m, int *s)
{
    uint8_t sec, min, hour;
    /* wait until no update in progress */
    while (cmos_r(0x0A) & 0x80);
    sec  = cmos_r(0x00);
    min  = cmos_r(0x02);
    hour = cmos_r(0x04);
    /* read again if UIP changed during read */
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
    vbe_w(4, 0x00);  /* disable */
    vbe_w(1, w);     /* X resolution */
    vbe_w(2, h);     /* Y resolution */
    vbe_w(3, bpp);   /* bits per pixel */
    vbe_w(4, 0x41);  /* enable + LFB */
}

/* ================================================================
   FRAMEBUFFER
   ================================================================ */

static uint32_t *fb;
static int fb_w, fb_h, fb_pch;

/* shadow buffer for double buffering */
static uint32_t shadow[800 * 600];
static uint32_t *sbuf;

#define RGB(r,g,b)  ((uint32_t)(((r)<<16)|((g)<<8)|(b)))

static void flip(void)
{
    uint32_t *s = shadow;
    uint32_t *d = fb;
    int n = fb_w * fb_h;
    while (n--) *d++ = *s++;
}

static void pixel(int x, int y, uint32_t c)
{
    if (x < 0 || x >= fb_w || y < 0 || y >= fb_h) return;
    sbuf[y * (fb_pch / 4) + x] = c;
}

static void rect(int x, int y, int w, int h, uint32_t c)
{
    for (int r = y; r < y + h && r < fb_h; r++)
        for (int cl = x; cl < x + w && cl < fb_w; cl++)
            sbuf[r * (fb_pch / 4) + cl] = c;
}

static void chr(int x, int y, unsigned char ch, uint32_t fg, uint32_t bg)
{
    if (ch < 32 || ch > 126) ch = ' ';
    int idx = ch - 32;
    for (int row = 0; row < 16; row++) {
        unsigned char bits = font8x16[idx][row];
        for (int col = 0; col < 8; col++)
            pixel(x + col, y + row, (bits & (0x80 >> col)) ? fg : bg);
    }
}

static void txt(int x, int y, const char *s, uint32_t fg, uint32_t bg)
{
    for (; *s; s++) { chr(x, y, *s, fg, bg); x += 8; }
}

static void border(int x, int y, int w, int h, uint32_t c)
{
    rect(x, y, w, 1, c); rect(x, y + h - 1, w, 1, c);
    rect(x, y, 1, h, c); rect(x + w - 1, y, 1, h, c);
}

/* forward declarations */
static void mouse_process(uint8_t data);
static void mouse_draw(void);
static void wclose(int idx);
static void cb_new(void);
static void cb_about(void);
static void cb_shutdown(void);

/* ================================================================
   KEYBOARD
   ================================================================ */

#define KB_S 0x64
#define KB_D 0x60

static int shift;

static const char kbm[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=',8,
    9,'q','w','e','r','t','y','u','i','o','p','[',']',10,
    0,'a','s','d','f','g','h','j','k','l',';',39,'`',
    0,92,'z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,'-',0,0,0,'+',0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static const char kbs[128] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+',8,
    9,'Q','W','E','R','T','Y','U','I','O','P','{','}',10,
    0,'A','S','D','F','G','H','J','K','L',':',34,'~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,'-',0,0,0,'+',0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

#define KB_BUF 32
static int kbq[KB_BUF], kbh, kbt;
static int ext_key;

#define KEY_UP   256
#define KEY_DOWN 257

static void kb_push(int c)
{
    int n = (kbh + 1) % KB_BUF;
    if (n != kbt) { kbq[kbh] = c; kbh = n; }
}

static int kb_pop(void)
{
    if (kbh == kbt) return -1;
    int c = kbq[kbt]; kbt = (kbt + 1) % KB_BUF; return c;
}

static void kb_poll(void)
{
    while (inb(KB_S) & 1) {
        uint8_t st = inb(KB_S);
        if (!(st & 1)) break;
        uint8_t data = inb(KB_D);
        if (st & 0x20) { mouse_process(data); continue; }
        if (data == 0xE0) { ext_key = 1; continue; }
        if (data & 0x80) {
            uint8_t mk = data & 0x7F;
            if (mk == 0x2A || mk == 0x36) shift = 0;
            ext_key = 0;
        } else {
            if (data == 0x2A || data == 0x36) { shift = 1; ext_key = 0; }
            else if (ext_key) {
                if (data == 0x48) kb_push(KEY_UP);
                if (data == 0x50) kb_push(KEY_DOWN);
                ext_key = 0;
            } else {
                char c = shift ? kbs[data] : kbm[data];
                if (c) kb_push(c);
            }
        }
    }
}

/* ================================================================
   MOUSE
   ================================================================ */

static int mouse_x, mouse_y;
static int mouse_btn;
static int mouse_drag;
static int mouse_drag_win, mouse_drag_ox, mouse_drag_oy;
static int mouse_cycle;
static uint8_t mouse_pkt[3];

static void mouse_process(uint8_t data)
{
    if (mouse_cycle == 0) {
        if (!(data & 0x08)) return;
        mouse_pkt[0] = data; mouse_cycle = 1;
    } else if (mouse_cycle == 1) {
        mouse_pkt[1] = data; mouse_cycle = 2;
    } else {
        mouse_pkt[2] = data; mouse_cycle = 0;
        int dx = (int8_t)mouse_pkt[1];
        int dy = -(int8_t)mouse_pkt[2];
        mouse_btn = mouse_pkt[0] & 3;
        mouse_x += dx; mouse_y += dy;
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_x >= fb_w) mouse_x = fb_w - 1;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_y >= fb_h) mouse_y = fb_h - 1;
    }
}

static void mouse_init(void)
{
    outb(0x64, 0xA8);
    while (inb(0x64) & 2);
    outb(0x64, 0xD4);
    while (inb(0x64) & 2);
    outb(0x60, 0xF6);
    while (!(inb(0x64) & 1)) { if (!(inb(0x64) & 1)) continue; break; }
    inb(0x60);
    outb(0x64, 0xD4);
    while (inb(0x64) & 2);
    outb(0x60, 0xF4);
    while (!(inb(0x64) & 1)) { if (!(inb(0x64) & 1)) continue; break; }
    inb(0x60);
    mouse_x = fb_w / 2;
    mouse_y = fb_h / 2;
}

/* ================================================================
   GUI
   ================================================================ */

#define MAX_W 8
#define MAX_B 16

typedef struct { int x, y, w, h; char t[24]; void (*cb)(void); } Btn;

typedef struct {
    int x, y, w, h;
    char title[24];
    uint32_t bg, tb;
    Btn btns[MAX_B];
    int nb, fc;
} Win;

static Win wins[MAX_W];
static int nw, act, run, about_win = -1;

enum {
    C_DSK    = 0x0F0F1C,
    C_WBG    = 0x1E1E32,
    C_TAC    = 0x3C50A0,
    C_TIN    = 0x323246,
    C_BDR    = 0x505078,
    C_BN     = 0x327878,
    C_BF     = 0x4A9696,
    C_BT     = 0xC8C8D2,
    C_LBL    = 0xB4B4BE,
    C_TTT    = 0xE6E6F0,
    C_CLS    = 0xB43232,
    C_TBAR   = 0x16162A,
    C_START  = 0x3C50A0,
    C_STARTF = 0x5A70C0,
    C_TBTN   = 0x282845,
    C_TBTNA  = 0x3C3C60,
    C_MBG    = 0x1E1E35,
    C_MFOC   = 0x3C50A0,
};

#define TB_H  28    /* taskbar height */
#define ST_W  60    /* start button width */
#define MN_W  160   /* menu width */

/* focus: 0 = window buttons, 1 = Start button, 2 = menu */
static int focus_mode;
static int menu_focus, menu_open;

static int wnew(const char *t, int x, int y, int w, int h)
{
    if (nw >= MAX_W) return -1;
    int i = nw++;
    wins[i].x = x; wins[i].y = y; wins[i].w = w; wins[i].h = h;
    s_cpy(wins[i].title, t, 24);
    wins[i].bg = C_WBG; wins[i].tb = C_TAC;
    wins[i].nb = 0; wins[i].fc = 0;
    act = i; return i;
}

static int wbtn(int wi, const char *t, int x, int y, int w, int h,
                void (*cb)(void))
{
    if (wi < 0 || wi >= nw) return -1;
    Btn *b = &wins[wi].btns[wins[wi].nb++];
    b->x = x; b->y = y; b->w = w; b->h = h; b->cb = cb;
    s_cpy(b->t, t, 24); return wins[wi].nb - 1;
}

static void wdraw(int wi)
{
    Win *w = &wins[wi];
    int x = w->x, y = w->y, ww = w->w, hh = w->h;
    int a = (wi == act);
    uint32_t tc = a ? C_TAC : C_TIN;
    uint32_t bc = a ? C_BDR : C_TIN;

    rect(x + 2, y + 20, ww - 4, hh - 22, w->bg);
    rect(x + 2, y + 2, ww - 4, 18, tc);
    txt(x + 6, y + 3, w->title, C_TTT, tc);

    rect(x + ww - 18, y + 3, 15, 15, C_CLS);
    txt(x + ww - 15, y + 4, "X", C_TTT, C_CLS);

    border(x, y, ww, hh, bc);

    for (int i = 0; i < w->nb; i++) {
        Btn *b = &w->btns[i];
        int bx = x + 2 + b->x, by = y + 20 + b->y;
        int f = (i == w->fc && wi == act);
        uint32_t col = f ? C_BF : C_BN;
        rect(bx, by, b->w, b->h, col);
        border(bx, by, b->w, b->h, f ? 0xFFFFFF : 0x1E4E4E);
        int l = s_len(b->t);
        txt(bx + (b->w - l * 8) / 2, by + (b->h - 16) / 2, b->t, C_BT, col);
    }
}

static const char *menu_items[] = { "New Window", "About", "Shutdown" };
static int menu_n = 3;

static void render(void)
{
    rect(0, 0, fb_w, fb_h, C_DSK);
    for (int i = 0; i < nw; i++) wdraw(i);

    /* Taskbar */
    rect(0, fb_h - TB_H, fb_w, TB_H, C_TBAR);
    border(0, fb_h - TB_H, fb_w, TB_H, C_BDR);

    uint32_t scol = (focus_mode == 1) ? C_STARTF : C_START;
    rect(2, fb_h - TB_H + 2, ST_W, TB_H - 4, scol);
    txt(8, fb_h - TB_H + 6, "Start", C_TTT, scol);

    int bx = ST_W + 6;
    for (int i = 0; i < nw; i++) {
        uint32_t c = (i == act) ? C_TBTNA : C_TBTN;
        int bw = s_len(wins[i].title) * 8 + 12;
        if (bw > 140) bw = 140;
        rect(bx, fb_h - TB_H + 2, bw, TB_H - 4, c);
        txt(bx + 4, fb_h - TB_H + 6, wins[i].title, C_TTT, c);
        bx += bw + 2;
    }

    /* Clock */
    int ch, cm, cs;
    rtc_read(&ch, &cm, &cs);
    char tstr[6]; int ti = 0;
    tstr[ti++] = '0' + ch / 10;
    tstr[ti++] = '0' + ch % 10;
    tstr[ti++] = ':';
    tstr[ti++] = '0' + cm / 10;
    tstr[ti++] = '0' + cm % 10;
    tstr[ti] = 0;
    int cx = fb_w - 4 - 8 * 5;
    txt(cx, fb_h - TB_H + 6, tstr, C_TTT, C_TBAR);

    /* Start menu */
    if (menu_open) {
        int mx = 2;
        int my = fb_h - TB_H - menu_n * 24 - 4;
        rect(mx, my, MN_W, menu_n * 24 + 4, C_MBG);
        border(mx, my, MN_W, menu_n * 24 + 4, C_BDR);
        for (int i = 0; i < menu_n; i++) {
            uint32_t mc = (i == menu_focus && focus_mode == 2) ? C_MFOC : C_MBG;
            rect(mx + 2, my + 2 + i * 24, MN_W - 4, 24, mc);
            txt(mx + 6, my + 4 + i * 24, menu_items[i], C_TTT, mc);
        }
    }

    if (about_win >= 0 && about_win < nw) {
        int wx = wins[about_win].x + 10;
        int wy = wins[about_win].y + 28;
        txt(wx, wy, "Lesano", C_TTT, wins[about_win].bg);
        txt(wx, wy + 18, "Nixxlte ver Alpha 0.1", C_LBL, wins[about_win].bg);
    }

    mouse_draw();
    flip();
}

static void mcpy(void *d, const void *s, int n)
{
    unsigned char *cd = d;
    const unsigned char *cs = s;
    while (n--) *cd++ = *cs++;
}

static void wclose(int idx)
{
    if (idx < 0 || idx >= nw) return;
    for (int i = idx; i < nw - 1; i++) mcpy(&wins[i], &wins[i+1], sizeof(Win));
    nw--;
    if (act >= nw && nw > 0) act = nw - 1;
    if (about_win == idx) about_win = -1;
    else if (about_win > idx) about_win--;
}

/* ================================================================
   MOUSE CURSOR + CLICK
   ================================================================ */

static const uint8_t curs_img[] = {
    0x80, 0xC0, 0xA0, 0x90, 0x88, 0x84,
    0x82, 0x81, 0x80, 0xA0, 0x50, 0x20
};

static void mouse_draw(void)
{
    int x = mouse_x, y = mouse_y;
    for (int r = 0; r < 12 && y + r + 1 < fb_h; r++) {
        uint8_t bits = curs_img[r];
        for (int c = 0; c < 8 && x + c + 1 < fb_w; c++)
            if (bits & (0x80 >> c))
                sbuf[(y + r + 1) * (fb_pch / 4) + (x + c + 1)] = 0x000000;
    }
    for (int r = 0; r < 12 && y + r < fb_h; r++) {
        uint8_t bits = curs_img[r];
        for (int c = 0; c < 8 && x + c < fb_w; c++)
            if (bits & (0x80 >> c))
                sbuf[(y + r) * (fb_pch / 4) + (x + c)] = 0xFFFFFF;
    }
}

static void mouse_click(void)
{
    int x = mouse_x, y = mouse_y;

    if (menu_open) {
        int mx = 2;
        int my = fb_h - TB_H - menu_n * 24 - 4;
        if (x >= mx && x < mx + MN_W && y >= my && y < my + menu_n * 24 + 4) {
            int item = (y - my - 2) / 24;
            if (item >= 0 && item < menu_n) {
                if (item == 0) cb_new();
                if (item == 1) cb_about();
                if (item == 2) cb_shutdown();
            }
        }
        menu_open = 0; focus_mode = 0;
        return;
    }

    if (y >= fb_h - TB_H) {
        if (x < ST_W + 2) {
            menu_open = 1; menu_focus = 0; focus_mode = 2;
            return;
        }
        int bx = ST_W + 6;
        for (int i = 0; i < nw; i++) {
            int bw = s_len(wins[i].title) * 8 + 12;
            if (bw > 140) bw = 140;
            if (x >= bx && x < bx + bw) { act = i; focus_mode = 0; return; }
            bx += bw + 2;
        }
        return;
    }

    for (int i = nw - 1; i >= 0; i--) {
        Win *w = &wins[i];
        int wx = w->x, wy = w->y, ww = w->w, wh = w->h;
        if (x < wx || x >= wx + ww || y < wy || y >= wy + wh) continue;
        act = i; focus_mode = 0;

        if (x >= wx + ww - 18 && x < wx + ww - 3 &&
            y >= wy + 3 && y < wy + 18) {
            wclose(i); return;
        }
        if (y >= wy + 2 && y < wy + 20) {
            mouse_drag = 1;
            mouse_drag_win = i;
            mouse_drag_ox = x - wx;
            mouse_drag_oy = y - wy;
            return;
        }
        for (int j = 0; j < w->nb; j++) {
            Btn *b = &w->btns[j];
            int bx = wx + 2 + b->x, by = wy + 20 + b->y;
            if (x >= bx && x < bx + b->w && y >= by && y < by + b->h) {
                if (b->cb) b->cb();
                return;
            }
        }
        return;
    }
}

/* ================================================================
   CALLBACKS
   ================================================================ */

static int ccount;
static void cb_close(void) { if (act >= 0 && act < nw) wclose(act); }
static void cb_shutdown(void) { run = 0; }

static void cb_new(void)
{
    ccount++;
    char title[16];
    int i;
    for (i = 0; "Window"[i]; i++) title[i] = "Window"[i];
    title[i] = '0' + ccount; title[++i] = 0;
    int wi = wnew(title, 80 + (ccount % 6) * 30, 80 + (ccount % 5) * 30,
                  260, 140);
    wbtn(wi, "Close", 100, 80, 60, 26, cb_close);
}

static void cb_about(void)
{
    about_win = wnew("About", 230, 180, 280, 140);
    wbtn(about_win, "OK", 110, 90, 60, 26, cb_close);
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

    fb   = (uint32_t*)fb_addr;
    sbuf = shadow;
    fb_w = 800;
    fb_h = 600;
    fb_pch = 800 * 4;
    return 0;
}

/* ================================================================
   ENTRY
   ================================================================ */

void kmain(uint32_t magic, void *mbinfo)
{
    (void)magic; (void)mbinfo;
    if (fb_init() < 0) return;

    mouse_init();

    run = 1;

    while (run) {
        int prev_btn = mouse_btn;
        kb_poll();

        if ((mouse_btn & 1) && !(prev_btn & 1))
            mouse_click();
        if (mouse_drag && !(mouse_btn & 1))
            mouse_drag = 0;
        if (mouse_drag)
            wins[mouse_drag_win].x = mouse_x - mouse_drag_ox,
            wins[mouse_drag_win].y = mouse_y - mouse_drag_oy;

        int key = kb_pop();
        if (key == '\t') {
            if (menu_open) {
                menu_focus = (menu_focus + 1) % menu_n;
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
                if (menu_focus == 0) cb_new();
                if (menu_focus == 1) cb_about();
                if (menu_focus == 2) cb_shutdown();
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
            else if (nw > 1) wclose(act);
        }
        if (key == KEY_UP && menu_open)
            menu_focus = (menu_focus - 1 + menu_n) % menu_n;
        if (key == KEY_DOWN && menu_open)
            menu_focus = (menu_focus + 1) % menu_n;

        render();
    }

    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    for (;;) asm("hlt");
}
