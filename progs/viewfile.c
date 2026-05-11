#include "../lunarsnow.h"

typedef struct { char name[64]; uint8_t *data; uint32_t size; } VF;

static VF vf_pool[MAX_W];
static int vf_n;

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

    if (!v->data) {
        v->data = file_read(v->name, &v->size);
        if (!v->data) {
            fb_txt(wx, wy, "Error: file not found", 0xFF4444, w->bg);
            return;
        }
        int longest = max_line_len(v->data, v->size);
        int nw = longest * 8 + 36;
        if (nw < 160) nw = 160;
        if (nw > fb_w - 60) nw = fb_w - 60;
        if (nw != w->w) {
            w->w = nw;
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
    uint32_t fsize;
    uint8_t *fdata = file_read(name, &fsize);
    int longest = fdata ? max_line_len(fdata, fsize) : 40;
    int w = longest * 8 + 36;
    if (w < 160) w = 160;
    if (w > fb_w - 60) w = fb_w - 60;

    int x = (fb_w - w) / 2 + (vf_n % 4) * 20;
    int y = 60 + (vf_n % 4) * 20;
    if (x + w > fb_w - 10) x = fb_w - w - 10;
    if (x < 10) x = 10;
    vf_n++;

    int wi = gui_wnew(name, x, y, w, 400);
    VF *v = &vf_pool[vf_n % MAX_W];
    wins[wi].userdata = v;
    s_cpy(v->name, name, sizeof(v->name));
    v->data = 0;
    v->size = 0;

    gui_wbtn(wi, "Close", w / 2 - 30, 340, 60, 26, app_close);
    wins[wi].draw = draw;
}
