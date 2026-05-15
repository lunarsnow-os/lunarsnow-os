#ifndef FS_H
#define FS_H

#include <stdint.h>

/* ─── Partition types ─── */
#define PART_MBR_FAT32  1
#define PART_GPT_DATA   2
#define PART_MBR_SNOWFS 3

typedef struct { uint64_t start, size; int type; } Partition;

/* ─── Disk I/O ─── */
int disk_detect_all(void);
int disk_count(void);
int disk_get_info(int drv, uint64_t *sectors, int *lba48);
int disk_read(int drv, uint64_t lba, int count, void *buf);
int disk_write(int drv, uint64_t lba, int count, const void *buf);

/* ─── Partition scan ─── */
int part_scan(int drv, Partition *parts, int max);

/* ─── SnowFS ─── */
#define SNOWFS_MAGIC   "SNOWFS1"
#define SNOWFS_VER     1
#define SNOWFS_ENT_SZ  64
#define SNOWFS_ENTSECS 8

#pragma pack(1)
typedef struct {
    char     magic[8];
    uint32_t version;
    uint32_t total_sectors;
    uint32_t root_sectors;
    uint32_t fat_sectors;
    uint32_t data_lba;
    uint32_t clus_sectors;
    uint32_t total_clusters;
    uint32_t free_clusters;
    uint32_t next_hint;
    uint8_t  reserved[468];
} SnowSuper;

typedef struct {
    char     name[56];
    uint32_t size;
    uint32_t first_clus;
} SnowEntry;
#pragma pack()

int snowfs_mount(int drv, uint32_t part_lba, uint32_t part_sectors);
int snowfs_format(int drv, uint32_t part_lba, uint32_t part_sectors);
int snowfs_iterate(void (*cb)(const char *name, uint32_t size));
int snowfs_read(const char *name, uint8_t *buf, uint32_t *size_out);
int snowfs_write(const char *name, const uint8_t *data, uint32_t size);
int snowfs_delete(const char *name);

/* ─── FAT32 ─── */
int  fat_mount(void);
void fat_iterate(void (*cb)(const char *name, uint32_t size));
int  fat_read_file(const char *name, uint8_t *buf, uint32_t *size_out);
int  fat_write_file(const char *name, const uint8_t *data, uint32_t size);
int  fat_delete_file(const char *name);

#endif
