#include "../lunarsnow.h"
#include "../fat.h"

/* Viewer always copies file data into a per-window buffer */
typedef struct { char name[64]; uint8_t data[16384]; uint32_t size; } VF;

static VF vf_pool[MAX_W];
static int vf_n;

static int read_any(const char *name, uint8_t *buf, uint32_t *size_out)
{
    uint32_t sz;
    uint8_t *p = file_read(name, &sz);
    if (p) {
        if (sz > 16384) sz = 16384;
        mcpy(buf, p, sz);
        *size_out = sz;
        return 0;
    }
    if (fat_read_file(name, buf, size_out) >= 0)
        return 0;
    return -1;
}

static int max_line_len(const uint8_t *data, uint32_t size)
{
    int max = 0, cur = 0;
    for (uint32_t i = 0; i < size; i++) {
        if (data[i] == '\n') {
            if (cur > max) max = cur;
            cur = 0;
        } else if (data[i] != '\r') {
            cur++;
        }
    }
    if (cur > max) max = cur;
    return max;
}

static void draw(int wi)
{
    Win *w = &wins[wi];
    VF *v = (VF *)w->userdata;
    int wx = w->x + 12, wy = w->y + 28;

    if (!v->size) {
        if (read_any(v->name, v->data, &v->size) < 0) {
            fb_txt(wx, wy, "Error: file not found", 0xFF4444, w->bg);
            return;
        }
        /* Auto-size width to longest line */
        int longest = max_line_len(v->data, v->size);
        int nw = longest * 8 + 36;
        if (nw < 160) nw = 160;
        if (nw > fb_w - 60) nw = fb_w - 60;
        if (nw != w->w) {
            w->w = nw;
            /* Re-center close button */
            wins[wi].btns[0].x = nw / 2 - 30;
            gui_set_dirty();
        }
    }

    char line[128];
    int ci = 0;
    for (uint32_t i = 0; i < v->size && wy < w->y + w->h - 40; i++) {
        char c = v->data[i];
        if (c == '\n' || ci >= 126) {
            line[ci] = 0;
            fb_txt(wx, wy, line, C_LBL, w->bg);
            wy += 16;
            ci = 0;
        } else if (c != '\r') {
            line[ci++] = c;
        }
    }
    if (ci > 0) {
        line[ci] = 0;
        fb_txt(wx, wy, line, C_LBL, w->bg);
    }
}

void prog_viewfile(const char *name)
{
    /* Pre-read to get width */
    static uint8_t tmp[16384];
    uint32_t sz;
    int has_file = (read_any(name, tmp, &sz) == 0);

    int longest = has_file ? max_line_len(tmp, sz) : 40;
    int w = longest * 8 + 36;
    if (w < 160) w = 160;
    if (w > fb_w - 60) w = fb_w - 60;

    int x = (fb_w - w) / 2 + (vf_n % 4) * 20;
    int y = 60 + (vf_n % 4) * 20;
    if (x + w > fb_w - 10) x = fb_w - w - 10;
    if (x < 10) x = 10;
    vf_n = (vf_n + 1) % MAX_W;

    int wi = gui_wnew(name, x, y, w, 400);
    VF *v = &vf_pool[vf_n];
    wins[wi].userdata = v;
    s_cpy(v->name, name, sizeof(v->name));
    v->size = 0;

    /* If we pre-read, store data now */
    if (has_file) {
        mcpy(v->data, tmp, sz);
        v->size = sz;
    }

    gui_wbtn(wi, "Close", w / 2 - 30, 340, 60, 26, app_close);
    wins[wi].draw = draw;
}
