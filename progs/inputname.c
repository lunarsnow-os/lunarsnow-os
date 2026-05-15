/* Input Name — digita o nome e mostra "Hello, [nome]!" */
#include "lunarsnow.h"

static char name[32];
static int name_len;

static void draw(int wi)
{
    Win *w = &wins[wi];
    int x = w->x + 10, y = w->y + 24;

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
}

static void key(int k)
{
    if (k == '\n' && name_len > 0) { app_close(); return; }
    if (k == 8 && name_len > 0) name_len--;
    if (k >= 32 && k <= 126 && name_len < 30) name[name_len++] = k;
    name[name_len] = 0;
}

void prog_inputname(void)
{
    name_len = 0; name[0] = 0;
    int wi = gui_wnew("Input Name", 200, 160, 260, 140);
    wins[wi].on_key = key;
    wins[wi].draw = draw;
}
