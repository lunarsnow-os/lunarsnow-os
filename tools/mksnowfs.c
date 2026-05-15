#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define SECTOR_SIZE 512

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <disk.img> <part_lba> <part_sectors> [files...]\n", argv[0]);
        return 1;
    }

    const char *img_path = argv[1];
    uint32_t part_lba = (uint32_t)atol(argv[2]);
    uint32_t part_sectors = (uint32_t)atol(argv[3]);

    FILE *f = fopen(img_path, "r+b");
    if (!f) { perror("fopen"); return 1; }

    uint32_t clus_sec = 4;
    uint32_t root_ent = 256;
    uint32_t root_sec = (root_ent * 64 + 511) / 512;
    uint32_t data_lba = 1 + root_sec;
    uint32_t avail_sec = part_sectors - data_lba;
    uint32_t total_clus = avail_sec / clus_sec;
    uint32_t fat_sec = (total_clus * 4 + 511) / 512;

    data_lba = 1 + root_sec + fat_sec;
    avail_sec = part_sectors - data_lba;
    total_clus = avail_sec / clus_sec;
    fat_sec = (total_clus * 4 + 511) / 512;

    printf("SnowFS partition at LBA %u, %u sectors\n", part_lba, part_sectors);
    printf("  root_sec=%u fat_sec=%u data_lba=%u total_clus=%u\n",
           root_sec, fat_sec, data_lba, total_clus);

    uint8_t sb[SECTOR_SIZE];
    memset(sb, 0, SECTOR_SIZE);
    memcpy(sb, "SNOWFS1", 8);
    wr32(sb + 8, 1);
    wr32(sb + 12, part_sectors);
    wr32(sb + 16, root_sec);
    wr32(sb + 20, fat_sec);
    wr32(sb + 24, data_lba);
    wr32(sb + 28, clus_sec);
    wr32(sb + 32, total_clus);
    wr32(sb + 36, total_clus);
    wr32(sb + 40, 2);

    fseek(f, part_lba * SECTOR_SIZE, SEEK_SET);
    if (fwrite(sb, 1, SECTOR_SIZE, f) != SECTOR_SIZE) {
        perror("write superblock"); fclose(f); return 1;
    }

    uint8_t zero[SECTOR_SIZE];
    memset(zero, 0, SECTOR_SIZE);

    for (uint32_t i = 0; i < root_sec; i++) {
        fwrite(zero, 1, SECTOR_SIZE, f);
    }

    for (uint32_t i = 0; i < fat_sec; i++) {
        fwrite(zero, 1, SECTOR_SIZE, f);
    }

    for (int i = 4; i < argc; i++) {
        printf("  copying %s...\n", argv[i]);

        FILE *src = fopen(argv[i], "rb");
        if (!src) { perror(argv[i]); continue; }

        fseek(src, 0, SEEK_END);
        long src_size = ftell(src);
        fseek(src, 0, SEEK_SET);

        if (src_size <= 0) { fclose(src); continue; }

        uint32_t need_clus = ((uint32_t)src_size + clus_sec * 512 - 1) / (clus_sec * 512);
        if (need_clus == 0) need_clus = 1;

        uint32_t *clus_list = malloc(need_clus * sizeof(uint32_t));
        if (!clus_list) { fclose(src); continue; }

        for (uint32_t j = 0; j < need_clus; j++)
            clus_list[j] = total_clus - need_clus + j;

        uint32_t fat_lba = part_lba + 1 + root_sec;
        for (uint32_t j = 0; j < need_clus; j++) {
            uint32_t next = (j < need_clus - 1) ? clus_list[j + 1] : 0xFFFFFFFF;
            uint32_t fat_off = fat_lba + (clus_list[j] * 4) / 512;
            uint32_t fat_inner = (clus_list[j] * 4) % 512;

            fseek(f, fat_off * SECTOR_SIZE + fat_inner, SEEK_SET);
            uint8_t ent[4];
            wr32(ent, next);
            fwrite(ent, 1, 4, f);
        }

        uint32_t data_lba_abs = part_lba + data_lba;
        for (uint32_t j = 0; j < need_clus; j++) {
            uint32_t lba = data_lba_abs + clus_list[j] * clus_sec;
            fseek(f, lba * SECTOR_SIZE, SEEK_SET);
            uint8_t buf[4096];
            memset(buf, 0, 4096);
            long to_read = (j < need_clus - 1) ? (long)(clus_sec * 512)
                                               : (src_size - j * clus_sec * 512);
            if (to_read > (long)(clus_sec * 512)) to_read = clus_sec * 512;
            fread(buf, 1, to_read, src);
            fwrite(buf, 1, clus_sec * 512, f);
        }

        const char *fname = argv[i];
        const char *base = strrchr(fname, '/');
        if (base) fname = base + 1;

        uint8_t dent[64];
        memset(dent, 0, 64);
        int nl = strlen(fname);
        if (nl > 55) nl = 55;
        memcpy(dent, fname, nl);
        wr32(dent + 56, (uint32_t)src_size);
        wr32(dent + 60, clus_list[0]);

        uint32_t dir_pos = part_lba + 1;
        for (uint32_t j = 0; j < root_ent; j++) {
            uint32_t ent_sec = dir_pos + (j * 64) / 512;
            uint32_t ent_off = (j * 64) % 512;

            fseek(f, ent_sec * SECTOR_SIZE + ent_off, SEEK_SET);
            uint8_t check[4];
            fread(check, 1, 4, f);

            int empty = 1;
            for (int k = 0; k < 4; k++) if (check[k] != 0) { empty = 0; break; }

            if (empty) {
                fseek(f, ent_sec * SECTOR_SIZE + ent_off, SEEK_SET);
                fwrite(dent, 1, 64, f);
                break;
            }
        }

        free(clus_list);
        fclose(src);
    }

    fclose(f);
    printf("Done.\n");
    return 0;
}
