#include <stdint.h>
#include "fs.h"
#include "io.h"
#include "fb.h"

/* ===== disk.c ===== */

#define ATA_DATA    0
#define ATA_SEC_CNT 2
#define ATA_LBA_LO  3
#define ATA_LBA_MI  4
#define ATA_LBA_HI  5
#define ATA_DRV     6
#define ATA_CMD     7

#define ATA_TIMEOUT 5000000

#define MAX_DRV 4

static struct {
    int present;
    int chn;
    int slave;
    int lba48;
    uint64_t sectors;
} drives[MAX_DRV];

static int ndrives;

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

static int identify(int chn, int slave, uint64_t *sectors, int *lba48)
{
    uint16_t base = (chn == 0) ? 0x1F0 : 0x170;
    uint16_t id[256];
    if (inb(base + ATA_CMD) == 0xFF) return -1;

    outb(base + ATA_DRV, 0xE0 | (slave << 4));
    ata_poll(base, 0);

    outb(base + ATA_SEC_CNT, 0);
    outb(base + ATA_LBA_LO, 0);
    outb(base + ATA_LBA_MI, 0);
    outb(base + ATA_LBA_HI, 0);
    outb(base + ATA_CMD, 0xEC);

    if (ata_poll(base, 0) < 0) return -1;

    for (int i = 0; i < 256; i++)
        id[i] = inw(base + ATA_DATA);

    *lba48   = (id[83] & 0x0400) ? 1 : 0;
    *sectors = *(uint32_t *)(id + 100);
    if (*lba48)
        *sectors = *(uint64_t *)(id + 100);
    return 0;
}

int disk_detect_all(void)
{
    ndrives = 0;

    for (int chn = 0; chn < 2 && ndrives < MAX_DRV; chn++) {
        for (int slv = 0; slv < 2 && ndrives < MAX_DRV; slv++) {
            uint64_t sectors = 0;
            int lba48 = 0;
            if (identify(chn, slv, &sectors, &lba48) < 0) continue;

            drives[ndrives].present = 1;
            drives[ndrives].chn     = chn;
            drives[ndrives].slave   = slv;
            drives[ndrives].lba48   = lba48;
            drives[ndrives].sectors = sectors;
            ndrives++;
        }
    }
    return ndrives;
}

int disk_count(void)
{
    return ndrives;
}

int disk_get_info(int drv, uint64_t *sectors, int *lba48)
{
    if (drv < 0 || drv >= ndrives || !drives[drv].present) return -1;
    if (sectors) *sectors = drives[drv].sectors;
    if (lba48)   *lba48   = drives[drv].lba48;
    return 0;
}

int disk_read(int drv, uint64_t lba, int count, void *buf)
{
    if (drv < 0 || drv >= ndrives || !drives[drv].present) return -1;

    uint16_t base = (drives[drv].chn == 0) ? 0x1F0 : 0x170;
    int slave = drives[drv].slave;
    int lba48 = drives[drv].lba48;

    if (ata_poll(base, 0) < 0) return -1;

    if (lba48 && (lba > 0x0FFFFFFF || count > 256)) {
        while (count > 0) {
            int nr = (count > 65535) ? 65535 : count;
            if (disk_read(drv, lba, nr, buf) < 0) return -1;
            buf  = (uint8_t *)buf + nr * 512;
            lba  += nr;
            count -= nr;
        }
        return 0;
    }

    if (lba48) {
        outb(base + ATA_DRV, 0x40 | (slave << 4));
        outb(base + ATA_SEC_CNT, (count >> 8) & 0xFF);
        outb(base + ATA_LBA_LO, (lba >> 24) & 0xFF);
        outb(base + ATA_LBA_MI, (lba >> 32) & 0xFF);
        outb(base + ATA_LBA_HI, (lba >> 40) & 0xFF);
        outb(base + ATA_SEC_CNT, count & 0xFF);
        outb(base + ATA_LBA_LO, lba & 0xFF);
        outb(base + ATA_LBA_MI, (lba >> 8) & 0xFF);
        outb(base + ATA_LBA_HI, (lba >> 16) & 0xFF);
        outb(base + ATA_CMD, 0x24);
    } else {
        if (lba > 0x0FFFFFFF) return -1;
        if (count > 256) count = 256;
        outb(base + ATA_DRV, 0xE0 | (slave << 4) | ((lba >> 24) & 0x0F));
        outb(base + ATA_SEC_CNT, count & 0xFF);
        outb(base + ATA_LBA_LO, lba & 0xFF);
        outb(base + ATA_LBA_MI, (lba >> 8) & 0xFF);
        outb(base + ATA_LBA_HI, (lba >> 16) & 0xFF);
        outb(base + ATA_CMD, 0x20);
    }

    for (int s = 0; s < count; s++) {
        if (ata_poll(base, 1) < 0) return -1;
        for (int i = 0; i < 256; i++)
            ((uint16_t *)buf)[s * 256 + i] = inw(base + ATA_DATA);
    }
    return 0;
}

int disk_write(int drv, uint64_t lba, int count, const void *buf)
{
    if (drv < 0 || drv >= ndrives || !drives[drv].present) return -1;

    uint16_t base = (drives[drv].chn == 0) ? 0x1F0 : 0x170;
    int slave = drives[drv].slave;
    int lba48 = drives[drv].lba48;

    if (ata_poll(base, 0) < 0) return -1;

    if (lba48 && (lba > 0x0FFFFFFF || count > 256)) {
        while (count > 0) {
            int nr = (count > 65535) ? 65535 : count;
            if (disk_write(drv, lba, nr, buf) < 0) return -1;
            buf  = (const uint8_t *)buf + nr * 512;
            lba  += nr;
            count -= nr;
        }
        return 0;
    }

    if (lba48) {
        outb(base + ATA_DRV, 0x40 | (slave << 4));
        outb(base + ATA_SEC_CNT, (count >> 8) & 0xFF);
        outb(base + ATA_LBA_LO, (lba >> 24) & 0xFF);
        outb(base + ATA_LBA_MI, (lba >> 32) & 0xFF);
        outb(base + ATA_LBA_HI, (lba >> 40) & 0xFF);
        outb(base + ATA_SEC_CNT, count & 0xFF);
        outb(base + ATA_LBA_LO, lba & 0xFF);
        outb(base + ATA_LBA_MI, (lba >> 8) & 0xFF);
        outb(base + ATA_LBA_HI, (lba >> 16) & 0xFF);
        outb(base + ATA_CMD, 0x34);
    } else {
        if (lba > 0x0FFFFFFF) return -1;
        if (count > 256) count = 256;
        outb(base + ATA_DRV, 0xE0 | (slave << 4) | ((lba >> 24) & 0x0F));
        outb(base + ATA_SEC_CNT, count & 0xFF);
        outb(base + ATA_LBA_LO, lba & 0xFF);
        outb(base + ATA_LBA_MI, (lba >> 8) & 0xFF);
        outb(base + ATA_LBA_HI, (lba >> 16) & 0xFF);
        outb(base + ATA_CMD, 0x30);
    }

    for (int s = 0; s < count; s++) {
        if (ata_poll(base, 1) < 0) return -1;
        for (int i = 0; i < 256; i++)
            outw(base + ATA_DATA, ((const uint16_t *)buf)[s * 256 + i]);
        if (ata_poll(base, 0) < 0) return -1;
    }

    /* Flush cache */
    outb(base + ATA_CMD, 0xE7);
    ata_poll(base, 0);

    return 0;
}

/* ===== partition.c ===== */

#define SECTOR_SIZE 512

static uint8_t buf[SECTOR_SIZE];

/* GPT partition type GUID for basic data (FAT32/NTFS/exFAT):
   EBD0A0A2-B9E5-4433-87C0-68B6B72699C7 */
static const uint8_t gpt_data_guid[16] = {
    0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44,
    0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7
};

static int guid_cmp(const uint8_t *a, const uint8_t *b)
{
    for (int i = 0; i < 16; i++)
        if (a[i] != b[i]) return -1;
    return 0;
}

static uint64_t rd64(const uint8_t *p)
{
    return (uint64_t)p[0] | ((uint64_t)p[1] << 8)
         | ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24)
         | ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40)
         | ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int part_scan(int drv, Partition *parts, int max)
{
    int n = 0;

    if (disk_read(drv, 0, 1, buf) < 0) return 0;
    if (buf[510] != 0x55 || buf[511] != 0xAA) return 0;

    /* Check for protective MBR (GPT) */
    int is_gpt = 0;
    uint8_t *pte = buf + 446;
    for (int i = 0; i < 4; i++) {
        if (pte[i * 16 + 4] == 0xEE) { is_gpt = 1; break; }
    }

    if (is_gpt) {
        /* Read GPT header at LBA 1 */
        if (disk_read(drv, 1, 1, buf) < 0) return 0;

        /* Verify signature "EFI PART" */
        if (buf[0] != 0x45 || buf[1] != 0x46 || buf[2] != 0x49 ||
            buf[3] != 0x20 || buf[4] != 0x50 || buf[5] != 0x41 ||
            buf[6] != 0x52 || buf[7] != 0x54) return 0;

        uint64_t part_lba  = rd64(buf + 72);
        uint32_t part_num  = rd32(buf + 80);
        uint32_t part_size = rd32(buf + 84);

        if (part_size < 128 || part_num == 0) return 0;
        if (part_size > SECTOR_SIZE) return 0;

        /* Read partition entries */
        for (uint32_t i = 0; i < part_num && n < max; i++) {
            uint32_t entry_off = (i * part_size) & (SECTOR_SIZE - 1);
            uint64_t entry_lba = part_lba + (i * part_size) / SECTOR_SIZE;

            if (entry_off == 0) {
                if (disk_read(drv, entry_lba, 1, buf) < 0) return n;
            }

            uint8_t *e = buf + entry_off;

            /* Check if partition is in use */
            if (e[0] == 0 && e[1] == 0 && e[2] == 0 && e[3] == 0) continue;

            if (guid_cmp(e, gpt_data_guid) == 0) {
                parts[n].start = rd64(e + 32);
                parts[n].size  = rd64(e + 40) - rd64(e + 32) + 1;
                parts[n].type  = PART_GPT_DATA;
                n++;
            }
        }
    } else {
        /* MBR: scan 4 primary entries */
        for (int i = 0; i < 4 && n < max; i++) {
            uint8_t *e = pte + i * 16;
            uint8_t  t = e[4];

            if (t == 0x0B || t == 0x0C) {
                parts[n].start = rd32(e + 8);
                parts[n].size  = rd32(e + 12);
                parts[n].type  = PART_MBR_FAT32;
                n++;
            } else if (t == 0xDA) {
                parts[n].start = rd32(e + 8);
                parts[n].size  = rd32(e + 12);
                parts[n].type  = PART_MBR_SNOWFS;
                n++;
            }
        }
    }

    return n;
}

/* ===== fat.c ===== */

static struct {
    int  mounted;
    int  drv;
    uint32_t part_lba;
    uint32_t fat_start;
    uint32_t fatsz;
    uint32_t nfat;
    uint32_t data_start;
    uint32_t root_clus;
    uint32_t sec_per_clus;
    uint32_t bytes_per_sec;
    uint8_t  buf[512];
} fat;

static uint32_t fat_clus2lba(uint32_t clus)
{
    return fat.data_start + (clus - 2) * fat.sec_per_clus;
}

static uint32_t fat_next_clus(uint32_t clus)
{
    uint32_t fat_sec = fat.fat_start + (clus * 4) / fat.bytes_per_sec;
    uint32_t off     = (clus * 4) % fat.bytes_per_sec;
    if (disk_read(fat.drv, fat_sec, 1, fat.buf) < 0) return 0xFFFFFFFF;
    uint32_t val = *(uint32_t *)(fat.buf + off) & 0x0FFFFFFF;
    return val;
}

static int fat_read_clus(uint32_t clus, uint8_t *buf)
{
    uint32_t lba = fat_clus2lba(clus);
    return disk_read(fat.drv, lba, fat.sec_per_clus, buf);
}

static int sfntostr(char *out, const char *name, int max)
{
    int i, p = 0;
    for (i = 0; i < 8 && name[i] && name[i] != ' '; i++)
        if (p < max - 1) out[p++] = name[i];
    int ext = 0;
    for (i = 8; i < 11 && name[i] && name[i] != ' '; i++) ext = 1;
    if (ext) {
        if (p < max - 1) out[p++] = '.';
        for (i = 8; i < 11 && name[i] && name[i] != ' '; i++)
            if (p < max - 1) out[p++] = name[i];
    }
    out[p] = 0;
    return p;
}

int fat_mount(void)
{
    if (disk_count() < 1) return -1;
    fat.drv = 0;

    Partition parts[4];
    int np = part_scan(fat.drv, parts, 4);
    if (np == 0) return -1;

    uint32_t part_lba = (uint32_t)parts[0].start;

    if (disk_read(fat.drv, part_lba, 1, fat.buf) < 0) return -1;
    if (fat.buf[510] != 0x55 || fat.buf[511] != 0xAA) return -1;

    uint16_t bps  = *(uint16_t *)(fat.buf + 11);
    uint8_t  spc  = fat.buf[13];
    uint16_t rsvd = *(uint16_t *)(fat.buf + 14);
    uint8_t  nfat = fat.buf[16];
    uint32_t fatsz = *(uint32_t *)(fat.buf + 36);
    uint32_t rootc = *(uint32_t *)(fat.buf + 44);

    if (bps != 512 || spc == 0) return -1;

    fat.part_lba      = part_lba;
    fat.bytes_per_sec  = bps;
    fat.sec_per_clus   = spc;
    fat.fat_start      = part_lba + rsvd;
    fat.fatsz          = fatsz;
    fat.nfat           = nfat;
    fat.data_start     = part_lba + rsvd + nfat * fatsz;
    fat.root_clus      = rootc;
    fat.mounted        = 1;
    return 0;
}

void fat_iterate(void (*cb)(const char *name, uint32_t size))
{
    if (!fat.mounted) return;

    uint32_t clus = fat.root_clus;
    static uint8_t buf[512 * 32];
    uint32_t clus_bytes = fat.sec_per_clus * 512;
    static char lfn_buf[512];
    int      lfn_pos = 0;
    int      lfn_n   = 0;

    while (clus < 0x0FFFFFF8) {
        if (fat_read_clus(clus, buf) < 0) return;

        for (uint32_t off = 0; off + 32 <= clus_bytes; off += 32) {
            uint8_t *e = buf + off;
            uint8_t  attr = e[11];

            if (e[0] == 0) return;

            if (attr == 0x0F) {
                uint8_t seq = e[0] & 0x3F;
                if (seq == 0 || seq > 20) continue;
                if (e[0] & 0x40) {
                    lfn_n = seq;
                    lfn_pos = 1;
                    for (int i = 0; i < lfn_n * 13; i++) lfn_buf[i] = 0;
                }
                if (!lfn_n) continue;
                int base = (lfn_n - seq) * 13;
                for (int i = 0; i < 5 && base + i < 511; i++)
                    if (e[1 + i*2]) lfn_buf[base + i] = e[1 + i*2];
                for (int i = 0; i < 6 && base + 5 + i < 511; i++)
                    if (e[14 + i*2]) lfn_buf[base + 5 + i] = e[14 + i*2];
                for (int i = 0; i < 2 && base + 11 + i < 511; i++)
                    if (e[28 + i*2]) lfn_buf[base + 11 + i] = e[28 + i*2];
                continue;
            }

            if (e[0] == 0xE5) { lfn_pos = 0; continue; }
            if (attr & 0x08)  { lfn_pos = 0; continue; }

            uint32_t fsize = *(uint32_t *)(e + 28);
            char fname[256];

            if (lfn_pos) {
                int max = lfn_n * 13;
                if (max > 255) max = 255;
                int i;
                for (i = 0; i < max; i++) {
                    fname[i] = lfn_buf[i];
                    if (!fname[i]) break;
                }
                fname[i] = 0;
                lfn_pos = 0;
            } else {
                sfntostr(fname, (const char *)e, 255);
                if (fname[0] == '.' && (fname[1] == 0 || (fname[1] == '.' && fname[2] == 0)))
                    continue;
            }

            if (attr & 0x10) continue;

            cb(fname, fsize);
        }

        clus = fat_next_clus(clus);
    }
}

int fat_read_file(const char *name, uint8_t *buf, uint32_t *size_out)
{
    if (!fat.mounted) return -1;

    uint32_t clus = fat.root_clus;
    static uint8_t tmp[512 * 32];
    uint32_t clus_bytes = fat.sec_per_clus * 512;

    while (clus < 0x0FFFFFF8) {
        if (fat_read_clus(clus, tmp) < 0) return -1;

        for (uint32_t off = 0; off + 32 <= clus_bytes; off += 32) {
            uint8_t *e = tmp + off;
            uint8_t  attr = e[11];

            if (e[0] == 0) return -1;
            if (e[0] == 0xE5 || attr == 0x0F || (attr & 0x08) || (attr & 0x10)) continue;

            char fname[13];
            sfntostr(fname, (const char *)e, 12);
            if (fname[0] && s_cmpi(fname, name) == 0) {
                uint32_t eclus = (*(uint16_t *)(e + 20) << 16) | *(uint16_t *)(e + 26);
                uint32_t fsize = *(uint32_t *)(e + 28);
                uint32_t remain = fsize;

                while (eclus < 0x0FFFFFF8 && remain > 0) {
                    uint32_t lba = fat_clus2lba(eclus);
                    uint32_t to_read = clus_bytes;
                    if (to_read > remain) to_read = remain;
                    if (disk_read(fat.drv, lba, to_read / 512, buf) < 0)
                        return -1;
                    buf += to_read;
                    remain -= to_read;
                    eclus = fat_next_clus(eclus);
                }
                if (size_out) *size_out = fsize;
                return fsize;
            }
        }

        clus = fat_next_clus(clus);
    }
    return -1;
}

/* ================================================================
   WRITE SUPPORT
   ================================================================ */

static int fat_write_clus(uint32_t clus, const uint8_t *buf)
{
    uint32_t lba = fat_clus2lba(clus);
    return disk_write(fat.drv, lba, fat.sec_per_clus, buf);
}

static int fat_set_fat(uint32_t clus, uint32_t val)
{
    val &= 0x0FFFFFFF;
    uint32_t fat_sec = fat.fat_start + (clus * 4) / fat.bytes_per_sec;
    uint32_t off     = (clus * 4) % fat.bytes_per_sec;

    for (uint32_t c = 0; c < fat.nfat; c++) {
        if (disk_read(fat.drv, fat_sec + c * fat.fatsz, 1, fat.buf) < 0) return -1;
        *(uint32_t *)(fat.buf + off) = (*(uint32_t *)(fat.buf + off) & 0xF0000000) | val;
        if (disk_write(fat.drv, fat_sec + c * fat.fatsz, 1, fat.buf) < 0) return -1;
    }
    return 0;
}

static uint32_t fat_alloc_clus(void)
{
    uint32_t total = fat.fatsz * fat.bytes_per_sec / 4;

    for (uint32_t c = 2; c < total; c++) {
        uint32_t sec  = fat.fat_start + (c * 4) / fat.bytes_per_sec;
        uint32_t off  = (c * 4) % fat.bytes_per_sec;
        if (disk_read(fat.drv, sec, 1, fat.buf) < 0) return 0xFFFFFFFF;
        uint32_t val = *(uint32_t *)(fat.buf + off) & 0x0FFFFFFF;
        if (val == 0) {
            if (fat_set_fat(c, 0x0FFFFFFF) < 0) return 0xFFFFFFFF;
            return c;
        }
    }
    return 0xFFFFFFFF;
}

static void strtosfn(const char *name, uint8_t *sfn)
{
    /* Name: 8 bytes, space-padded */
    int i;
    for (i = 0; i < 8; i++) {
        if (*name && *name != '.' && *name != ' ') {
            char c = *name++;
            if (c >= 'a' && c <= 'z') c -= 32;
            sfn[i] = c;
        } else {
            sfn[i] = ' ';
        }
    }
    /* Skip to extension after dot */
    const char *ext = 0;
    const char *p = name;
    while (*p) {
        if (*p == '.') ext = p + 1;
        p++;
    }
    if (ext) name = ext;
    else name = 0;
    /* Extension: 3 bytes, space-padded */
    for (i = 0; i < 3; i++) {
        if (name && *name) {
            char c = *name++;
            if (c >= 'a' && c <= 'z') c -= 32;
            sfn[8 + i] = c;
        } else {
            sfn[8 + i] = ' ';
        }
    }
}

int fat_write_file(const char *name, const uint8_t *data, uint32_t size)
{
    if (!fat.mounted) return -1;

    /* Calculate needed clusters */
    uint32_t clus_bytes = fat.sec_per_clus * 512;
    uint32_t need_clus = (size + clus_bytes - 1) / clus_bytes;
    if (need_clus == 0) need_clus = 1;

    /* Allocate clusters */
    static uint32_t clus_list[256];
    if (need_clus > 256) return -1;

    for (uint32_t i = 0; i < need_clus; i++) {
        clus_list[i] = fat_alloc_clus();
        if (clus_list[i] == 0xFFFFFFFF) {
            /* Free already allocated */
            for (uint32_t j = 0; j < i; j++)
                fat_set_fat(clus_list[j], 0);
            return -1;
        }
    }

    /* Chain clusters */
    for (uint32_t i = 0; i < need_clus; i++) {
        if (i < need_clus - 1)
            fat_set_fat(clus_list[i], clus_list[i + 1]);
        else
            fat_set_fat(clus_list[i], 0x0FFFFFFF);
    }

    /* Write data */
    uint32_t remain = size;
    for (uint32_t i = 0; i < need_clus && remain > 0; i++) {
        uint32_t lba = fat_clus2lba(clus_list[i]);
        uint32_t to_write = clus_bytes;
        if (to_write > remain) to_write = remain;
        if (disk_write(fat.drv, lba, to_write / 512, data + (i * clus_bytes)) < 0)
            return -1;
        remain -= to_write;
    }

    /* Find free directory entry */
    uint32_t clus = fat.root_clus;
    static uint8_t tmp[512 * 32];

    while (clus < 0x0FFFFFF8) {
        if (fat_read_clus(clus, tmp) < 0) return -1;

        for (uint32_t off = 0; off + 32 <= clus_bytes; off += 32) {
            if (tmp[off] == 0 || tmp[off] == 0xE5) {
                /* Create SFN directory entry */
                uint8_t sfn[11];
                strtosfn(name, sfn);

                /* Write name */
                for (int i = 0; i < 11; i++) tmp[off + i] = sfn[i];

                /* Attributes: archive */
                tmp[off + 11] = 0x20;

                /* Reserved / creation time fields — zero */
                for (int i = 12; i < 20; i++) tmp[off + i] = 0;

                /* First cluster HI (FAT32) */
                tmp[off + 20] = (clus_list[0] >> 16) & 0xFF;
                tmp[off + 21] = (clus_list[0] >> 24) & 0xFF;

                /* Last modified time (12:00:00) */
                tmp[off + 22] = 0; tmp[off + 23] = 0;

                /* Last modified date (arbitrary) */
                tmp[off + 24] = 0; tmp[off + 25] = 0;

                /* First cluster LO (FAT32) */
                tmp[off + 26] = clus_list[0] & 0xFF;
                tmp[off + 27] = (clus_list[0] >> 8) & 0xFF;

                /* File size */
                tmp[off + 28] = size & 0xFF;
                tmp[off + 29] = (size >> 8) & 0xFF;
                tmp[off + 30] = (size >> 16) & 0xFF;
                tmp[off + 31] = (size >> 24) & 0xFF;

                /* Write back the directory cluster */
                if (fat_write_clus(clus, tmp) < 0) return -1;

                return 0;
            }
        }
        clus = fat_next_clus(clus);
    }
    return -1;
}

int fat_delete_file(const char *name)
{
    if (!fat.mounted) return -1;

    uint32_t clus = fat.root_clus;
    static uint8_t tmp[512 * 32];
    uint32_t clus_bytes = fat.sec_per_clus * 512;

    while (clus < 0x0FFFFFF8) {
        if (fat_read_clus(clus, tmp) < 0) return -1;

        for (uint32_t off = 0; off + 32 <= clus_bytes; off += 32) {
            uint8_t *e = tmp + off;
            uint8_t attr = e[11];

            if (e[0] == 0) continue;
            if (e[0] == 0xE5 || attr == 0x0F || (attr & 0x08)) continue;

            char fname[13];
            sfntostr(fname, (const char *)e, 12);
            if (fname[0] && s_cmpi(fname, name) == 0) {
                /* Free cluster chain */
                uint32_t eclus = (*(uint16_t *)(e + 20) << 16) | *(uint16_t *)(e + 26);
                while (eclus < 0x0FFFFFF8) {
                    uint32_t next = fat_next_clus(eclus);
                    fat_set_fat(eclus, 0);
                    eclus = next;
                }

                /* Mark entry as deleted */
                tmp[off] = 0xE5;
                /* Also clear LFN entries above this one if any */
                /* For now, just write back the directory cluster */
                if (fat_write_clus(clus, tmp) < 0) return -1;

                return 0;
            }
        }
        clus = fat_next_clus(clus);
    }
    return -1;
}

/* ===== snowfs.c ===== */

static int rd32_le(const uint8_t *p)
{
    return (int)p[0] | ((int)p[1] << 8) | ((int)p[2] << 16) | ((int)p[3] << 24);
}

static void wr32_le(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}

static struct {
    int      mounted;
    int      drv;
    uint32_t part_lba;
    SnowSuper sb;
    uint32_t root_lba;
    uint32_t fat_lba;
    uint32_t data_lba;
    uint32_t clus_bytes;
    uint32_t root_entries;
    uint8_t  sec[512];
} sfs;

static uint32_t sfs_clus2lba(uint32_t clus)
{
    return sfs.data_lba + clus * sfs.sb.clus_sectors;
}

static int sfs_read_clus(uint32_t clus, uint8_t *buf)
{
    return disk_read(sfs.drv, sfs_clus2lba(clus), sfs.sb.clus_sectors, buf);
}

static int sfs_write_clus(uint32_t clus, const uint8_t *buf)
{
    return disk_write(sfs.drv, sfs_clus2lba(clus), sfs.sb.clus_sectors, buf);
}

static int sfs_read_fat(uint32_t clus, uint32_t *val)
{
    uint32_t sec = sfs.fat_lba + (clus * 4) / 512;
    uint32_t off = (clus * 4) % 512;
    if (disk_read(sfs.drv, sec, 1, sfs.sec) < 0) return -1;
    *val = rd32_le(sfs.sec + off);
    return 0;
}

static int sfs_write_fat(uint32_t clus, uint32_t val)
{
    uint32_t sec = sfs.fat_lba + (clus * 4) / 512;
    uint32_t off = (clus * 4) % 512;
    if (disk_read(sfs.drv, sec, 1, sfs.sec) < 0) return -1;
    wr32_le(sfs.sec + off, val);
    return disk_write(sfs.drv, sec, 1, sfs.sec);
}

static int sfs_next_clus(uint32_t clus, uint32_t *next)
{
    return sfs_read_fat(clus, next);
}

static int sfs_alloc_clus(void)
{
    uint32_t n = sfs.sb.total_clusters;
    uint32_t hint = sfs.sb.next_hint;
    if (hint >= n) hint = 2;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t c = hint + i;
        if (c >= n) c = c - n + 2;
        if (c >= n) c = 2;
        uint32_t val;
        if (sfs_read_fat(c, &val) < 0) return -1;
        if (val == 0) {
            if (sfs_write_fat(c, 0xFFFFFFFF) < 0) return -1;
            sfs.sb.free_clusters--;
            sfs.sb.next_hint = c + 1;
            return (int)c;
        }
    }
    return -1;
}

static int sfs_free_chain(uint32_t clus)
{
    while (clus < 0x0FFFFFF0) {
        uint32_t next;
        if (sfs_read_fat(clus, &next) < 0) return -1;
        if (sfs_write_fat(clus, 0) < 0) return -1;
        sfs.sb.free_clusters++;
        clus = next;
    }
    return 0;
}

int snowfs_mount(int drv, uint32_t part_lba, uint32_t part_sectors)
{
    if (disk_read(drv, part_lba, 1, sfs.sec) < 0) return -1;

    SnowSuper *s = (SnowSuper *)sfs.sec;
    if (s->magic[0] != 'S' || s->magic[1] != 'N' || s->magic[6] != '1')
        return -1;

    sfs.mounted = 1;
    sfs.drv = drv;
    sfs.part_lba = part_lba;
    sfs.sb = *s;
    sfs.root_lba = part_lba + 1;
    sfs.fat_lba = part_lba + 1 + s->root_sectors;
    sfs.data_lba = part_lba + s->data_lba;
    sfs.clus_bytes = s->clus_sectors * 512;
    sfs.root_entries = s->root_sectors * 8;
    return 0;
}

int snowfs_format(int drv, uint32_t part_lba, uint32_t part_sectors)
{
    uint32_t clus_sec = 4;
    uint32_t root_ent = 256;
    uint32_t root_sec = (root_ent * SNOWFS_ENT_SZ + 511) / 512;
    uint32_t data_lba = 1 + root_sec;
    uint32_t avail_sec = part_sectors - data_lba;
    uint32_t total_clus = avail_sec / clus_sec;
    uint32_t fat_sec = (total_clus * 4 + 511) / 512;

    data_lba = 1 + root_sec + fat_sec;
    avail_sec = part_sectors - data_lba;
    total_clus = avail_sec / clus_sec;
    fat_sec = (total_clus * 4 + 511) / 512;

    SnowSuper sb;
    s_cpy(sb.magic, "SNOWFS1", 8);
    sb.version = 1;
    sb.total_sectors = part_sectors;
    sb.root_sectors = root_sec;
    sb.fat_sectors = fat_sec;
    sb.data_lba = data_lba;
    sb.clus_sectors = clus_sec;
    sb.total_clusters = total_clus;
    sb.free_clusters = total_clus;
    sb.next_hint = 2;
    for (int i = 0; i < 468; i++) sb.reserved[i] = 0;

    if (disk_write(drv, part_lba, 1, (const uint8_t *)&sb) < 0) return -1;

    uint8_t zero[512];
    for (int i = 0; i < 512; i++) zero[i] = 0;

    for (uint32_t i = 0; i < root_sec; i++)
        if (disk_write(drv, part_lba + 1 + i, 1, zero) < 0) return -1;

    for (uint32_t i = 0; i < fat_sec; i++)
        if (disk_write(drv, part_lba + 1 + root_sec + i, 1, zero) < 0) return -1;

    return 0;
}

int snowfs_format_quick(int drv, uint32_t part_lba, uint32_t part_sectors)
{
    return snowfs_format(drv, part_lba, part_sectors);
}

int snowfs_iterate(void (*cb)(const char *name, uint32_t size))
{
    if (!sfs.mounted) return -1;

    uint8_t *buf = sfs.sec;
    uint32_t n = sfs.root_entries;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t ent_sec = sfs.root_lba + (i * SNOWFS_ENT_SZ) / 512;
        uint32_t ent_off = (i * SNOWFS_ENT_SZ) % 512;
        if (ent_off == 0)
            if (disk_read(sfs.drv, ent_sec, 1, buf) < 0) return -1;

        SnowEntry *e = (SnowEntry *)(buf + ent_off);
        if (e->first_clus != 0 && e->name[0] != 0)
            cb(e->name, e->size);
    }
    return 0;
}

int snowfs_read(const char *name, uint8_t *buf, uint32_t *size_out)
{
    if (!sfs.mounted) return -1;

    uint32_t n = sfs.root_entries;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t ent_sec = sfs.root_lba + (i * SNOWFS_ENT_SZ) / 512;
        uint32_t ent_off = (i * SNOWFS_ENT_SZ) % 512;
        if (ent_off == 0)
            if (disk_read(sfs.drv, ent_sec, 1, sfs.sec) < 0) return -1;

        SnowEntry *e = (SnowEntry *)(sfs.sec + ent_off);
        if (e->first_clus == 0 || e->name[0] == 0) continue;

        if (s_cmpi(e->name, name) == 0) {
            uint32_t remain = e->size;
            uint32_t clus = e->first_clus;
            uint8_t tmp[4096];

            while (clus < 0x0FFFFFF0 && remain > 0) {
                uint32_t to_read = sfs.clus_bytes;
                if (to_read > remain) to_read = remain;
                if (sfs_read_clus(clus, tmp) < 0) return -1;
                for (uint32_t j = 0; j < to_read; j++)
                    buf[j] = tmp[j];
                buf += to_read;
                remain -= to_read;
                if (sfs_next_clus(clus, &clus) < 0) return -1;
            }

            if (size_out) *size_out = e->size;
            return 0;
        }
    }
    return -1;
}

int snowfs_write(const char *name, const uint8_t *data, uint32_t size)
{
    if (!sfs.mounted) return -1;

    snowfs_delete(name);

    uint32_t need_clus = (size + sfs.clus_bytes - 1) / sfs.clus_bytes;
    if (need_clus == 0) need_clus = 1;

    static uint32_t clus_list[512];
    if (need_clus > 512) return -1;

    for (uint32_t i = 0; i < need_clus; i++) {
        int c = sfs_alloc_clus();
        if (c < 0) {
            for (uint32_t j = 0; j < i; j++)
                sfs_free_chain(clus_list[j]);
            return -1;
        }
        clus_list[i] = (uint32_t)c;
    }

    for (uint32_t i = 0; i < need_clus; i++) {
        if (i < need_clus - 1)
            sfs_write_fat(clus_list[i], clus_list[i + 1]);
        else
            sfs_write_fat(clus_list[i], 0xFFFFFFFF);
    }

    uint32_t remain = size;
    for (uint32_t i = 0; i < need_clus && remain > 0; i++) {
        uint32_t to_write = sfs.clus_bytes;
        if (to_write > remain) to_write = remain;

        uint8_t tmp[4096];
        for (uint32_t j = 0; j < to_write; j++)
            tmp[j] = data[i * sfs.clus_bytes + j];

        if (sfs_write_clus(clus_list[i], tmp) < 0) return -1;
        remain -= to_write;
    }

    uint32_t n = sfs.root_entries;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t ent_sec = sfs.root_lba + (i * SNOWFS_ENT_SZ) / 512;
        uint32_t ent_off = (i * SNOWFS_ENT_SZ) % 512;
        if (ent_off == 0)
            if (disk_read(sfs.drv, ent_sec, 1, sfs.sec) < 0) return -1;

        SnowEntry *e = (SnowEntry *)(sfs.sec + ent_off);
        if (e->first_clus == 0) {
            s_cpy(e->name, name, 56);
            e->size = size;
            e->first_clus = clus_list[0];
            return disk_write(sfs.drv, ent_sec, 1, sfs.sec);
        }
    }
    return -1;
}

int snowfs_delete(const char *name)
{
    if (!sfs.mounted) return -1;

    uint32_t n = sfs.root_entries;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t ent_sec = sfs.root_lba + (i * SNOWFS_ENT_SZ) / 512;
        uint32_t ent_off = (i * SNOWFS_ENT_SZ) % 512;
        if (ent_off == 0)
            if (disk_read(sfs.drv, ent_sec, 1, sfs.sec) < 0) return -1;

        SnowEntry *e = (SnowEntry *)(sfs.sec + ent_off);
        if (e->first_clus == 0 || e->name[0] == 0) continue;

        if (s_cmpi(e->name, name) == 0) {
            sfs_free_chain(e->first_clus);
            e->first_clus = 0;
            e->name[0] = 0;
            e->size = 0;
            return disk_write(sfs.drv, ent_sec, 1, sfs.sec);
        }
    }
    return -1;
}
