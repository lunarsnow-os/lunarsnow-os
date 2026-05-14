#include <stdint.h>
#include "disk.h"
#include "fb.h"

static struct {
    int  mounted;
    uint32_t fat_start;     /* LBA of first FAT */
    uint32_t data_start;    /* LBA of data area */
    uint32_t root_clus;     /* First cluster of root dir */
    uint32_t sec_per_clus;
    uint32_t bytes_per_sec;
    uint8_t  buf[512];
} fs;

/* Cluster to LBA */
static uint32_t clus2lba(uint32_t clus)
{
    return fs.data_start + (clus - 2) * fs.sec_per_clus;
}

/* Read next cluster in chain. Returns 0x0FFFFFF8-0x0FFFFFFF for end. */
static uint32_t next_clus(uint32_t clus)
{
    uint32_t fat_sec = fs.fat_start + (clus * 4) / fs.bytes_per_sec;
    uint32_t off     = (clus * 4) % fs.bytes_per_sec;
    if (disk_read(fat_sec, 1, (uint16_t *)fs.buf) < 0) return 0xFFFFFFFF;
    uint32_t val = *(uint32_t *)(fs.buf + off) & 0x0FFFFFFF;
    return val;
}

/* Read a cluster's data into buf (size = clus * sec_per_clus * 512) */
static int read_clus(uint32_t clus, uint8_t *buf)
{
    uint32_t lba = clus2lba(clus);
    return disk_read(lba, fs.sec_per_clus, (uint16_t *)buf);
}

/* Convert DOS 8.3 name to string, returns length */
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

/* Mount the first FAT32 partition found on disk.
   Returns 0 on success. */
int fat_mount(void)
{
    /* Read MBR */
    if (disk_read(0, 1, (uint16_t *)fs.buf) < 0) return -1;
    if (fs.buf[510] != 0x55 || fs.buf[511] != 0xAA) return -1;

    /* Scan partition table for FAT32 (type 0x0B or 0x0C) */
    uint32_t part_lba = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t *e = fs.buf + 446 + i * 16;
        uint8_t  t = e[4];
        if (t == 0x0B || t == 0x0C) {
            part_lba = *(uint32_t *)(e + 8);
            break;
        }
    }
    if (part_lba == 0) return -1;

    /* Read FAT32 VBR */
    if (disk_read(part_lba, 1, (uint16_t *)fs.buf) < 0) return -1;
    if (fs.buf[510] != 0x55 || fs.buf[511] != 0xAA) return -1;

    uint16_t bps = *(uint16_t *)(fs.buf + 11);
    uint8_t  spc = fs.buf[13];
    uint16_t rsvd = *(uint16_t *)(fs.buf + 14);
    uint8_t  nfat = fs.buf[16];
    uint32_t fatsz = *(uint32_t *)(fs.buf + 36);
    uint32_t rootc = *(uint32_t *)(fs.buf + 44);

    if (bps != 512 || spc == 0) return -1;

    fs.bytes_per_sec = bps;
    fs.sec_per_clus  = spc;
    fs.fat_start     = part_lba + rsvd;
    fs.data_start    = part_lba + rsvd + nfat * fatsz;
    fs.root_clus     = rootc;
    fs.mounted       = 1;
    return 0;
}

/* Iterate root directory entries, calling cb(name, size) for each file.
   name is only valid during the callback. */
void fat_iterate(void (*cb)(const char *name, uint32_t size))
{
    if (!fs.mounted) return;

    uint32_t clus = fs.root_clus;
    static uint8_t buf[512 * 32];
    uint32_t clus_bytes = fs.sec_per_clus * 512;
    static char lfn_buf[512];
    int      lfn_pos = 0;
    int      lfn_n   = 0;

    while (clus < 0x0FFFFFF8) {
        if (read_clus(clus, buf) < 0) return;

        for (uint32_t off = 0; off + 32 <= clus_bytes; off += 32) {
            uint8_t *e = buf + off;
            uint8_t  attr = e[11];

            if (e[0] == 0) return;            /* end of directory */

            if (attr == 0x0F) {
                /* Long filename entry */
                uint8_t seq = e[0] & 0x3F;    /* sequence number (1-20) */
                if (seq == 0 || seq > 20) continue;
                if (e[0] & 0x40) {           /* last LFN entry — comes first in dir */
                    lfn_n = seq;             /* total number of LFN blocks */
                    lfn_pos = 1;             /* flag: accumulating LFN */
                    for (int i = 0; i < lfn_n * 13; i++) lfn_buf[i] = 0;
                }
                if (!lfn_n) continue;        /* safety: no 0x40 entry seen yet */
                int base = (lfn_n - seq) * 13;
                for (int i = 0; i < 5 && base + i < 511; i++)
                    if (e[1 + i*2]) lfn_buf[base + i] = e[1 + i*2];
                for (int i = 0; i < 6 && base + 5 + i < 511; i++)
                    if (e[14 + i*2]) lfn_buf[base + 5 + i] = e[14 + i*2];
                for (int i = 0; i < 2 && base + 11 + i < 511; i++)
                    if (e[28 + i*2]) lfn_buf[base + 11 + i] = e[28 + i*2];
                continue;
            }

            if (e[0] == 0xE5) { lfn_pos = 0; continue; } /* deleted entry */
            if (attr & 0x08)  { lfn_pos = 0; continue; } /* volume label */

            /* Regular file/directory entry */
            uint32_t fsize    = *(uint32_t *)(e + 28);

            char fname[256];

            if (lfn_pos) {
                /* Use accumulated LFN (already in correct order) */
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
                /* No LFN — use short filename (8.3) */
                sfntostr(fname, (const char *)e, 255);
                /* Skip . and .. */
                if (fname[0] == '.' && (fname[1] == 0 || (fname[1] == '.' && fname[2] == 0)))
                    continue;
            }

            /* Skip directories for now */
            if (attr & 0x10) continue;

            cb(fname, fsize);
        }

        clus = next_clus(clus);
    }
}

/* Read a file's content from root directory.
   Caller must provide buf with enough space.
   Returns file size on success, -1 on error. */
int fat_read_file(const char *name, uint8_t *buf, uint32_t *size_out)
{
    if (!fs.mounted) return -1;

    uint32_t clus = fs.root_clus;
    static uint8_t tmp[512 * 32];
    uint32_t clus_bytes = fs.sec_per_clus * 512;

    while (clus < 0x0FFFFFF8) {
        if (read_clus(clus, tmp) < 0) return -1;

        for (uint32_t off = 0; off + 32 <= clus_bytes; off += 32) {
            uint8_t *e = tmp + off;
            uint8_t  attr = e[11];

            if (e[0] == 0) return -1;
            if (e[0] == 0xE5 || attr == 0x0F || (attr & 0x08) || (attr & 0x10)) continue;

            /* Match SFN (case-insensitive) */
            char fname[13];
            sfntostr(fname, (const char *)e, 12);
            if (fname[0] && s_cmpi(fname, name) == 0) {
                uint32_t eclus = (*(uint16_t *)(e + 20) << 16) | *(uint16_t *)(e + 26);
                uint32_t fsize = *(uint32_t *)(e + 28);
                uint32_t remain   = fsize;

                while (eclus < 0x0FFFFFF8 && remain > 0) {
                    uint32_t lba = clus2lba(eclus);
                    uint32_t to_read = clus_bytes;
                    if (to_read > remain) to_read = remain;
                    if (disk_read(lba, to_read / 512, (uint16_t *)buf) < 0)
                        return -1;
                    buf += to_read;
                    remain -= to_read;
                    eclus = next_clus(eclus);
                }
                if (size_out) *size_out = fsize;
                return fsize;
            }
        }

        clus = next_clus(clus);
    }
    return -1;
}
