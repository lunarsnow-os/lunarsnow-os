#include <stdint.h>
#include "font8x16.h"
#include "fb.h"

int fb_w, fb_h, fb_pch;
uint32_t *sbuf;
static uint32_t *fb;
static uint32_t shadow[800 * 600];

void fb_init_ptr(uint32_t *addr, int w, int h, int pch)
{
    fb = addr;
    sbuf = shadow;
    fb_w = w;
    fb_h = h;
    fb_pch = pch;
}

void fb_flip(void)
{
    uint32_t *s = shadow;
    uint32_t *d = fb;
    int n = fb_w * fb_h;
    while (n--) *d++ = *s++;
}

void fb_pixel(int x, int y, uint32_t c)
{
    if (x < 0 || x >= fb_w || y < 0 || y >= fb_h) return;
    sbuf[y * (fb_pch / 4) + x] = c;
}

void fb_rect(int x, int y, int w, int h, uint32_t c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (w <= 0 || h <= 0) return;
    for (int r = y; r < y + h && r < fb_h; r++)
        for (int cl = x; cl < x + w && cl < fb_w; cl++)
            sbuf[r * (fb_pch / 4) + cl] = c;
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
