#ifndef FAT_H
#define FAT_H

#include <stdint.h>

int  fat_mount(void);

void fat_iterate(void (*cb)(const char *name, uint32_t size));
int  fat_read_file(const char *name, uint8_t *buf, uint32_t *size_out);

#endif
