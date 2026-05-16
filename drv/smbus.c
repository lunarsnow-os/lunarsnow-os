#include <stdint.h>
#include "smbus.h"
#include "io.h"
#include "gui.h"
#include "fb.h"

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
    /* Try class code 0x0C0500 first (ICH/PCH dedicated SMBus controllers) */
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

    /* Fallback: Intel ISA bridge devices with class 0x068000 (PIIX4/ICH ACPI)
       that also contain SMBus registers at BAR offset 0x20 (function 3). */
    for (int dev = 0; dev < 32; dev++) {
        uint32_t vd0 = pci_readl(PCI_ADDR(0, dev, 0, 0));
        if ((vd0 & 0xFFFF) == 0xFFFF) break;
        if ((vd0 & 0xFFFF) != 0x8086) continue;

        for (int func = 0; func < 4; func++) {
            uint32_t cc = pci_readl(PCI_ADDR(0, dev, func, 8));
            if ((cc >> 8) != 0x068000) continue;

            uint32_t bar = pci_readl(PCI_ADDR(0, dev, func, 0x20));
            if (!(bar & 1)) continue;
            smb_base = bar & 0xFFF0;
            if (smb_base < 0x400 || smb_base > 0xFFFE) continue;

            return 1;
        }
    }

    return 0;
}

static int detect_vm(void)
{
    uint32_t a, b, c, d;
    asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0x40000000));
    if (a < 0x40000000) return 0;
    char sig[13];
    *(uint32_t*)(sig + 0) = b;
    *(uint32_t*)(sig + 4) = c;
    *(uint32_t*)(sig + 8) = d;
    sig[12] = 0;
    return (s_cmp(sig, "KVMKVMKVM") == 0 || s_cmp(sig, "VBoxVBoxVBox") == 0);
}

void battery_poll(void)
{
    static int init_done;
    static int last_pct = -1;
    static uint8_t bat_addr;
    static int sim_mode;
    uint16_t val;

    if (!smb_base) return;

    if (!init_done) {
        init_done = 1;
        battery_percent = 0;
        battery_charging = 0;
        battery_present = 0;
        sim_mode = 0;

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
            return;
        }

        /* No real battery found — simulate in VMs for testing */
        if (detect_vm()) {
            battery_present = 1;
            battery_percent = 80;
            battery_charging = 1;
            last_pct = 80;
            sim_mode = 1;
        }
        return;
    }

    if (!battery_present) return;

    if (!sim_mode) {
        /* Real Smart Battery read */
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
    } else {
        /* Simulation mode for VMs — cycle 80% → 0% → 100% → 80% */
        static int hold;
        if (++hold < 60) return;
        hold = 0;

        if (battery_charging) {
            battery_percent++;
            if (battery_percent >= 80) {
                battery_percent = 80;
                battery_charging = 0;
            }
        } else {
            battery_percent--;
            if (battery_percent <= 0) {
                battery_percent = 0;
                battery_charging = 1;
            }
        }
        need_render = 1;
    }
}
