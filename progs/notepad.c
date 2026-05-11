#include "../lunarsnow.h"

#define NP_COLS 72
#define NP_ROWS 256
static char np_buf[NP_ROWS][NP_COLS + 1];
static int np_cr, np_cc;

static void key(int k)
{
    if (k >= 32 && k <= 126) {
        if (np_cc < NP_COLS) {
            np_buf[np_cr][np_cc++] = k;
            np_buf[np_cr][np_cc] = 0;
        } else {
            np_buf[np_cr][NP_COLS] = 0;
            np_cr++; np_cc = 0;
            if (np_cr < NP_ROWS) {
                np_buf[np_cr][np_cc++] = k;
                np_buf[np_cr][np_cc] = 0;
            }
            if (np_cr >= NP_ROWS) np_cr = NP_ROWS - 1;
        }
    } else if (k == 8) {
        if (np_cc > 0) {
            np_cc--; np_buf[np_cr][np_cc] = 0;
        } else if (np_cr > 0) {
            np_buf[np_cr][0] = 0;
            np_cr--; np_cc = s_len(np_buf[np_cr]);
        }
    } else if (k == '\n') {
        np_buf[np_cr][np_cc] = 0;
        np_cr++; np_cc = 0;
        if (np_cr >= NP_ROWS) np_cr = NP_ROWS - 1;
    }
}

static void draw(int wi)
{
    Win *w = &wins[wi];
    int wx = w->x + 4, wy = w->y + 22;
    int ww = w->w - 8, wh = w->h - 26;
    fb_rect(wx, wy, ww, wh, 0xFFFFFF);
    int max_rows = wh / 16;
    int start = np_cr >= max_rows ? np_cr - max_rows + 1 : 0;
    int dy = wy;
    for (int r = start; r <= np_cr && dy < wy + wh; r++) {
        fb_txt(wx + 4, dy, np_buf[r], 0x000000, 0xFFFFFF);
        dy += 16;
    }
    if (dy < wy + wh && np_cr - start < max_rows) {
        int cx = wx + 4 + np_cc * 8;
        fb_rect(cx, dy - 16, 2, 16, 0x000000);
    }
}

void prog_notepad(void)
{
    int wi = gui_wnew("Notepad", 150, 100, 600, 450);
    wins[wi].on_key = key;
    wins[wi].draw = draw;
}
