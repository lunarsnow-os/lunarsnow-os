#ifndef DISK_H
#define DISK_H

#include <stdint.h>

int disk_read(uint32_t lba, int count, uint16_t *buf);

#endif
