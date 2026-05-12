#include <stdint.h>

static inline void outb(uint16_t p, uint8_t v)
{ asm volatile("outb %0, %1" : : "a"(v), "Nd"(p)); }

static inline uint8_t inb(uint16_t p)
{ uint8_t v; asm volatile("inb %1, %0" : "=a"(v) : "Nd"(p)); return v; }

static inline void outw(uint16_t p, uint16_t v)
{ asm volatile("outw %0, %1" : : "a"(v), "Nd"(p)); }

static inline uint16_t inw(uint16_t p)
{ uint16_t v; asm volatile("inw %1, %0" : "=a"(v) : "Nd"(p)); return v; }

#define ATA_DATA    0
#define ATA_SEC_CNT 2
#define ATA_LBA_LO  3
#define ATA_LBA_MI  4
#define ATA_LBA_HI  5
#define ATA_DRV     6
#define ATA_CMD     7

#define ATA_TIMEOUT 5000000

static int ata_poll(uint16_t base, int wait_drq)
{
    for (int t = 0; t < ATA_TIMEOUT; t++) {
        uint8_t st = inb(base + ATA_CMD);
        if (st & 0x01) return -1;
        if (!(st & 0x80)) {
            if (!wait_drq || (st & 0x08)) return 0;
        }
    }
    return -1;
}

/* Cached detected drive: channel and LBA48-capable flags */
static int disk_chan = -1;

static int disk_detect(void)
{
    for (int chan = 0; chan < 2; chan++) {
        uint16_t base = (chan == 0) ? 0x1F0 : 0x170;
        if (inb(base + ATA_CMD) == 0xFF) continue;

        outb(base + ATA_DRV, 0xE0);
        ata_poll(base, 0);

        outb(base + ATA_SEC_CNT, 0);
        outb(base + ATA_LBA_LO, 0);
        outb(base + ATA_LBA_MI, 0);
        outb(base + ATA_LBA_HI, 0);
        outb(base + ATA_CMD, 0xEC);

        if (ata_poll(base, 0) < 0) continue;

        /* Must read IDENTIFY data (256 words) even if we don't use it */
        for (int i = 0; i < 256; i++) inw(base + ATA_DATA);

        disk_chan = chan;
        return 0;
    }
    return -1;
}

int disk_read(uint32_t lba, int count, uint16_t *buf)
{
    if (disk_chan < 0 && disk_detect() < 0) return -1;

    uint16_t base = (disk_chan == 0) ? 0x1F0 : 0x170;

    if (ata_poll(base, 0) < 0) return -1;

    outb(base + ATA_DRV, 0xE0 | ((lba >> 24) & 0x0F));
    outb(base + ATA_SEC_CNT, count);
    outb(base + ATA_LBA_LO, lba & 0xFF);
    outb(base + ATA_LBA_MI, (lba >> 8) & 0xFF);
    outb(base + ATA_LBA_HI, (lba >> 16) & 0xFF);
    outb(base + ATA_CMD, 0x20);

    for (int s = 0; s < count; s++) {
        if (ata_poll(base, 1) < 0) return -1;
        for (int i = 0; i < 256; i++)
            buf[s * 256 + i] = inw(base + ATA_DATA);
    }
    return 0;
}
