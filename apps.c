#include <stdint.h>
#include "font8x16.h"
#include "fb.h"
#include "input.h"
#include "gui.h"

static int ccount;

/* forward declarations for callbacks used in term_exec */
void cb_about(void);
void cb_new(void);
void cb_reboot(void);
void msgbox(const char *title, const char *msg);

/* ================================================================
   TERMINAL
   ================================================================ */

#define TERM_COLS 70
#define TERM_BUF 128
static char term_lines[TERM_BUF][TERM_COLS + 1];
static int term_cnt;
static char term_input[TERM_COLS + 1];
static int term_len;

static void term_add(const char *s)
{
    if (term_cnt < TERM_BUF) term_cnt++;
    for (int i = 0; i < term_cnt - 1; i++)
        mcpy(term_lines[i], term_lines[i + 1], TERM_COLS + 1);
    int j;
    for (j = 0; j < TERM_COLS && s[j]; j++) term_lines[term_cnt - 1][j] = s[j];
    while (j <= TERM_COLS) term_lines[term_cnt - 1][j++] = 0;
}

static void term_exec(void)
{
    term_add(term_input);
    const char *p = term_input;
    while (*p == ' ') p++;
    if (*p == 0) { term_input[0] = 0; term_len = 0; return; }

    char cmd[TERM_COLS + 1]; int ci = 0;
    while (*p && *p != ' ' && ci < TERM_COLS) cmd[ci++] = *p++;
    cmd[ci] = 0;
    while (*p == ' ') p++;

    int match(const char *a, const char *b) {
        int i; for (i = 0; a[i] && b[i]; i++) if (a[i] != b[i]) return 0;
        return a[i] == b[i];
    }

    if      (match(cmd, "help")) {
        term_add("  echo <text>  - print text");
        term_add("  about        - open About");
        term_add("  cls          - clear screen");
        term_add("  time         - show time");
        term_add("  ver          - show version");
        term_add("  shutdown     - power off");
        term_add("  newwin       - new window");
        term_add("  help         - this list");
    }
    else if (match(cmd, "echo"))    term_add(p);
    else if (match(cmd, "about"))   cb_about();
    else if (match(cmd, "cls"))     term_cnt = 0;
    else if (match(cmd, "time")) {
        int h, m, s;
        extern void rtc_read(int *h, int *m, int *s);
        rtc_read(&h, &m, &s);
        char buf[16]; int bi = 0;
        buf[bi++] = '0' + h / 10; buf[bi++] = '0' + h % 10;
        buf[bi++] = ':'; buf[bi++] = '0' + m / 10; buf[bi++] = '0' + m % 10;
        buf[bi++] = ':'; buf[bi++] = '0' + s / 10; buf[bi++] = '0' + s % 10;
        buf[bi] = 0; term_add(buf);
    }
    else if (match(cmd, "ver"))     term_add("LunarSnow OS v0.2-alpha x64");
    else if (match(cmd, "shutdown")) run = 0;
    else if (match(cmd, "newwin"))  cb_new();
    else                            term_add("unknown command (try help)");
    term_input[0] = 0; term_len = 0;
}

static void term_key(int key)
{
    if (key >= 32 && key <= 126 && term_len < TERM_COLS) {
        term_input[term_len++] = key;
        term_input[term_len] = 0;
    }
    if (key == 8 && term_len > 0) term_input[--term_len] = 0;
    if (key == '\n') term_exec();
}

static void term_draw(int wi)
{
    Win *w = &wins[wi];
    int wx = w->x + 4, wy = w->y + 22;
    uint32_t bg = 0x000000, fg = 0x00CC00;
    int max_rows = (w->h - 26) / 16;
    if (max_rows < 3) max_rows = 3;
    fb_rect(wx - 2, wy - 2, w->w - 8, w->h - 24, bg);

    int avail = max_rows - 1;
    int start = term_cnt > avail ? term_cnt - avail : 0;
    int dy = wy;
    for (int i = start; i < term_cnt; i++, dy += 16) {
        for (int j = 0; j < TERM_COLS && term_lines[i][j]; j++)
            fb_chr(wx + j * 8, dy, term_lines[i][j], fg, bg);
    }

    int px = wx, py = wy + max_rows * 16 - 16;
    fb_txt(px, py, "> ", 0x00FF00, bg);
    for (int j = 0; j < term_len; j++)
        fb_chr(px + 16 + j * 8, py, term_input[j], fg, bg);
}

/* ================================================================
   CALCULATOR
   ================================================================ */

static int calc_val, calc_cur, calc_op;
static char calc_disp[16];

static void calc_disp_upd(void);

#define CD(n) static void cc##n(void) { if (calc_cur < 9999999) calc_cur = calc_cur * 10 + n; calc_disp_upd(); }
CD(0) CD(1) CD(2) CD(3) CD(4) CD(5) CD(6) CD(7) CD(8) CD(9)

static void calc_disp_upd(void)
{
    if (calc_cur == 0) { calc_disp[0] = '0'; calc_disp[1] = 0; return; }
    int v = calc_cur, p = 14;
    calc_disp[15] = 0;
    while (v && p >= 0) { calc_disp[p--] = '0' + (v % 10); v /= 10; }
    int i = 0;
    while (p + 1 + i <= 14) { calc_disp[i] = calc_disp[p + 1 + i]; i++; }
    calc_disp[i] = 0;
}

static void ccE(void)
{
    if      (calc_op == 1) calc_val += calc_cur;
    else if (calc_op == 2) calc_val -= calc_cur;
    else if (calc_op == 3) calc_val *= calc_cur;
    else if (calc_op == 4 && calc_cur) calc_val /= calc_cur;
    else calc_val = calc_cur;
    calc_cur = calc_val; calc_op = 0; calc_disp_upd();
}

static void ccP(void) { ccE(); calc_val = calc_cur; calc_op = 1; calc_cur = 0; }
static void ccS(void) { ccE(); calc_val = calc_cur; calc_op = 2; calc_cur = 0; }
static void ccM(void) { ccE(); calc_val = calc_cur; calc_op = 3; calc_cur = 0; }
static void ccD(void) { ccE(); calc_val = calc_cur; calc_op = 4; calc_cur = 0; }
static void ccC(void) { calc_val = 0; calc_cur = 0; calc_op = 0; calc_disp_upd(); }

static void calc_draw(int wi)
{
    Win *w = &wins[wi];
    int dx = w->x + 10, dy = w->y + 24;
    fb_rect(dx, dy, 220, 34, 0xFFFFFF);
    fb_border(dx, dy, 220, 34, 0x888888);
    fb_txt(dx + 8, dy + 9, calc_disp[0] ? calc_disp : "0", 0x000000, 0xFFFFFF);
}

/* ================================================================
   CALLBACKS
   ================================================================ */

static void cb_close(void) { if (act >= 0 && act < nw) gui_wclose(act); }
void app_close(void) { cb_close(); }

void cb_new(void)
{
    ccount++;
    char title[16]; int i;
    for (i = 0; "Window"[i]; i++) title[i] = "Window"[i];
    title[i] = '0' + ccount; title[++i] = 0;
    int wi = gui_wnew(title, 80 + (ccount % 6) * 30, 80 + (ccount % 5) * 30, 260, 140);
    gui_wbtn(wi, "Close", 100, 80, 60, 26, cb_close);
}

void cb_term(void)
{
    int wi = gui_wnew("Terminal", 120, 80, 580, 400);
    wins[wi].on_key = term_key;
    wins[wi].draw = term_draw;
}

void cb_calc(void)
{
    int wi = gui_wnew("Calculator", 200, 140, 260, 290);
    wins[wi].bg = 0xD4D4D4;
    int bx = 8;
    gui_wbtn(wi, "7", bx, 50, 52, 34, cc7);
    gui_wbtn(wi, "8", bx+58, 50, 52, 34, cc8);
    gui_wbtn(wi, "9", bx+116, 50, 52, 34, cc9);
    gui_wbtn(wi, "/", bx+174, 50, 52, 34, ccD);
    gui_wbtn(wi, "4", bx, 88, 52, 34, cc4);
    gui_wbtn(wi, "5", bx+58, 88, 52, 34, cc5);
    gui_wbtn(wi, "6", bx+116, 88, 52, 34, cc6);
    gui_wbtn(wi, "*", bx+174, 88, 52, 34, ccM);
    gui_wbtn(wi, "1", bx, 126, 52, 34, cc1);
    gui_wbtn(wi, "2", bx+58, 126, 52, 34, cc2);
    gui_wbtn(wi, "3", bx+116, 126, 52, 34, cc3);
    gui_wbtn(wi, "-", bx+174, 126, 52, 34, ccS);
    gui_wbtn(wi, "C", bx, 164, 52, 34, ccC);
    gui_wbtn(wi, "0", bx+58, 164, 52, 34, cc0);
    gui_wbtn(wi, "=", bx+116, 164, 52, 34, ccE);
    gui_wbtn(wi, "+", bx+174, 164, 52, 34, ccP);
    wins[wi].draw = calc_draw;
}

extern void prog_about(void);
void cb_about(void) { prog_about(); }

void cb_shutdown(void) { run = 0; }
void cb_reboot(void) { run = -1; }

/* ================================================================
   MESSAGE BOX
   ================================================================ */

static char msgbox_msg[128];

static void msgbox_draw(int wi)
{
    Win *w = &wins[wi];
    fb_txt(w->x + 16, w->y + 32, msgbox_msg, C_LBL, w->bg);
}

void msgbox(const char *title, const char *msg)
{
    s_cpy(msgbox_msg, msg, 127);
    int wi = gui_wnew(title, 200, 200, 300, 120);
    gui_wbtn(wi, "OK", 120, 75, 60, 26, cb_close);
    wins[wi].draw = msgbox_draw;
}


