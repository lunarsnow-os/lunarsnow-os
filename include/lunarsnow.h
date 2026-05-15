#ifndef LUNARSNOW_H
#define LUNARSNOW_H

#include "fb.h"
#include "input.h"
#include "gui.h"
#include "apps.h"
#include "progs.h"

void rtc_read(int *h, int *m, int *s);
void rtc_read_date(int *d, int *m, int *y);
uint8_t *file_read(const char *name, uint32_t *size_out);
void file_iterate(void (*cb)(const char *name, uint32_t size));
int vbe_available(void);
int vbe_set_mode(int w, int h, int bpp);
int vbe_try_set_mode(int w, int h, int bpp);

/* Kernel globals */
extern uint64_t total_ram;
extern char cpu_vendor[16];
extern char cpu_brand[64];
extern int boot_sec_total;
extern int snowfs_mounted;
extern int cpu_ok;
extern uint8_t *initrd_start;
extern uint32_t initrd_size;

#endif
