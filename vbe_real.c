#include "lunarsnow.h"

extern uint16_t int10_real(uint16_t ax, uint16_t bx, uint16_t cx, uint16_t di, uint16_t es);

#define VBE_INFO_ADDR   0x6000
#define MODE_INFO_ADDR  0x6200

typedef struct {
    char     sig[4];
    uint16_t version;
    uint32_t oem_ptr;
    uint32_t caps;
    uint32_t video_mode_ptr;
    uint16_t total_mem;
    uint16_t oem_sw_rev;
    uint32_t oem_vendor;
    uint32_t oem_product;
    uint32_t oem_rev;
    uint8_t  reserved[222];
} __attribute__((packed)) vbe_info_t;

typedef struct {
    uint16_t attributes;
    uint8_t  win_a_attr;
    uint8_t  win_b_attr;
    uint16_t win_gran;
    uint16_t win_size;
    uint16_t win_a_seg;
    uint16_t win_b_seg;
    uint32_t win_func;
    uint16_t bytes_per_scan;
    uint16_t x_res;
    uint16_t y_res;
    uint8_t  x_char_size;
    uint8_t  y_char_size;
    uint8_t  planes;
    uint8_t  bpp;
    uint16_t banks;
    uint8_t  mem_model;
    uint8_t  bank_size;
    uint8_t  img_pages;
    uint8_t  reserved1;
    uint8_t  red_mask;
    uint8_t  red_pos;
    uint8_t  green_mask;
    uint8_t  green_pos;
    uint8_t  blue_mask;
    uint8_t  blue_pos;
    uint8_t  rsvd_mask;
    uint8_t  rsvd_pos;
    uint8_t  directcolor;
    uint32_t phys_base;
    uint32_t reserved2;
    uint16_t reserved3;
    uint8_t  reserved4[206];
} __attribute__((packed)) mode_info_t;

static volatile vbe_info_t * const vbe_info = (volatile vbe_info_t *)VBE_INFO_ADDR;
static volatile mode_info_t * const mode_info = (volatile mode_info_t *)MODE_INFO_ADDR;

static int set_mode_from_info(uint16_t mode, int w, int h, int bpp)
{
    uint32_t phys_base = mode_info->phys_base;
    uint16_t pitch = mode_info->bytes_per_scan;

    if (int10_real(0x4F02, mode | 0x4000, 0, 0, 0) != 0x004F)
        return -1;

    uint32_t *addr = (uint32_t *)(uintptr_t)phys_base;
    if (!addr) return -1;

    fb_init_ptr(addr, w, h, pitch, bpp);
    fb_clear(0);
    gui_reset_cursor();
    if (mouse_x >= fb_w) mouse_x = fb_w - 1;
    if (mouse_y >= fb_h) mouse_y = fb_h - 1;
    need_render = 1;
    return 0;
}

int vbe_try_set_mode(int w, int h, int bpp)
{
    if (bpp != 32) return -1;

    /* Try scanning VBE modes via int 0x10 AX=0x4F00/0x4F01 */
    if (int10_real(0x4F00, 0, 0, VBE_INFO_ADDR, 0) == 0x004F &&
        vbe_info->sig[0] == 'V' && vbe_info->sig[1] == 'E' &&
        vbe_info->sig[2] == 'S' && vbe_info->sig[3] == 'A') {

        uint16_t seg = vbe_info->video_mode_ptr >> 16;
        uint16_t off = vbe_info->video_mode_ptr & 0xFFFF;
        volatile uint16_t *modes = (volatile uint16_t *)(uintptr_t)((seg << 4) + off);

        for (int i = 0; modes[i] != 0xFFFF; i++) {
            uint16_t mode_num = modes[i];

            if (int10_real(0x4F01, 0, mode_num, MODE_INFO_ADDR, 0) != 0x004F)
                continue;

            uint16_t attr = mode_info->attributes;
            if (!(attr & 0x01)) continue;
            if (!(attr & 0x08)) continue;
            if (!(attr & 0x80)) continue;
            if (mode_info->x_res != w || mode_info->y_res != h) continue;
            if (mode_info->bpp != bpp) continue;
            if (mode_info->phys_base == 0) continue;

            return set_mode_from_info(mode_num, w, h, bpp);
        }
    }

    /* Fallback: hardcoded VESA mode numbers */
    static const struct { int w, h, mode; } vesa_modes[] = {
        {640,  480,  0x112},
        {800,  600,  0x115},
        {1024, 768,  0x118},
        {1280, 1024, 0x11B},
    };

    int mode = 0;
    for (int i = 0; i < (int)(sizeof(vesa_modes)/sizeof(vesa_modes[0])); i++) {
        if (vesa_modes[i].w == w && vesa_modes[i].h == h) {
            mode = vesa_modes[i].mode;
            break;
        }
    }
    if (!mode) return -1;

    if (int10_real(0x4F01, 0, (uint16_t)mode, MODE_INFO_ADDR, 0) != 0x004F)
        return -1;

    return set_mode_from_info((uint16_t)mode, w, h, bpp);
}

