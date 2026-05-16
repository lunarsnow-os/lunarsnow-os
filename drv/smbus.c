#include <stdint.h>
#include "smbus.h"
#include "io.h"
#include "gui.h"

#define PCI_ADDR(bus, dev, func, reg) \
    (0x80000000 | (bus << 16) | (dev << 11) | (func << 8) | (reg & 0xFC))

static uint32_t pci_readl(uint32_t addr)
{
    outl(0xCF8, addr);
    return inl(0xCFC);
}

static uint16_t smb_base;
int battery_present;
int battery_percent;
int battery_charging;

static int smb_wait(uint16_t base)
{
    int timeout = 30000;
    while (inb(base + 0x00) & 1) {
        if (--timeout <= 0) return -1;
        asm("pause");
    }
    return 0;
}

static int smb_read_word(uint16_t base, uint8_t addr, uint8_t cmd, uint16_t *data)
{
    if (smb_wait(base) < 0) return -1;

    outb(base + 0x00, 0xFE);
    outb(base + 0x03, cmd);
    outb(base + 0x04, (addr << 1) | 1);

    int timeout = 50000;
    outb(base + 0x02, 0xCC);
    while (1) {
        uint8_t sts = inb(base + 0x00);
        if (sts & 0xE0) { outb(base + 0x00, 0xFE); return -1; }
        if (sts & 0x02) break;
        if (--timeout <= 0) { outb(base + 0x00, 0xFE); return -1; }
        asm("pause");
    }

    uint8_t lo = inb(base + 0x05);
    uint8_t hi = inb(base + 0x06);
    *data = lo | (hi << 8);
    outb(base + 0x00, 0xFE);
    return 0;
}

int smbus_init(void)
{
    for (int dev = 0; dev < 32; dev++) {
        int mf = 0;
        for (int func = 0; func < 8; func++) {
            uint32_t vd = pci_readl(PCI_ADDR(0, dev, func, 0));
            uint16_t vid = vd & 0xFFFF;
            if (vid == 0xFFFF) { if (func == 0) break; continue; }
            if (func == 0) mf = 1;

            uint32_t cc = pci_readl(PCI_ADDR(0, dev, func, 8));
            if ((cc >> 8) != 0x0C0500) continue;

            uint32_t bar = pci_readl(PCI_ADDR(0, dev, func, 0x20));
            if (!(bar & 1)) continue;
            smb_base = bar & 0xFFF0;
            if (smb_base < 0x400 || smb_base > 0xFFFE) continue;

            return 1;
        }
        if (!mf) break;
    }
    return 0;
}

void battery_poll(void)
{
    static int init_done;
    static int last_pct = -1;
    static uint8_t bat_addr;
    uint16_t val;

    if (!smb_base) return;

    if (!init_done) {
        init_done = 1;
        battery_percent = 0;
        battery_charging = 0;
        battery_present = 0;

        static const uint8_t addrs[] = { 0x16, 0x0B, 0x0C };
        for (int i = 0; i < 3; i++) {
            if (smb_read_word(smb_base, addrs[i], 0x0D, &val) < 0) continue;
            int pct = val;
            if (pct > 100) pct = (pct + 50) / 100;
            if (pct > 100) pct = 100;
            battery_percent = pct;
            battery_present = 1;
            last_pct = pct;
            bat_addr = addrs[i];

            if (smb_read_word(smb_base, addrs[i], 0x09, &val) == 0)
                battery_charging = (val & 0x0008) ? 1 : 0;
            break;
        }
        return;
    }

    if (!battery_present) return;

    if (smb_read_word(smb_base, bat_addr, 0x0D, &val) < 0) return;

    int pct = val;
    if (pct > 100) pct = (pct + 50) / 100;
    if (pct > 100) pct = 100;
    battery_percent = pct;

    if (pct != last_pct) {
        last_pct = pct;
        need_render = 1;
    }

    static int charge_cnt;
    if (++charge_cnt >= 6) {
        charge_cnt = 0;
        if (smb_read_word(smb_base, bat_addr, 0x09, &val) == 0) {
            int chg = (val & 0x0008) ? 1 : 0;
            if (chg != battery_charging) {
                battery_charging = chg;
                need_render = 1;
            }
        }
    }
}
