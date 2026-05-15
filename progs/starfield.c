#include "lunarsnow.h"
#include "config.h"

#define NUM_STARS 400

static int sx[NUM_STARS], sy[NUM_STARS], sz[NUM_STARS];
static int tick_div, tick_cnt;
static int ticks_this_sec;
static int last_h, last_m, last_s;

static unsigned int rng_state = 1;

static unsigned int rand_val(void)
{
    rng_state = rng_state * 1103515245 + 12345;
    return (rng_state / 65536) % 32768;
}

static void tick_fn(void)
{
    tick_cnt++;
    ticks_this_sec++;

    int h, m, s;
    rtc_read(&h, &m, &s);
    int now = h * 3600 + m * 60 + s;
    int prev = last_h * 3600 + last_m * 60 + last_s;
    if (now != prev) {
        tick_div = ticks_this_sec / 60;
        if (tick_div < 1) tick_div = 1;
        ticks_this_sec = 0;
        last_h = h; last_m = m; last_s = s;
    }

    if (tick_cnt % tick_div != 0) return;

    for (int i = 0; i < NUM_STARS; i++) {
        sz[i] -= 4;
        if (sz[i] < 1) {
            sx[i] = (int)(rand_val() % 1025) - 512;
            sy[i] = (int)(rand_val() % 1025) - 512;
            sz[i] = 512;
        }
    }

    fb_rect(0, 0, fb_w, fb_h, 0x000000);

    int cx = fb_w / 2, cy = fb_h / 2;
    for (int i = 0; i < NUM_STARS; i++) {
        int px = cx + (sx[i] * 200) / sz[i];
        int py = cy + (sy[i] * 200) / sz[i];

        if (px < 0 || px >= fb_w || py < 0 || py >= fb_h) continue;

        uint32_t col;
        int sz2 = 1;
        if (sz[i] > 400)      { col = 0x4466AA; sz2 = 1; }
        else if (sz[i] > 250) { col = 0x7799DD; sz2 = 1; }
        else if (sz[i] > 120) { col = 0xAACCFF; sz2 = 2; }
        else                  { col = 0xFFFFFF;  sz2 = 2; }

        fb_rect(px, py, sz2, sz2, col);
    }

    fb_flip();
}

void prog_starfield(void)
{
    tick_div = 10;
    tick_cnt = 0;
    ticks_this_sec = 0;
    rtc_read(&last_h, &last_m, &last_s);

    for (int i = 0; i < NUM_STARS; i++) {
        sx[i] = (int)(rand_val() % 1025) - 512;
        sy[i] = (int)(rand_val() % 1025) - 512;
        sz[i] = (int)(rand_val() % 512) + 1;
    }

    starfield_active = 1;
    close_on_esc = 1;
    gui_tick = tick_fn;
}
