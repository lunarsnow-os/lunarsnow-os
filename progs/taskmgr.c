#include "lunarsnow.h"
#include "fs.h"

static int wi;

static void taskmgr_click(int wi2) {
    if (wi2 < 0 || wi2 >= nw) return;
    Win *w = &wins[wi2];
    int x = mouse_x, y = mouse_y;
    int wx = w->x, wy = w->y;
    int by = wy + 130;

    for (int i = 0; i < nw; i++) {
        if (i == wi2) continue;
        int ry = by + i * 26;
        if (x >= wx + 180 && x < wx + 240 && y >= ry && y < ry + 22) {
            gui_wclose(i);
            need_render = 1;
            return;
        }
    }
}

static void draw_fn(int wi2) {
    Win *w = &wins[wi2];
    int x = w->x + 6, y = w->y + 24;

    fb_txt(x, y, "System Information", C_TAC, w->bg);
    y += 20;

    fb_txt(x, y, "CPU:", C_LBL, w->bg);
    if (cpu_brand[0])
        fb_txt(x + 40, y, cpu_brand, C_TTT, w->bg);
    else
        fb_txt(x + 40, y, cpu_vendor[0] ? cpu_vendor : "N/A", C_TTT, w->bg);
    y += 16;

    fb_txt(x, y, "RAM:", C_LBL, w->bg);
    char buf[32];
    uint64_t mb = total_ram / (1024 * 1024);
    int pi = 0;
    const char *ps = " MB total";
    if (mb >= 1000) { buf[pi++] = '0' + (mb / 1000) % 10; }
    if (mb >= 100)  { buf[pi++] = '0' + (mb / 100) % 10; }
    if (mb >= 10)   { buf[pi++] = '0' + (mb / 10) % 10; }
    buf[pi++] = '0' + mb % 10;
    while (*ps) buf[pi++] = *ps++;
    buf[pi] = 0;
    fb_txt(x + 40, y, buf, C_TTT, w->bg);
    y += 16;

    int h, m, s;
    rtc_read(&h, &m, &s);
    int now = h * 3600 + m * 60 + s;
    if (now < boot_sec_total) now += 86400;
    int up = now - boot_sec_total;
    int uh = up / 3600;
    int um = (up % 3600) / 60;
    int us = up % 60;
    fb_txt(x, y, "Uptime:", C_LBL, w->bg);
    pi = 0;
    buf[pi++] = '0' + uh / 10; buf[pi++] = '0' + uh % 10;
    buf[pi++] = 'h'; buf[pi++] = ' ';
    buf[pi++] = '0' + um / 10; buf[pi++] = '0' + um % 10;
    buf[pi++] = 'm'; buf[pi++] = ' ';
    buf[pi++] = '0' + us / 10; buf[pi++] = '0' + us % 10;
    buf[pi++] = 's'; buf[pi] = 0;
    fb_txt(x + 60, y, buf, C_TTT, w->bg);
    y += 16;

    uint64_t sectors;
    int lba48;
    if (disk_get_info(0, &sectors, &lba48) == 0) {
        fb_txt(x, y, "Disk:", C_LBL, w->bg);
        pi = 0;
        uint64_t gb = sectors * 512 / (1024 * 1024 * 1024);
        if (gb >= 100) { buf[pi++] = '0' + (gb / 100) % 10; }
        if (gb >= 10)  { buf[pi++] = '0' + (gb / 10) % 10; }
        buf[pi++] = '0' + gb % 10;
        buf[pi++] = 'G'; buf[pi++] = 0;
        fb_txt(x + 40, y, buf, C_TTT, w->bg);
        if (lba48) fb_txt(x + 75, y, "LBA48", C_TAC, w->bg);
        y += 16;
    }

    y += 8;
    fb_txt(x, y, "Running Apps:", C_TAC, w->bg);
    y += 20;
    fb_rect(x, y, 280, 1, 0x3C3C60);
    y += 8;

    int has = 0;
    for (int i = nw - 1; i >= 0; i--) {
        if (i == wi2) continue;
        has = 1;
        uint32_t row = (i == act) ? 0x282848 : w->bg;
        fb_rect(x, y, 280, 24, row);
        fb_txt(x + 6, y + 4, wins[i].title, C_TTT, row);

        fb_rect(x + 180, y + 2, 60, 20, C_CLS);
        fb_txt(x + 186, y + 5, "Close", C_TTT, C_CLS);
        y += 26;
    }

    if (!has)
        fb_txt(x, y, "(none)", C_LBL, w->bg);
}

void prog_taskmgr(void) {
    wi = gui_wnew("Task Manager", 80, 40, 340, 420);
    wins[wi].draw = draw_fn;
    wins[wi].on_click = taskmgr_click;
    close_on_esc = 1;
    need_render = 1;
}
