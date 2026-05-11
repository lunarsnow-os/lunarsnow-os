#include <stdint.h>
#include "font8x16.h"
#include "fb.h"

int fb_w, fb_h, fb_pch, fb_bpp;
uint32_t *sbuf;
static uint32_t *fb;
static uint32_t shadow[800 * 600];

void fb_init_ptr(uint32_t *addr, int w, int h, int pch, int bpp)
{
    fb = addr;
    sbuf = shadow;
    fb_w = w;
    fb_h = h;
    fb_pch = pch;
    fb_bpp = bpp;
}

void fb_flip(void)
{
    uint8_t *d = (uint8_t*)fb;
    uint32_t *s = shadow;
    for (int y = 0; y < fb_h; y++) {
        uint32_t *rs = s + y * fb_w;
        if (fb_bpp == 32) {
            uint32_t *rd = (uint32_t*)(d + y * fb_pch);
            for (int x = 0; x < fb_w; x++) rd[x] = rs[x];
        } else if (fb_bpp == 24) {
            uint8_t *rd = d + y * fb_pch;
            for (int x = 0; x < fb_w; x++) {
                uint32_t px = rs[x];
                rd[x * 3 + 0] = px;
                rd[x * 3 + 1] = px >> 8;
                rd[x * 3 + 2] = px >> 16;
            }
        } else if (fb_bpp == 16) {
            uint16_t *rd = (uint16_t*)(d + y * fb_pch);
            for (int x = 0; x < fb_w; x++) {
                uint32_t px = rs[x];
                uint8_t r = px >> 16, g = px >> 8, b = px;
                rd[x] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            }
        }
    }
}

void fb_flip_rect(int x, int y, int w, int h)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= fb_w || y >= fb_h || w <= 0 || h <= 0) return;
    if (x + w > fb_w) w = fb_w - x;
    if (y + h > fb_h) h = fb_h - y;
    for (int r = y; r < y + h; r++) {
        uint32_t *s = sbuf + r * fb_w + x;
        uint8_t *d = (uint8_t*)fb + r * fb_pch;
        if (fb_bpp == 32) {
            uint32_t *rd = (uint32_t*)d + x;
            for (int c = 0; c < w; c++) rd[c] = s[c];
        } else if (fb_bpp == 24) {
            d += x * 3;
            for (int c = 0; c < w; c++) {
                uint32_t px = s[c];
                d[c * 3] = px; d[c * 3 + 1] = px >> 8; d[c * 3 + 2] = px >> 16;
            }
        } else if (fb_bpp == 16) {
            uint16_t *rd = (uint16_t*)d + x;
            for (int c = 0; c < w; c++) {
                uint32_t px = s[c];
                uint8_t rv = px >> 16, gv = px >> 8, bv = px;
                rd[c] = ((rv >> 3) << 11) | ((gv >> 2) << 5) | (bv >> 3);
            }
        }
    }
}

void fb_pixel(int x, int y, uint32_t c)
{
    if (x < 0 || x >= fb_w || y < 0 || y >= fb_h) return;
    sbuf[y * fb_w + x] = c;
}

void fb_rect(int x, int y, int w, int h, uint32_t c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (w <= 0 || h <= 0) return;
    for (int r = y; r < y + h && r < fb_h; r++)
        for (int cl = x; cl < x + w && cl < fb_w; cl++)
            sbuf[r * fb_w + cl] = c;
}

void fb_chr(int x, int y, unsigned char ch, uint32_t fg, uint32_t bg)
{
    if (ch < 32 || ch > 126) ch = ' ';
    int idx = ch - 32;
    for (int row = 0; row < 16; row++) {
        unsigned char bits = font8x16[idx][row];
        for (int col = 0; col < 8; col++)
            fb_pixel(x + col, y + row, (bits & (0x80 >> col)) ? fg : bg);
    }
}

void fb_txt(int x, int y, const char *s, uint32_t fg, uint32_t bg)
{
    for (; *s; s++) { fb_chr(x, y, *s, fg, bg); x += 8; }
}

void fb_border(int x, int y, int w, int h, uint32_t c)
{
    fb_rect(x, y, w, 1, c); fb_rect(x, y + h - 1, w, 1, c);
    fb_rect(x, y, 1, h, c); fb_rect(x + w - 1, y, 1, h, c);
}

void fb_clear(uint32_t c)
{
    fb_rect(0, 0, fb_w, fb_h, c);
}

int s_len(const char *s)
{
    int n = 0; while (s[n]) n++; return n;
}

void s_cpy(char *d, const char *s, int max)
{
    int i;
    for (i = 0; i < max - 1 && s[i]; i++) d[i] = s[i];
    d[i] = 0;
}

void mcpy(void *d, const void *s, int n)
{
    unsigned char *cd = d;
    const unsigned char *cs = s;
    while (n--) *cd++ = *cs++;
}
