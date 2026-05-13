#ifndef FB_H
#define FB_H

#include <stdint.h>

void fb_init_ptr(uint32_t *addr, int w, int h, int pch, int bpp);
void fb_pixel(int x, int y, uint32_t c);
void fb_rect(int x, int y, int w, int h, uint32_t c);
void fb_chr(int x, int y, unsigned char ch, uint32_t fg, uint32_t bg);
void fb_txt(int x, int y, const char *s, uint32_t fg, uint32_t bg);
void fb_border(int x, int y, int w, int h, uint32_t c);
void fb_flip(void);
void fb_flip_rect(int x, int y, int w, int h);
void fb_clear(uint32_t c);
uint32_t *fb_get_addr(void);

int  s_len(const char *s);
void s_cpy(char *d, const char *s, int max);
void mcpy(void *d, const void *s, int n);
int  s_cmp(const char *a, const char *b);
void str_int(char *buf, int val);

#define RGB(r,g,b) ((uint32_t)(((r)<<16)|((g)<<8)|(b)))

#define FB_TYPE_VBE 1
#define FB_TYPE_GOP 2

extern int fb_w, fb_h, fb_pch, fb_bpp, fb_type;
extern uint32_t *sbuf;

#endif
