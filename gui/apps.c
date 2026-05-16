#include <stdint.h>
#include "lunarsnow.h"
#include "config.h"

/* forward declarations for callbacks used in term_exec */
static void cb_close(void);
void cb_about(void);
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
    if (term_cnt < TERM_BUF) {
        term_cnt++;
    } else {
        for (int i = 0; i < TERM_BUF - 1; i++)
            mcpy(term_lines[i], term_lines[i + 1], TERM_COLS + 1);
    }
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
        term_add("  reboot       - restart");
        term_add("  exit         - close terminal");
        term_add("  help         - this list");
    }
    else if (match(cmd, "echo"))    term_add(p);
    else if (match(cmd, "about"))   cb_about();
    else if (match(cmd, "cls"))     term_cnt = 0;
    else if (match(cmd, "time")) {
        int h, m, s;
        rtc_read(&h, &m, &s);
        char buf[16]; int bi = 0;
        buf[bi++] = '0' + h / 10; buf[bi++] = '0' + h % 10;
        buf[bi++] = ':'; buf[bi++] = '0' + m / 10; buf[bi++] = '0' + m % 10;
        buf[bi++] = ':'; buf[bi++] = '0' + s / 10; buf[bi++] = '0' + s % 10;
        buf[bi] = 0; term_add(buf);
    }
    else if (match(cmd, "ver"))     term_add(OS_FULL);
    else if (match(cmd, "shutdown")) run = 0;
    else if (match(cmd, "exit"))    cb_close();
    else if (match(cmd, "reboot"))  cb_reboot();
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
    int x = w->x, y = w->y, ww = w->w, hh = w->h;
    uint32_t bg = 0x0A0A0A, fg = 0x33FF33;

    int bx = x + 2, by = y + 20, bw = ww - 4, bh = hh - 22;
    fb_rect(bx, by, bw, bh, bg);

    fb_rect(bx, by, bw, 1, 0x000000);
    fb_rect(bx, by, 1, bh, 0x000000);
    fb_rect(bx, by + bh - 1, bw, 1, 0x3C3C3C);
    fb_rect(bx + bw - 1, by, 1, bh, 0x3C3C3C);

    int pad = 4;
    int tx = bx + pad, ty = by + pad;
    int max_rows = (bh - pad * 2) / 16;
    if (max_rows < 3) max_rows = 3;

    int avail = max_rows - 1;
    int start = term_cnt > avail ? term_cnt - avail : 0;
    int dy = ty;
    for (int i = start; i < term_cnt; i++, dy += 16) {
        for (int j = 0; j < TERM_COLS && term_lines[i][j]; j++)
            fb_chr(tx + j * 8, dy, term_lines[i][j], fg, bg);
    }

    int px = tx, py = ty + max_rows * 16 - 16;
    fb_txt(px, py, "$ ", fg, bg);
    for (int j = 0; j < term_len; j++)
        fb_chr(px + 16 + j * 8, py, term_input[j], fg, bg);

    static int cursor_frame = 0;
    cursor_frame++;
    if ((cursor_frame / 12) % 2) {
        fb_rect(px + 16 + term_len * 8, py, 8, 16, fg);
    }
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

static void calc_key(int k)
{
    if (k >= '0' && k <= '9') {
        static void (*cdigs[10])(void) = {cc0,cc1,cc2,cc3,cc4,cc5,cc6,cc7,cc8,cc9};
        cdigs[k - '0']();
    } else if (k == '+') ccP();
    else if (k == '-') ccS();
    else if (k == '*') ccM();
    else if (k == '/') ccD();
    else if (k == '=' || k == '\n') ccE();
    else if (k == 'c' || k == 'C') ccC();
}

static void calc_draw(int wi)
{
    Win *w = &wins[wi];
    int dx = w->x + 10, dy = w->y + 24;
    fb_rect(dx, dy, 220, 34, w->bg);
    fb_border(dx, dy, 220, 34, 0x3C50A0);
    fb_txt(dx + 8, dy + 9, calc_disp[0] ? calc_disp : "0", C_TTT, w->bg);
}

/* ================================================================
   CALLBACKS
   ================================================================ */

static void cb_close(void) { if (act >= 0 && act < nw) gui_wclose(act); }
void app_close(void) { cb_close(); }

void cb_term(void)
{
    int wi = gui_wnew("Terminal", 120, 80, 580, 400);
    wins[wi].on_key = term_key;
    wins[wi].draw = term_draw;
    term_cnt = 0;
    term_add(OS_FULL);
    term_add("Type 'help' for available commands.");
    term_add("");
}

void cb_calc(void)
{
    int wi = gui_wnew("Calculator", 200, 140, 260, 290);
    wins[wi].on_key = calc_key;
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
   POWER DIALOG — Windows XP-style shutdown / restart
   ================================================================ */

static int pwr_wi;

static void pwr_icon(int x, int y, uint32_t fg) {
    uint32_t bg = C_WBG;
    fb_rect(x, y, 28, 28, bg);
    fb_border(x + 2, y + 2, 24, 24, fg);
    fb_rect(x + 12, y + 8, 4, 14, fg);
    fb_rect(x + 12, y + 2, 4, 8, bg);
}

static void pwr_draw(int wi) {
    Win *w = &wins[wi];
    int x = w->x + 16, y = w->y + 30;
    pwr_icon(x, y, 0x50D070);
    fb_txt(x + 36, y + 2, "What do you want the", C_TTT, w->bg);
    fb_txt(x + 36, y + 18, "computer to do?", C_TTT, w->bg);
}

static void pwr_shut(void) {
    int i;
    for (i = 0; i < nw; i++) gui_wclose(0);
    run = 0;
}
static void pwr_reboot(void) {
    int i;
    for (i = 0; i < nw; i++) gui_wclose(0);
    run = -1;
}

void power_dialog(void) {
    pwr_wi = gui_wnew("Turn off computer", (fb_w - 340) / 2, (fb_h - 180) / 2, 340, 180);
    gui_wbtn(pwr_wi, "Shut Down", 20, 90, 90, 32, pwr_shut);
    gui_wbtn(pwr_wi, "Restart", 125, 90, 90, 32, pwr_reboot);
    gui_wbtn(pwr_wi, "Cancel", 230, 90, 90, 32, app_close);
    gui_set_dirty();
    wins[pwr_wi].draw = pwr_draw;
}

/* ================================================================
   MESSAGE BOX
   ================================================================ */

static char msgbox_msg[128];

static void msgbox_draw(int wi)
{
    Win *w = &wins[wi];
    int l = s_len(msgbox_msg);
    int tx = w->x + (w->w - l * 8) / 2;
    fb_txt(tx, w->y + 35, msgbox_msg, C_LBL, w->bg);
}

void msgbox(const char *title, const char *msg)
{
    s_cpy(msgbox_msg, msg, 127);
    int wi = gui_wnew(title, 200, 200, 300, 110);
    gui_wbtn(wi, "OK", 120, 60, 60, 26, cb_close);
    wins[wi].draw = msgbox_draw;
}


