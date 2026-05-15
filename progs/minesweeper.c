#include "lunarsnow.h"

#define S 9
#define C 40
#define WW (S * C + 16)
#define WH (S * C + 70)
#define NM 10

static int b[S][S];
static int win, flag_mode, started;
static int revealed, total_ok;

#define M_MINE 1
#define M_REV 2
#define M_FLG 4

static int rng;

static int rng_n(void)
{
    rng = rng * 1103515245 + 12345;
    return (rng >> 16) & 0x7FFF;
}

static int nums(int x, int y)
{
    int n = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (dx || dy) {
                int nx = x + dx, ny = y + dy;
                if (nx >= 0 && nx < S && ny >= 0 && ny < S && (b[ny][nx] & M_MINE))
                    n++;
            }
    return n;
}

static void reveal(int x, int y)
{
    if (x < 0 || x >= S || y < 0 || y >= S) return;
    if (b[y][x] & (M_REV | M_FLG)) return;
    b[y][x] |= M_REV;
    revealed++;
    if (b[y][x] & M_MINE) return;
    if (nums(x, y) == 0) {
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                if (dx || dy) reveal(x + dx, y + dy);
    }
}

static void init_board(int sx, int sy)
{
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++)
            b[y][x] = 0;
    int placed = 0;
    while (placed < NM) {
        int x = rng_n() % S;
        int y = rng_n() % S;
        if ((b[y][x] & M_MINE) || (x >= sx - 1 && x <= sx + 1 && y >= sy - 1 && y <= sy + 1))
            continue;
        b[y][x] |= M_MINE;
        placed++;
    }
    total_ok = S * S - NM;
    revealed = 0;
}

static void game_over(void)
{
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++)
            b[y][x] |= M_REV;
    need_render = 1;
}

static void check_win(void)
{
    if (revealed >= total_ok) {
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++)
                if (b[y][x] & M_MINE) b[y][x] |= M_FLG;
        need_render = 1;
    }
}

static void draw(int wi)
{
    Win *w = &wins[wi];
    int ox = w->x + 8, oy = w->y + 30;

    for (int y = 0; y < S; y++) {
        for (int x = 0; x < S; x++) {
            int rx = ox + x * C, ry = oy + y * C;
            if (b[y][x] & M_REV) {
                fb_rect(rx, ry, C, C, 0xCCCCCC);
                fb_rect(rx, ry, C, 1, 0xAAAAAA);
                fb_rect(rx, ry, 1, C, 0xAAAAAA);
                if (b[y][x] & M_MINE) {
                    fb_rect(rx + 8, ry + 8, C - 16, C - 16, 0x000000);
                } else {
                    int n = nums(x, y);
                    if (n) {
                        static const char nd[][4] = {"0","1","2","3","4","5","6","7","8"};
                        uint32_t nc[] = {0,0x0000FF,0x008000,0xFF0000,0x000080,0x800000,0x008080,0x000000,0x808080};
                        fb_txt(rx + C/2 - 4, ry + C/2 - 8, nd[n], nc[n], 0xCCCCCC);
                    }
                }
            } else {
                fb_rect(rx, ry, C, C, 0xB0B0B0);
                fb_rect(rx, ry, C, 1, 0xE0E0E0);
                fb_rect(rx, ry, 1, C, 0xE0E0E0);
                fb_rect(rx, ry + C - 1, C, 1, 0x808080);
                fb_rect(rx + C - 1, ry, 1, C, 0x808080);
                if (b[y][x] & M_FLG) {
                    fb_rect(rx + C/2 - 3, ry + C/2 - 3, 6, 6, 0xFF0000);
                }
            }
        }
    }

    char buf[32];
    const char *m = flag_mode ? "[FLAG]" : "[DIG]  ";
    int p = 0;
    while (m[p]) p++;
    fb_txt(w->x + 8, w->y + WH - 20, m, 0xFFCC00, w->bg);

    int mp = p * 8 + 12;
    int left = NM;
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++)
            if (b[y][x] & M_FLG) left--;
    if (left < 0) left = 0;
    p = 0;
    while ("Mines: "[p]) buf[p] = "Mines: "[p], p++;
    str_int(buf + p, left);
    fb_txt(w->x + 8 + mp, w->y + WH - 20, buf, 0xCCCCCC, w->bg);
}

static void click_fn(int wi)
{
    Win *w = &wins[wi];
    win = wi;
    int mx = mouse_x - (w->x + 8);
    int my = mouse_y - (w->y + 30);
    if (mx < 0 || my < 0) return;
    int gx = mx / C, gy = my / C;
    if (gx >= S || gy >= S) return;

    if (flag_mode) {
        if (!(b[gy][gx] & M_REV))
            b[gy][gx] ^= M_FLG;
    } else {
        if (b[gy][gx] & (M_FLG | M_REV)) return;
        if (!started) {
            int h, m, s;
            rtc_read(&h, &m, &s);
            rng = h * 3600 + m * 60 + s;
            init_board(gx, gy);
            started = 1;
        }
        if (b[gy][gx] & M_MINE) {
            game_over();
            return;
        }
        reveal(gx, gy);
        check_win();
    }
    need_render = 1;
}

static void rclick_fn(int wi)
{
    win = wi;
    flag_mode = !flag_mode;
    need_render = 1;
}

static void key_fn(int k)
{
    win = act;
    if (k == 'f' || k == 'F') {
        flag_mode = !flag_mode;
        need_render = 1;
    }
}

void prog_minesweeper(void)
{
    win = gui_wnew("Minesweeper", (fb_w - WW) / 2, (fb_h - WH) / 2, WW, WH);
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++)
            b[y][x] = 0;
    flag_mode = 0; started = 0;
    wins[win].draw = draw;
    wins[win].on_click = click_fn;
    wins[win].on_rclick = rclick_fn;
    wins[win].on_key = key_fn;
}
