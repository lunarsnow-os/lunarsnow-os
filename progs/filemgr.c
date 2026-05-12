#include "../lunarsnow.h"
#include "../fat.h"

#define FM_MAX 32
static char fm_names[FM_MAX][64];
static uint32_t fm_sizes[FM_MAX];
static int fm_n, fm_sel;

static int fm_has(const char *name)
{
    for (int i = 0; i < fm_n; i++)
        if (s_cmp(fm_names[i], name) == 0) return 1;
    return 0;
}

static void fm_collect(const char *name, uint32_t size)
{
    if (fm_n < FM_MAX && !fm_has(name)) {
        s_cpy(fm_names[fm_n], name, 64);
        fm_sizes[fm_n] = size;
        fm_n++;
    }
}

static void fm_open_sel(void)
{
    if (fm_n > 0 && fm_sel >= 0 && fm_sel < fm_n)
        prog_viewfile(fm_names[fm_sel]);
}

static void fm_click(int wi)
{
    Win *w = &wins[wi];
    int y = mouse_y;
    int start_y = w->y + 28;
    int max_visible = (w->h - 80) / 16;

    if (y < start_y) return;
    int idx = (y - start_y) / 16;
    if (idx >= 0 && idx < fm_n && idx < max_visible) {
        fm_sel = idx;
        fm_open_sel();
    }
}

static void fm_on_key(int k)
{
    if (k == KEY_UP) {
        if (fm_sel > 0) { fm_sel--; gui_set_dirty(); }
    } else if (k == KEY_DOWN) {
        if (fm_sel < fm_n - 1) { fm_sel++; gui_set_dirty(); }
    } else if (k == '\n') {
        fm_open_sel();
    }
}

static void draw(int wi)
{
    Win *w = &wins[wi];
    int wx = w->x + 12, wy = w->y + 28;
    int max_visible = (w->h - 80) / 16;
    char buf[72];
    int drawn = 0;

    if (fm_n == 0) {
        fb_txt(wx, wy, "No files found", C_LBL, w->bg); return;
    }

    for (int i = 0; i < fm_n && drawn < max_visible; i++) {
        if (i == fm_sel)
            fb_rect(wx, wy, w->w - 24, 16, C_MFOC);

        int pi = 0;
        const char *fn = fm_names[i];
        while (*fn && pi < 40) buf[pi++] = *fn++;
        buf[pi++] = ' ';
        buf[pi++] = '(';
        str_int(buf + pi, (int)fm_sizes[i]);
        while (buf[pi]) pi++;
        buf[pi++] = ' '; buf[pi++] = 'b'; buf[pi++] = ')';
        buf[pi] = 0;

        fb_txt(wx + 4, wy, buf, C_LBL, (i == fm_sel) ? C_MFOC : w->bg);
        wy += 16;
        drawn++;
    }
}

void prog_filemgr(void)
{
    fm_n = 0;
    fm_sel = 0;
    file_iterate(fm_collect);
    fat_iterate(fm_collect);

    int wi = gui_wnew("File Manager", 50, 40, 340, 340);
    wins[wi].draw = draw;
    wins[wi].on_key = fm_on_key;
    wins[wi].on_click = fm_click;
}
