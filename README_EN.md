# LunarSnow OS

A bare-metal 64-bit graphical OS that boots directly in QEMU (no host OS).

Architecture: **x86_64**, long mode, 1 GB pages.

## Building

### Prerequisites

| Program | Debian/Ubuntu | Arch | Fedora |
|---|---|---|---|
| `gcc` + `ld` (x86_64) | `gcc` `binutils` | `gcc` `binutils` | `gcc` `binutils` |
| `as` (assembler) | `binutils` | `binutils` | `binutils` |
| `grub-mkrescue` | `grub2-common` `xorriso` `mtools` | `grub` `libisoburn` `mtools` | `grub2-tools` `xorriso` `mtools` |
| `qemu-system-x86_64` (testing) | `qemu-system-x86` | `qemu-system-x86` | `qemu-system-x86` |

### Build & Run

```sh
make              # builds lunarsnow.elf + initrd.tar
make run          # BIOS + QEMU with ISO + FAT32 disk
make run-uefi     # UEFI + QEMU (OVMF)
make run-uefi-gtk # UEFI + QEMU with GTK display
make iso          # generates bootable ISO (lunarsnow.iso)
make clean        # cleans objects
```

### UEFI

```sh
make run-uefi-gtk
```

Requires `edk2-ovmf` package.

### Real hardware

```sh
make usb DEVICE=/dev/sdX   # writes ISO to USB (destroys data!)
```

Boots via BIOS or UEFI. Requires VESA (or UEFI GOP) graphics.

## Structure

```
boot.s              → Multiboot2 header + entry point (x86_64 assembly)
linker.ld           → Linker script (loads at 1 MB)
kernel.c            → Init: PCI, framebuffer, RTC, ACPI, boot screen, event loop
├── fb.c / fb.h         → Framebuffer: pixel, rect, text, flip (double buffer)
├── input.c / input.h   → PS/2 keyboard + mouse
├── gui.c / gui.h       → Window manager, buttons, taskbar, clock, menu, cursor
├── apps.c / apps.h     → Terminal, calculator, msgbox, callbacks
├── progs.c / progs.h   → Program registry
├── config.h             → Centralized settings (version, name, etc.)
├── progs/              → Programs (each in its own .c)
│   ├── about.c         → About (CPU, RAM, uptime, display resolution)
│   ├── controlpanel.c  → Control Panel (CPU, display, version)
│   ├── filemgr.c       → File Manager (initrd + FAT disk)
│   ├── viewfile.c      → Text viewer (auto-size width)
│   ├── hello.c         → Hello World demo
│   ├── inputname.c     → Input Name demo
│   ├── notepad.c       → Notepad text editor
│   ├── msgboxdemo.c    → MessageBox demo
│   ├── snake.c         → Snake game
│   └── minesweeper.c   → Minesweeper game
├── disk.c / disk.h     → ATA PIO driver (LBA28)
├── fat.c / fat.h       → FAT32 driver (mount, read, iterate)
├── initrd/             → Files baked into initrd.tar
│   ├── boot.txt, meow.txt, hii.txt, ...
└── lunarsnow.h         → SDK umbrella (includes everything)
```

## Programs

| Program | File | Description |
|---|---|---|
| Terminal | built-in (`apps.c`) | Commands: `help`, `echo`, `cls`, `time`, `ver`, `shutdown`, `newwin` |
| Calculator | built-in (`apps.c`) | +, -, ×, ÷ |
| Notepad | `progs/notepad.c` | Text editor (72 col × 256 lines, scroll) |
| About | `progs/about.c` | CPU, vendor, RAM, uptime, resolution |
| Control Panel | `progs/controlpanel.c` | System info, version, display |
| File Manager | `progs/filemgr.c` | Browse initrd + FAT disk files |
| File Viewer | `progs/viewfile.c` | Auto-sized text viewer |
| Hello Demo | `progs/hello.c` | Minimal example |
| Input Name | `progs/inputname.c` | Keyboard input demo |
| Message Demo | `progs/msgboxdemo.c` | MessageBox demo |
| Snake | `progs/snake.c` | Snake game (arrow keys, score, adjustable speed) |
| Minesweeper | `progs/minesweeper.c` | Minesweeper (9×9, right-click / F key to flag) |

## FAT32 Disk

`make run` attaches `fat_disk.raw` (64 MB, FAT32) to QEMU via IDE.
The kernel mounts the first FAT32 partition and makes files available
through File Manager and File Viewer.

```sh
make fat_disk.raw   # creates disk with files from initrd/
bash create_fat_disk.sh   # or manually
```

## Shortcuts

| Key | Action |
|---|---|
| Super (Win) | Toggle Start menu |
| Tab | Navigate windows/buttons |
| Alt+F4 | Close active window |
| Escape | Close menu / sub-window |
| Enter/Space | Activate button / menu item |
| Arrows | Menu / file list / game navigation |
| Right-click | Toggle flag/dig mode in Minesweeper |
| F | Toggle flag/dig mode in Minesweeper (keyboard) |
| +/- | Increase/decrease Snake speed |

## How to create an app

1. Create `progs/my_app.c`:
```c
#include "../lunarsnow.h"

static void draw(int wi) {
    Win *w = &wins[wi];
    fb_txt(w->x + 10, w->y + 30, "My App!", C_TTT, w->bg);
}

void prog_my_app(void) {
    int wi = gui_wnew("My App", 100, 100, 260, 140);
    gui_wbtn(wi, "Close", 100, 80, 60, 26, app_close);
    wins[wi].draw = draw;
}
```

2. Register in `progs.c`:
```c
extern void prog_my_app(void);
Prog progs[] = { ..., {"My App", prog_my_app}, };
```

3. `make` — the Start menu updates automatically.
