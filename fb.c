#include <stdint.h>
#include "font8x16.h"
#include "fb.h"

int fb_w, fb_h, fb_pch, fb_bpp, fb_type;

uint32_t *sbuf;

static uint32_t *fb;

/* 4K shadow buffer */
static uint32_t shadow_buf[3840 * 2160];
static uint32_t *shadow;

void fb_init_ptr(uint32_t *addr, int w, int h, int pch, int bpp)
{
    fb = addr;

    fb_w = w;
    fb_h = h;
    fb_pch = pch;
    fb_bpp = bpp;

    shadow = shadow_buf;
    sbuf = shadow;
}

void mcpy(void *d, const void *s, int n)
{
    unsigned char *cd = d;
    const unsigned char *cs = s;

    while (n--)
        *cd++ = *cs++;
}

void fb_flip(void)
{
    uint8_t *d = (uint8_t*)fb;
    uint32_t *s = shadow;

    for (int y = 0; y < fb_h; y++) {

        uint32_t *rs = s + (y * fb_w);

        if (fb_bpp == 32) {

            uint32_t *rd = (uint32_t*)(d + (y * fb_pch));

            mcpy(rd, rs, fb_w * 4);

        } else if (fb_bpp == 24) {

            uint8_t *rd = d + (y * fb_pch);

            for (int x = 0; x < fb_w; x++) {

                uint32_t px = rs[x];

                rd[x * 3 + 0] = (uint8_t)(px);
                rd[x * 3 + 1] = (uint8_t)(px >> 8);
                rd[x * 3 + 2] = (uint8_t)(px >> 16);
            }

        } else if (fb_bpp == 16) {

            uint16_t *rd = (uint16_t*)(d + (y * fb_pch));

            for (int x = 0; x < fb_w; x++) {

                uint32_t px = rs[x];

                uint8_t r = (px >> 16) & 0xFF;
                uint8_t g = (px >> 8) & 0xFF;
                uint8_t b = px & 0xFF;

                rd[x] =
                    ((r >> 3) << 11) |
                    ((g >> 2) << 5)  |
                    (b >> 3);
            }
        }
    }
}

void fb_flip_rect(int x, int y, int w, int h)
{
    if (x < 0) {
        w += x;
        x = 0;
    }

    if (y < 0) {
        h += y;
        y = 0;
    }

    if (x >= fb_w || y >= fb_h || w <= 0 || h <= 0)
        return;

    if (x + w > fb_w)
        w = fb_w - x;

    if (y + h > fb_h)
        h = fb_h - y;

    for (int r = y; r < y + h; r++) {

        uint32_t *s = sbuf + (r * fb_w) + x;

        uint8_t *d = (uint8_t*)fb + (r * fb_pch);

        if (fb_bpp == 32) {

            uint32_t *rd = (uint32_t*)d + x;

            mcpy(rd, s, w * 4);

        } else if (fb_bpp == 24) {

            d += x * 3;

            for (int c = 0; c < w; c++) {

                uint32_t px = s[c];

                d[c * 3 + 0] = (uint8_t)(px);
                d[c * 3 + 1] = (uint8_t)(px >> 8);
                d[c * 3 + 2] = (uint8_t)(px >> 16);
            }

        } else if (fb_bpp == 16) {

            uint16_t *rd = (uint16_t*)d + x;

            for (int c = 0; c < w; c++) {

                uint32_t px = s[c];

                uint8_t r8 = (px >> 16) & 0xFF;
                uint8_t g8 = (px >> 8) & 0xFF;
                uint8_t b8 = px & 0xFF;

                rd[c] =
                    ((r8 >> 3) << 11) |
                    ((g8 >> 2) << 5)  |
                    (b8 >> 3);
            }
        }
    }
}

void fb_pixel(int x, int y, uint32_t c)
{
    if (x < 0 || x >= fb_w)
        return;

    if (y < 0 || y >= fb_h)
        return;

    sbuf[(y * fb_w) + x] = c;
}

void fb_rect(int x, int y, int w, int h, uint32_t c)
{
    if (x < 0) {
        w += x;
        x = 0;
    }

    if (y < 0) {
        h += y;
        y = 0;
    }

    if (w <= 0 || h <= 0)
        return;

    for (int r = y; r < y + h && r < fb_h; r++) {

        uint32_t *row = sbuf + (r * fb_w);

        for (int cl = x; cl < x + w && cl < fb_w; cl++)
            row[cl] = c;
    }
}

void fb_border(int x, int y, int w, int h, uint32_t c)
{
    fb_rect(x, y, w, 1, c);
    fb_rect(x, y + h - 1, w, 1, c);

    fb_rect(x, y, 1, h, c);
    fb_rect(x + w - 1, y, 1, h, c);
}

void fb_clear(uint32_t c)
{
    fb_rect(0, 0, fb_w, fb_h, c);
}

void fb_chr(int x, int y, unsigned char ch, uint32_t fg, uint32_t bg)
{
    if (ch < 32 || ch > 126)
        ch = ' ';

    int idx = ch - 32;

    for (int row = 0; row < 16; row++) {

        unsigned char bits = font8x16[idx][row];

        for (int col = 0; col < 8; col++) {

            fb_pixel(
                x + col,
                y + row,
                (bits & (0x80 >> col)) ? fg : bg
            );
        }
    }
}

void fb_txt(int x, int y, const char *s, uint32_t fg, uint32_t bg)
{
    while (*s) {

        fb_chr(x, y, *s, fg, bg);

        x += 8;
        s++;
    }
}

uint32_t *fb_get_addr(void)
{
    return fb;
}

int s_len(const char *s)
{
    int n = 0;

    while (s[n])
        n++;

    return n;
}

void s_cpy(char *d, const char *s, int max)
{
    int i;

    for (i = 0; i < max - 1 && s[i]; i++)
        d[i] = s[i];

    d[i] = 0;
}

int s_cmp(const char *a, const char *b)
{
    while (*a && (*a == *b)) {
        a++;
        b++;
    }

    return *(unsigned char*)a - *(unsigned char*)b;
}

int s_cmpi(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    if (*a) return 1;
    if (*b) return -1;
    return 0;
}

void str_int(char *buf, int val)
{
    if (val == 0) {

        buf[0] = '0';
        buf[1] = 0;

        return;
    }

    if (val < 0) {

        buf[0] = '-';

        str_int(buf + 1, -val);

        return;
    }

    char tmp[16];

    int i = 0;

    while (val > 0) {

        tmp[i++] = '0' + (val % 10);

        val /= 10;
    }

    int j = 0;

    while (i > 0)
        buf[j++] = tmp[--i];

    buf[j] = 0;
}