#include "lunarsnow.h"
#include "fs.h"
#include "bmp.h"

#define P_W 120
#define P_H 90
#define P_S 4
#define P_NC 12

static uint8_t p_canvas[P_W * P_H];
static uint32_t p_pal[P_NC] = {
    0x141428, 0xFFFFFF, 0xFF4444, 0x44FF44,
    0x4444FF, 0xFFFF44, 0xFF44FF, 0x44FFFF,
    0x884400, 0x888888, 0x000000, 0xFF8800
};
static int p_cur = 1, p_win, p_lx = -1, p_ly = -1;

static void p_line(int x0, int y0, int x1, int y1, int ci)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    while (1) {
        if (x0 >= 0 && x0 < P_W && y0 >= 0 && y0 < P_H)
            p_canvas[y0 * P_W + x0] = ci;
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

static void p_clear(void)
{
    for (int i = 0; i < P_W * P_H; i++) p_canvas[i] = 0;
}

static uint8_t p_bmp_buf[BMP_HDR + BMP_ROW(120) * P_H];

static void p_write_bmp(const char *name)
{
    uint32_t img_size = BMP_ROW(120) * P_H;
    uint32_t file_size = BMP_HDR + img_size;
    uint8_t *bmp = p_bmp_buf;
    uint32_t i;
    for (i = 0; i < file_size; i++) bmp[i] = 0;

    bmp[0] = 'B'; bmp[1] = 'M';
    bmp[2] = file_size; bmp[3] = file_size >> 8;
    bmp[4] = file_size >> 16; bmp[5] = file_size >> 24;
    bmp[10] = BMP_HDR;

    bmp[14] = 40;
    bmp[18] = P_W; bmp[19] = P_W >> 8;
    bmp[20] = P_W >> 16; bmp[21] = P_W >> 24;
    bmp[22] = P_H; bmp[23] = P_H >> 8;
    bmp[24] = P_H >> 16; bmp[25] = P_H >> 24;
    bmp[26] = 1;
    bmp[28] = 24;

    for (int y = 0; y < P_H; y++) {
        int sy = P_H - 1 - y;
        int off = BMP_HDR + sy * BMP_ROW(120);
        for (int x = 0; x < P_W; x++) {
            uint32_t col = p_pal[p_canvas[y * P_W + x]];
            bmp[off + x * BMP_BPP + 0] = col;
            bmp[off + x * BMP_BPP + 1] = col >> 8;
            bmp[off + x * BMP_BPP + 2] = col >> 16;
        }
    }

    if (snowfs_mounted)
        snowfs_write(name, bmp, file_size);
    else
        msgbox("Error", "SnowFS not mounted");
}

static int p_read_bmp(const char *name)
{
    uint32_t size = sizeof(p_bmp_buf);
    if (snowfs_read(name, p_bmp_buf, &size) < 0) return -1;
    uint8_t *bmp = p_bmp_buf;
    if (size < 54 || bmp[0] != 'B' || bmp[1] != 'M') return -1;

    int w = bmp[18] | (bmp[19] << 8) | (bmp[20] << 16) | (bmp[21] << 24);
    int h = bmp[22] | (bmp[23] << 8) | (bmp[24] << 16) | (bmp[25] << 24);
    int bpp = bmp[28] | (bmp[29] << 8);
    if (w != P_W || h != P_H || bpp != 24) return -1;

    for (int y = 0; y < P_H; y++) {
        int sy = P_H - 1 - y;
        int off = BMP_HDR + sy * BMP_ROW(120);
        for (int x = 0; x < P_W; x++) {
            uint32_t r = bmp[off + x * BMP_BPP + 2];
            uint32_t g = bmp[off + x * BMP_BPP + 1];
            uint32_t b = bmp[off + x * BMP_BPP + 0];
            uint32_t col = (r << 16) | (g << 8) | b;
            int best = 0, best_d = 0x7FFFFFFF;
            for (int pi = 0; pi < P_NC; pi++) {
                int dr = (int)((col >> 16) & 0xFF) - (int)((p_pal[pi] >> 16) & 0xFF);
                int dg = (int)((col >> 8) & 0xFF) - (int)((p_pal[pi] >> 8) & 0xFF);
                int db = (int)(col & 0xFF) - (int)(p_pal[pi] & 0xFF);
                int d = dr * dr + dg * dg + db * db;
                if (d < best_d) { best_d = d; best = pi; }
            }
            p_canvas[y * P_W + x] = best;
        }
    }
    return 0;
}

static void cb_save(void)
{
    p_write_bmp("PAINT.BMP");
    gui_set_dirty();
}

static void cb_load(void)
{
    p_read_bmp("PAINT.BMP");
    gui_set_dirty();
}

static void p_draw(int wi)
{
    Win *w = &wins[wi];
    int ox = w->x + 8, oy = w->y + 30;
    int cvw = P_W * P_S, cvh = P_H * P_S;

    for (int y = 0; y < P_H; y++)
        for (int x = 0; x < P_W; x++)
            fb_rect(ox + x * P_S, oy + y * P_S, P_S, P_S,
                    p_pal[p_canvas[y * P_W + x]]);

    fb_border(ox - 1, oy - 1, cvw + 2, cvh + 2, C_BDR);

    int py = oy + cvh + 8, ps = 18, gap = 2;
    for (int i = 0; i < P_NC; i++) {
        int px = ox + i * (ps + gap);
        fb_rect(px, py, ps, ps, p_pal[i]);
        if (i == p_cur)
            fb_border(px - 1, py - 1, ps + 2, ps + 2, 0xE6E6F0);
    }

    int bx = ox + P_NC * (ps + gap) + 6;
    fb_rect(bx, py, 50, ps, C_BN);
    fb_border(bx, py, 50, ps, C_BDR);
    fb_txt(bx + 7, py + 1, "Clear", C_BT, C_BN);

    if (snowfs_mounted) {
        bx += 54;
        fb_rect(bx, py, 40, ps, C_BN);
        fb_border(bx, py, 40, ps, C_BDR);
        fb_txt(bx + 5, py + 1, "Save", C_BT, C_BN);

        bx += 44;
        fb_rect(bx, py, 40, ps, C_BN);
        fb_border(bx, py, 40, ps, C_BDR);
        fb_txt(bx + 5, py + 1, "Load", C_BT, C_BN);
    }
}

static void p_tick(void)
{
    if (p_win < 0 || p_win >= nw) { gui_tick = 0; return; }
    if (wins[p_win].draw != p_draw) { gui_tick = 0; return; }
    if (!(mouse_btn & 1)) { p_lx = -1; return; }

    Win *w = &wins[p_win];
    int ox = w->x + 8, oy = w->y + 30;
    int cvw = P_W * P_S, cvh = P_H * P_S;
    int mx = mouse_x, my = mouse_y;

    if (mx >= ox && mx < ox + cvw && my >= oy && my < oy + cvh) {
        int cx = (mx - ox) / P_S, cy = (my - oy) / P_S;
        if (cx >= 0 && cx < P_W && cy >= 0 && cy < P_H) {
            if (p_lx >= 0) p_line(p_lx, p_ly, cx, cy, p_cur);
            else p_canvas[cy * P_W + cx] = p_cur;
            p_lx = cx; p_ly = cy;
            need_render = 1;
        }
        return;
    }
    p_lx = -1;

    int py = oy + cvh + 8, ps = 18;
    if (my >= py && my < py + ps) {
        int bx = ox + P_NC * (ps + 2) + 6;
        if (mx >= bx && mx < bx + 50) { p_clear(); need_render = 1; return; }

        if (snowfs_mounted) {
            bx += 54;
            if (mx >= bx && mx < bx + 40) { cb_save(); need_render = 1; return; }
            bx += 44;
            if (mx >= bx && mx < bx + 40) { cb_load(); need_render = 1; return; }
        }

        int idx = (mx - ox) / (ps + 2);
        if (idx >= 0 && idx < P_NC) { p_cur = idx; need_render = 1; }
    }
}

static void p_close(int wi) { (void)wi; gui_tick = 0; }

void prog_paint(void)
{
    int ww = P_W * P_S + 16, wh = P_H * P_S + 70;
    p_win = gui_wnew("Paint", (fb_w - ww) / 2, (fb_h - wh) / 2, ww, wh);
    p_clear(); p_lx = -1;
    wins[p_win].draw = p_draw;
    wins[p_win].on_close = p_close;
    gui_tick = p_tick;
}
