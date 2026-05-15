#include "lunarsnow.h"
#include "apps.h"
#include "fs.h"

typedef struct {
    uint8_t data[131072];
    uint32_t size;
    int valid;
    int w, h, bpp, row_size, pix_off, top_down;
    int draw_w, draw_h, scale_x, scale_y;
} BMPCtx;

static BMPCtx bmp_ctx;

static int bmp_read(const char *name)
{
    uint8_t *p = file_read(name, &bmp_ctx.size);
    if (p) {
        if (bmp_ctx.size > 131072) bmp_ctx.size = 131072;
        mcpy(bmp_ctx.data, p, bmp_ctx.size);
        return 0;
    }
    if (snowfs_mounted && snowfs_read(name, bmp_ctx.data, &bmp_ctx.size) >= 0) {
        if (bmp_ctx.size > 131072) bmp_ctx.size = 131072;
        return 0;
    }
    if (fat_read_file(name, bmp_ctx.data, &bmp_ctx.size) >= 0) {
        if (bmp_ctx.size > 131072) bmp_ctx.size = 131072;
        return 0;
    }
    return -1;
}

static uint32_t rd32(const uint8_t *p, int off)
{
    return (uint32_t)p[off] | ((uint32_t)p[off + 1] << 8)
         | ((uint32_t)p[off + 2] << 16) | ((uint32_t)p[off + 3] << 24);
}

static uint16_t rd16(const uint8_t *p, int off)
{
    return (uint16_t)p[off] | ((uint32_t)p[off + 1] << 8);
}

static void draw(int wi)
{
    Win *w = &wins[wi];
    (void)w;

    if (!bmp_ctx.valid) return;

    int wx = w->x + 12, wy = w->y + 28;
    int px_off = bmp_ctx.pix_off;
    int bw = bmp_ctx.w, bh = bmp_ctx.h;
    int row = bmp_ctx.row_size;
    int bpp = bmp_ctx.bpp;
    int bpp_bytes = bpp / 8;

    for (int dy = 0; dy < bmp_ctx.draw_h; dy++) {
        int sy = (dy * bmp_ctx.scale_y) >> 16;
        if (sy >= bh) sy = bh - 1;
        int src_y = bmp_ctx.top_down ? sy : (bh - 1 - sy);
        int src_off = px_off + src_y * row;

        for (int dx = 0; dx < bmp_ctx.draw_w; dx++) {
            int sx = (dx * bmp_ctx.scale_x) >> 16;
            if (sx >= bw) sx = bw - 1;
            int off = src_off + sx * bpp_bytes;
            uint32_t col = bmp_ctx.data[off]
                | ((uint32_t)bmp_ctx.data[off + 1] << 8)
                | ((uint32_t)bmp_ctx.data[off + 2] << 16);
            fb_pixel(wx + dx, wy + dy, col);
        }
    }
}

static void on_close(int wi)
{
    (void)wi;
    bmp_ctx.valid = 0;
}

void prog_bmpviewer(void)
{
    msgbox("BMP Viewer", "Open .bmp files from File Manager");
}

void prog_bmpview(const char *name)
{
    if (bmp_read(name) < 0) return;

    uint32_t sz = bmp_ctx.size;
    uint8_t *d = bmp_ctx.data;

    if (sz < 54 || d[0] != 'B' || d[1] != 'M') { bmp_ctx.valid = 0; return; }

    bmp_ctx.pix_off = rd32(d, 10);
    bmp_ctx.w = (int)rd32(d, 18);
    bmp_ctx.h = (int)rd32(d, 22);

    if (bmp_ctx.h < 0) { bmp_ctx.h = -bmp_ctx.h; bmp_ctx.top_down = 1; }
    else { bmp_ctx.top_down = 0; }

    bmp_ctx.bpp = rd16(d, 28);
    if (bmp_ctx.bpp != 24 && bmp_ctx.bpp != 32) { bmp_ctx.valid = 0; return; }

    bmp_ctx.row_size = ((bmp_ctx.w * bmp_ctx.bpp + 31) / 32) * 4;

    int max_draw = fb_w < fb_h ? fb_w * 4 / 5 : fb_h * 4 / 5;
    bmp_ctx.draw_w = bmp_ctx.w < max_draw ? bmp_ctx.w : max_draw;
    bmp_ctx.draw_h = bmp_ctx.h < max_draw ? bmp_ctx.h : max_draw;
    bmp_ctx.scale_x = (bmp_ctx.draw_w << 16) / bmp_ctx.w;
    bmp_ctx.scale_y = (bmp_ctx.draw_h << 16) / bmp_ctx.h;

    int win_w = bmp_ctx.draw_w + 24;
    int win_h = bmp_ctx.draw_h + 48;
    int wi = gui_wnew(name, (fb_w - win_w) / 2, (fb_h - win_h) / 2, win_w, win_h);
    bmp_ctx.valid = 1;
    wins[wi].draw = draw;
    wins[wi].on_close = on_close;
}
