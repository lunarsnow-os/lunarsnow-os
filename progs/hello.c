/* Hello Demo — exemplo de app externa */
#include "../lunarsnow.h"

static void draw(int wi)
{
    fb_txt(wins[wi].x + 10, wins[wi].y + 30, "Hello World! :3", C_TTT, wins[wi].bg);
    fb_txt(wins[wi].x + 10, wins[wi].y + 50, "This is an external app.", C_LBL, wins[wi].bg);
}

void prog_hello(void)
{
    int wi = gui_wnew("Hello Demo", 150, 150, 260, 140);
    gui_wbtn(wi, "OK", 100, 80, 60, 26, app_close);
    wins[wi].draw = draw;
}
