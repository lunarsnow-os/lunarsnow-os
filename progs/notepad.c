#include "lunarsnow.h"
#include "fs.h"

#define NP_COLS 72
#define NP_ROWS 256
static char np_buf[NP_ROWS][NP_COLS + 1];
static int np_cr, np_cc, np_scroll;
static char np_fname[64];
static int np_has_file, np_modified;
static int np_win;

static uint8_t np_tmp[NP_ROWS * (NP_COLS + 1)];

static void np_load(const char *name)
{
    uint32_t sz = sizeof(np_tmp);
    s_cpy(np_fname, name, 64);
    np_has_file = 0;

    uint8_t *p = file_read(name, &sz);
    if (p) {
        if (sz > sizeof(np_tmp) - 1) sz = sizeof(np_tmp) - 1;
        for (uint32_t i = 0; i < sz; i++) np_tmp[i] = p[i];
        np_has_file = 1;
    } else if (snowfs_mounted && snowfs_read(name, np_tmp, &sz) >= 0) {
        np_has_file = 1;
    } else if (fat_read_file(name, np_tmp, &sz) >= 0) {
        np_has_file = 1;
    }

    if (!np_has_file) return;

    if (sz > sizeof(np_tmp)) sz = sizeof(np_tmp);
    int r = 0, c = 0;
    for (uint32_t i = 0; i < sz && r < NP_ROWS; i++) {
        char ch = np_tmp[i];
        if (ch == '\n') {
            np_buf[r][c] = 0;
            r++; c = 0;
        } else if (ch >= 32 && c < NP_COLS) {
            np_buf[r][c++] = ch;
        }
    }
    if (c > 0 || r < NP_ROWS) np_buf[r][c] = 0;
    if (r == 0 && c == 0 && sz == 0) np_buf[0][0] = 0;
    np_cr = r; np_cc = c;
    np_scroll = 0;
    np_modified = 0;
}

static void np_save(void)
{
    if (!np_has_file || np_fname[0] == 0) return;

    int last = NP_ROWS - 1;
    while (last > 0 && np_buf[last][0] == 0) last--;

    uint32_t pos = 0;
    for (int r = 0; r <= last && pos < sizeof(np_tmp); r++) {
        int i = 0;
        while (np_buf[r][i] && pos < sizeof(np_tmp))
            np_tmp[pos++] = np_buf[r][i++];
        if (r < last && pos < sizeof(np_tmp))
            np_tmp[pos++] = '\n';
    }

    if (snowfs_mounted)
        snowfs_write(np_fname, np_tmp, pos);
    else
        fat_write_file(np_fname, np_tmp, pos);
    np_modified = 0;
}

static void cb_save(void) { np_save(); }

static int np_line_len(int r)
{
    int i = 0;
    while (np_buf[r][i]) i++;
    return i;
}

static void np_scroll_to_cursor(int max_rows)
{
    if (np_cr < np_scroll)
        np_scroll = np_cr;
    else if (np_cr >= np_scroll + max_rows)
        np_scroll = np_cr - max_rows + 1;
}

static void key(int k)
{
    int mxr = NP_ROWS - 1;
    int len;

    if (k >= 32 && k <= 126) {
        if ((k == 's' || k == 'S') && np_fname[0] && np_has_file) {
            np_save(); gui_set_dirty(); return;
        }
        if (np_cc < NP_COLS) {
            int i;
            for (i = NP_COLS; i > np_cc; i--)
                np_buf[np_cr][i] = np_buf[np_cr][i - 1];
            np_buf[np_cr][np_cc++] = k;
        } else {
            np_buf[np_cr][NP_COLS] = 0;
            np_cr++; np_cc = 0;
            if (np_cr < NP_ROWS) {
                np_buf[np_cr][np_cc++] = k;
                np_buf[np_cr][np_cc] = 0;
            }
            if (np_cr >= NP_ROWS) np_cr = mxr;
        }
        np_modified = 1;
    } else if (k == 8) {
        if (np_cc > 0) {
            np_cc--;
            for (int i = np_cc; i < NP_COLS; i++)
                np_buf[np_cr][i] = np_buf[np_cr][i + 1];
        } else if (np_cr > 0) {
            len = np_line_len(np_cr - 1);
            int rest = np_line_len(np_cr);
            int space = NP_COLS - len;
            if (space > rest) space = rest;
            for (int i = 0; i < space; i++)
                np_buf[np_cr - 1][len + i] = np_buf[np_cr][i];
            np_buf[np_cr - 1][len + space] = 0;
            for (int i = np_cr; i < mxr; i++)
                for (int j = 0; j <= NP_COLS; j++)
                    np_buf[i][j] = np_buf[i + 1][j];
            np_buf[mxr][0] = 0;
            np_cr--; np_cc = len + space;
            if (np_cc > NP_COLS) np_cc = NP_COLS;
        }
        np_modified = 1;
    } else if (k == KEY_DEL) {
        len = np_line_len(np_cr);
        if (np_cc < len) {
            for (int i = np_cc; i < NP_COLS; i++)
                np_buf[np_cr][i] = np_buf[np_cr][i + 1];
        } else if (np_cr < mxr) {
            int nl = np_line_len(np_cr + 1);
            int space = NP_COLS - len;
            if (space > nl) space = nl;
            for (int i = 0; i < space; i++)
                np_buf[np_cr][len + i] = np_buf[np_cr + 1][i];
            np_buf[np_cr][len + space] = 0;
            for (int i = np_cr + 1; i < mxr; i++)
                for (int j = 0; j <= NP_COLS; j++)
                    np_buf[i][j] = np_buf[i + 1][j];
            np_buf[mxr][0] = 0;
        }
        np_modified = 1;
    } else if (k == '\n') {
        if (np_cr < mxr) {
            int rest = np_line_len(np_cr) - np_cc;
            if (rest < 0) rest = 0;
            if (rest > NP_COLS) rest = NP_COLS;
            for (int i = mxr - 1; i > np_cr; i--)
                for (int j = 0; j <= NP_COLS; j++)
                    np_buf[i][j] = np_buf[i - 1][j];
            np_buf[np_cr + 1][rest] = 0;
            for (int i = 0; i < rest; i++)
                np_buf[np_cr + 1][i] = np_buf[np_cr][np_cc + i];
            np_buf[np_cr][np_cc] = 0;
            np_cr++; np_cc = 0;
        }
        np_modified = 1;
    } else if (k == KEY_UP) {
        if (np_cr > 0) { np_cr--;
            len = np_line_len(np_cr);
            if (np_cc > len) np_cc = len;
        }
    } else if (k == KEY_DOWN) {
        if (np_cr < mxr) { np_cr++;
            len = np_line_len(np_cr);
            if (np_cc > len) np_cc = len;
        }
    } else if (k == KEY_LEFT) {
        if (np_cc > 0) np_cc--;
        else if (np_cr > 0) { np_cr--; np_cc = np_line_len(np_cr); }
    } else if (k == KEY_RIGHT) {
        len = np_line_len(np_cr);
        if (np_cc < len) np_cc++;
        else if (np_cr < mxr) { np_cr++; np_cc = 0; }
    } else if (k == KEY_HOME) {
        np_cc = 0;
    } else if (k == KEY_END) {
        np_cc = np_line_len(np_cr);
    } else if (k == KEY_PGUP) {
        Win *w = &wins[np_win];
        int max_rows = (w->h - 50) / 16;
        if (max_rows < 1) max_rows = 1;
        np_cr -= max_rows;
        if (np_cr < 0) np_cr = 0;
        len = np_line_len(np_cr);
        if (np_cc > len) np_cc = len;
    } else if (k == KEY_PGDN) {
        Win *w = &wins[np_win];
        int max_rows = (w->h - 50) / 16;
        if (max_rows < 1) max_rows = 1;
        np_cr += max_rows;
        if (np_cr > mxr) np_cr = mxr;
        len = np_line_len(np_cr);
        if (np_cc > len) np_cc = len;
    }
    gui_set_dirty();
}

static void draw(int wi)
{
    Win *w = &wins[wi];
    int bx = w->x + 2, by = w->y + 20;
    int bw = w->w - 4, bh = w->h - 42;
    uint32_t bg = 0x141428, fg = C_TTT;

    fb_rect(bx, by, bw, bh, bg);

    int pad = 4;
    int tx = bx + pad;
    int max_rows = (bh - pad * 2) / 16;
    if (max_rows < 1) max_rows = 1;

    int total = NP_ROWS;
    while (total > 0 && np_buf[total - 1][0] == 0) total--;
    if (total == 0) total = 1;

    np_scroll_to_cursor(max_rows);

    int dy = by + pad;
    for (int r = np_scroll; r < np_scroll + max_rows && r < NP_ROWS; r++) {
        fb_txt(tx, dy, np_buf[r], fg, bg);
        if (r == np_cr) {
            int cx = tx + np_cc * 8;
            fb_rect(cx, dy, 2, 16, C_TAC);
        }
        dy += 16;
    }

    /* scrollbar */
    if (total > max_rows) {
        int sbx = bx + bw - 8, sby = by + 2, sbh = bh - 4;
        fb_rect(sbx, sby, 6, sbh, 0x1A1A30);
        int thumb_h = sbh * max_rows / total;
        if (thumb_h < 8) thumb_h = 8;
        if (thumb_h > sbh) thumb_h = sbh;
        int thumb_y = sby + (sbh - thumb_h) * np_scroll / (total - max_rows);
        fb_rect(sbx, thumb_y, 6, thumb_h, 0x3C3C60);
    }

    /* status bar */
    int sby = by + bh;
    fb_rect(bx, sby, bw, 18, 0x1E1E32);
    char st[80];
    int p = 0;
    if (np_fname[0]) {
        int i = 0;
        while (np_fname[i] && p < 76) st[p++] = np_fname[i++];
    } else {
        const char *u = "(unnamed)";
        while (*u && p < 76) st[p++] = *u++;
    }
    if (np_modified) {
        if (p < 78) { st[p++] = ' '; st[p++] = '*'; }
    }
    st[p] = 0;
    fb_txt(bx + 4, sby + 1, st, C_LBL, 0x1E1E32);

    /* line:col */
    char lc[16];
    int lp = 0;
    int tr = np_cr + 1, tc = np_cc + 1;
    if (tr >= 1000) lc[lp++] = '0' + (tr / 1000) % 10;
    if (tr >= 100) lc[lp++] = '0' + (tr / 100) % 10;
    lc[lp++] = '0' + (tr / 10) % 10;
    lc[lp++] = '0' + tr % 10;
    lc[lp++] = ':';
    if (tc >= 100) lc[lp++] = '0' + (tc / 100) % 10;
    if (tc >= 10) lc[lp++] = '0' + (tc / 10) % 10;
    lc[lp++] = '0' + tc % 10;
    lc[lp] = 0;
    fb_txt(bx + bw - 8 * lp - 4, sby + 1, lc, 0x5A7A8A, 0x1E1E32);
}

void prog_notepad(void)
{
    np_has_file = 0;
    np_fname[0] = 0;
    np_cr = 0; np_cc = 0; np_scroll = 0;
    np_modified = 0;
    np_buf[0][0] = 0;

    np_win = gui_wnew("Notepad", 150, 80, 640, 480);
    gui_wbtn(np_win, "Save", 580, 454, 50, 20, cb_save);
    wins[np_win].on_key = key;
    wins[np_win].draw = draw;
}

void prog_notepad_open(const char *name)
{
    np_cr = 0; np_cc = 0; np_scroll = 0;
    np_modified = 0;
    for (int r = 0; r < NP_ROWS; r++) np_buf[r][0] = 0;
    np_load(name);

    char title[80];
    int ti = 0;
    const char *pre = "Notepad - ";
    while (*pre && ti < 78) title[ti++] = *pre++;
    const char *fn = name;
    while (*fn && ti < 78) title[ti++] = *fn++;
    title[ti] = 0;

    np_win = gui_wnew(title, 150, 80, 640, 480);
    gui_wbtn(np_win, "Save", 580, 454, 50, 20, cb_save);
    wins[np_win].on_key = key;
    wins[np_win].draw = draw;
}
