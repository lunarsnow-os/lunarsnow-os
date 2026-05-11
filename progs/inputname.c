/* Input Name — digita o nome e mostra "Hello, [nome]!" */
#include "../lunarsnow.h"

static char name[32];
static int name_len, name_done;

static void draw(int wi)
{
    Win *w = &wins[wi];
    int x = w->x + 10, y = w->y + 28;

    if (!name_done) {
        fb_txt(x, y, "Type your name:", C_TTT, w->bg);
        fb_rect(x, y + 20, 220, 20, 0xFFFFFF);
        fb_border(x, y + 20, 220, 20, 0x888888);
        if (name_len > 0) {
            char buf[48]; int i;
            for (i = 0; i < name_len && i < 24; i++) buf[i] = name[i];
            buf[i] = 0;
            fb_txt(x + 4, y + 22, buf, 0x000000, 0xFFFFFF);
        }
        fb_txt(x, y + 50, "Press Enter when done", 0x888888, w->bg);
    } else {
        char buf[48]; int i;
        buf[0] = 'H'; buf[1] = 'e'; buf[2] = 'l'; buf[3] = 'l'; buf[4] = 'o'; buf[5] = ',';
        buf[6] = ' '; i = 7;
        for (int j = 0; j < name_len && i < 44; j++) buf[i++] = name[j];
        buf[i++] = '!'; buf[i] = 0;
        fb_txt(x, y, buf, C_TTT, w->bg);
    }
}

static void key(int k)
{
    if (name_done) return;
    if (k == '\n') { name_done = 1; return; }
    if (k == 8 && name_len > 0) name_len--;
    if (k >= 32 && k <= 126 && name_len < 30) name[name_len++] = k;
    name[name_len] = 0;
}

void prog_inputname(void)
{
    name_len = 0; name_done = 0; name[0] = 0;
    int wi = gui_wnew("Input Name", 200, 160, 260, 160);
    gui_wbtn(wi, "OK", 100, 100, 60, 26, app_close);
    wins[wi].on_key = key;
    wins[wi].draw = draw;
}
