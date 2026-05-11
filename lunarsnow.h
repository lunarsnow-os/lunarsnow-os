#ifndef LUNARSNOW_H
#define LUNARSNOW_H

#include "fb.h"
#include "input.h"
#include "gui.h"
#include "apps.h"
#include "progs.h"

void rtc_read(int *h, int *m, int *s);
uint8_t *file_read(const char *name, uint32_t *size_out);
void file_iterate(void (*cb)(const char *name, uint32_t size));

#endif
