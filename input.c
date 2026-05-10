#include <stdint.h>
#include "fb.h"
#include "input.h"

static inline void outb(uint16_t p, uint8_t v)
{ asm volatile("outb %0, %1" : : "a"(v), "Nd"(p)); }

static inline uint8_t inb(uint16_t p)
{ uint8_t v; asm volatile("inb %1, %0" : "=a"(v) : "Nd"(p)); return v; }

int mouse_x, mouse_y, mouse_btn;

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

/* mouse_process is defined below */
static void mouse_process(uint8_t data);

static void kb_push(int c)
{
    int n = (kbh + 1) % KB_BUF;
    if (n != kbt) { kbq[kbh] = c; kbh = n; }
}

void kb_poll(void)
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
                if (data == 0x5B) kb_push(KEY_SUPER);
                ext_key = 0;
            } else {
                char c = shift ? kbs[data] : kbm[data];
                if (c) kb_push(c);
            }
        }
    }
}

int kb_pop(void)
{
    if (kbh == kbt) return -1;
    int c = kbq[kbt]; kbt = (kbt + 1) % KB_BUF; return c;
}

/* ================================================================
   MOUSE
   ================================================================ */

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

void mouse_init(void)
{
    outb(0x64, 0xA8);
    while (inb(0x64) & 2);
    outb(0x64, 0xD4);
    while (inb(0x64) & 2);
    outb(0x60, 0xF6);
    while (!(inb(0x64) & 1));
    inb(0x60);
    outb(0x64, 0xD4);
    while (inb(0x64) & 2);
    outb(0x60, 0xF4);
    while (!(inb(0x64) & 1));
    inb(0x60);
    mouse_x = fb_w / 2;
    mouse_y = fb_h / 2;
}
