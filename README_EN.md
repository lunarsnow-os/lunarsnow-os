# LunarSnow OS

A bare-metal 64-bit graphical OS that boots directly in QEMU (no host OS).

Architecture: **x86_64**, long mode, 2 MB pages.

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
make run-sdl          # QEMU + SDL2 display (recommended)
make run              # QEMU + default display
make run-uefi         # UEFI + QEMU (OVMF)
make iso              # generates bootable ISO (lunarsnow.iso)
make clean            # cleans objects
```

### UEFI

```sh
make run-uefi-sdl
```

Requires `edk2-ovmf` package.

### Real hardware

```sh
make usb DEVICE=/dev/sdX   # writes ISO to USB (destroys data!)
```

Boots via BIOS or UEFI. Requires VESA (or UEFI GOP) graphics.

## Structure

```
boot/               → Multiboot2 header + entry point (x86_64 assembly)
kernel/kernel.c     → Init: PCI, framebuffer, RTC, ACPI, boot screen, event loop
drv/                → Drivers
├── fb.c            → Framebuffer: pixel, rect, text, flip (double buffer)
├── input.c         → PS/2 keyboard + mouse
gui/                → GUI
├── gui.c           → Window manager, buttons, taskbar, clock, menu, cursor
├── apps.c          → Terminal, calculator, msgbox, callbacks
├── progs.c         → Program registry
fs/fs.c             → Unified filesystem (ATA PIO, partitions, FAT32, SnowFS)
vbe/                → VESA BIOS Extensions (video modes)
include/            → Central headers
├── lunarsnow.h     → SDK umbrella (includes everything)
├── config.h        → Centralized settings (version, name, etc.)
├── io.h            → Port I/O (inb/outb, inw/outw, inl/outl)
├── bmp.h           → BMP constants
├── fb.h, input.h, gui.h, apps.h, progs.h, fs.h
progs/              → Programs (each in its own .c)
│ ├── about.c       → About (CPU, RAM, uptime, resolution, initrd, logo)
│ ├── controlpanel.c→ Control Panel (mouse, display)
│ ├── filemgr.c     → File Manager (initrd + FAT32 + SnowFS)
│ ├── viewfile.c    → Text viewer (auto-size width)
│ ├── notepad.c     → Notepad with arrows, scroll, status bar
│ ├── paint.c       → Paint (mouse drawing, saves BMP to SnowFS)
│ ├── clock.c       → Analog Clock + Calendar
│ ├── bmpview.c     → BMP Viewer (any size, scaling)
│ ├── taskmgr.c     → Task Manager (RAM, CPU, disk, version)
│ ├── snake.c       → Snake game
│ ├── minesweeper.c → Minesweeper
│ ├── pong.c        → Pong vs CPU
│ ├── tetris.c      → Classic Tetris
│ ├── starfield.c   → 3D Starfield screensaver
│ └── inputname.c   → Input Name demo
initrd/             → Files baked into initrd.tar
tools/              → Host tools
├── mksnowfs.c      → Formats a SnowFS partition
linker.ld           → Linker script (loads at 1 MB)
Makefile
create_fat_disk.sh  → Creates 128MB disk image (FAT32 + SnowFS)
```

## Programs

| Program | File | Description |
|---|---|---|
| Terminal | built-in (`gui/apps.c`) | Commands: `help`, `echo`, `cls`, `time`, `ver`, `shutdown`, `reboot`, `exit` |
| Calculator | built-in (`gui/apps.c`) | +, -, ×, ÷, keyboard |
| Notepad | `progs/notepad.c` | Editor (72 col × 256 lines, arrows, Home/End, PgUp/PgDn, Del, scrollbar, status bar) |
| Paint | `progs/paint.c` | Mouse drawing, 12-color palette, Save/Load BMP via SnowFS |
| File Manager | `progs/filemgr.c` | Browse initrd + FAT32 + SnowFS, opens .txt in Notepad, New/Del |
| About | `progs/about.c` | CPU, vendor, RAM, uptime, resolution, initrd, LunarSnow logo |
| Control Panel | `progs/controlpanel.c` | Mouse, display (VBE) & power/battery |
| Clock | `progs/clock.c` | Analog clock + calendar |
| Task Manager | `progs/taskmgr.c` | RAM, CPU vendor, disk, version |
| BMP Viewer | `progs/bmpview.c` | Opens BMP from initrd/SnowFS/FAT with scaling |
| Snake | `progs/snake.c` | Snake game (arrows, score, speed +/-) |
| Minesweeper | `progs/minesweeper.c` | 9×9 Minesweeper (F to flag) |
| Pong | `progs/pong.c` | Pong vs CPU |
| Tetris | `progs/tetris.c` | Classic Tetris |
| Starfield | `progs/starfield.c` | 3D starfield screensaver |
| Input Name | `progs/inputname.c` | Text input demo |

## SnowFS (Custom Filesystem)

LunarSnow OS includes **SnowFS**, a custom filesystem (type 0xDA) with:
- 512B superblock, flat root directory (256 entries, names up to 55 chars)
- 32-bit FAT for cluster allocation (4 sectors/cluster)
- Operations: create, read, write, delete, iterate

The `fat_disk.raw` has two partitions:
- **FAT32** (LBA 2048, 32 MB) — compatibility
- **SnowFS** (LBA 67584, ~95 MB) — save Paint/Notepad files

File Manager lists all 3 sources (initrd, FAT32, SnowFS).

## Disk

```sh
make fat_disk.raw          # generates 128MB disk with FAT32 + SnowFS
bash create_fat_disk.sh    # or manually
```

## Battery

LunarSnow OS reads battery percentage via **SMBus/Smart Battery** (present on most Intel laptops). The indicator appears in the taskbar between app buttons and the clock. Colors:
- **Green** (>20%) — normal / charging (`~` symbol)
- **Orange** (10–20%) — warning
- **Red** (<10%) — critical

Control Panel → Power shows detailed status.

In VMs (VirtualBox, QEMU) the battery is **simulated** (cycles 80%→0%→100%) for visual testing.

## Shortcuts

| Key | Action |
|---|---|
| Super (Win) | Toggle Start menu |
| Tab | Navigate windows/buttons |
| Alt+F4 | Close active window |
| Escape | Close menu / sub-window |
| Enter/Space | Activate button / menu item |
| Arrows | Menu / file list / game navigation |
| S (Notepad) | Save file |
| F (Minesweeper) | Toggle flag/dig |
| +/- | Snake: speed; Tetris: rotation |

## How to create an app

### 1. Basic file

Create `progs/my_app.c`:

```c
#include "lunarsnow.h"

static void draw(int wi) {
    Win *w = &wins[wi];
    fb_txt(w->x + 10, w->y + 30, "My App!", C_TTT, w->bg);
}

void prog_my_app(void) {
    int wi = gui_wnew("My App", 100, 100, 260, 140);
    wins[wi].draw = draw;
}
```

### 2. Register

In `gui/progs.c`, add:

```c
extern void prog_my_app(void);
Prog progs[] = { ..., {"My App", prog_my_app}, };
```

`make` compiles automatically (wildcard in Makefile).

### 3. Available APIs

#### Window

```c
int wi = gui_wnew(title, x, y, width, height);
```

Content area starts at `y + 20`.

#### Buttons

```c
gui_wbtn(wi, "Text", x, y, width, height, callback);
```

#### Drawing

```c
fb_txt(x, y, "text", fg_color, bg_color);
fb_rect(x, y, width, height, color);
fb_pixel(x, y, color);
fb_border(x, y, width, height, color);
```

All coordinates are absolute (`w->x + offset`).

#### Keyboard

```c
static void key(int k) {
    if (k == '\n') { /* Enter */ }
    if (k == 8)     { /* Backspace */ }
    if (k >= 32 && k <= 126) { /* printable char */ }
}
wins[wi].on_key = key;
```

Arrows: `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT`, `KEY_HOME`, `KEY_END`, `KEY_PGUP`, `KEY_PGDN`, `KEY_DEL` (defined in `input.h`).

#### Mouse

```c
static void click(int wi) {
    int mx = mouse_x, my = mouse_y;
}
wins[wi].on_click = click;
```

For continuous drawing (e.g., Paint), use `gui_tick`:

```c
static void tick(void) {
    if (!(mouse_btn & 1)) return;
    int mx = mouse_x, my = mouse_y;
    need_render = 1;
}
gui_tick = tick;
```

#### Files (SnowFS / FAT32 / initrd)

```c
uint8_t *file_read(const char *name, uint32_t *size_out);
void file_iterate(void (*cb)(const char *name, uint32_t size));

int snowfs_write(const char *name, const uint8_t *data, uint32_t size);
int snowfs_read(const char *name, uint8_t *buf, uint32_t *size_out);
int snowfs_delete(const char *name);
int snowfs_iterate(void (*cb)(const char *name, uint32_t size));

int fat_read_file(const char *name, uint8_t *buf, uint32_t *size_out);
int fat_write_file(const char *name, const uint8_t *data, uint32_t size);
int fat_delete_file(const char *name);
void fat_iterate(void (*cb)(const char *name, uint32_t size));
```

#### Colors

| Constant | Value | Usage |
|---|---|---|
| `C_DSK` | `0x0F0F1C` | desktop background |
| `C_WBG` | `0x1E1E32` | window body background |
| `C_TAC` | `0x3C50A0` | active title bar / accents |
| `C_TIN` | `0x323246` | inactive title bar |
| `C_BDR` | `0x505078` | window border |
| `C_BN` | `0x327878` | normal button |
| `C_BF` | `0x4A9696` | focused button |
| `C_BT` | `0xC8C8D2` | button text |
| `C_LBL` | `0xB4B4BE` | label text |
| `C_TTT` | `0xE6E6F0` | title text (soft white) |
| `C_TBAR` | `0x16162A` | taskbar |
| `C_MBG` | `0x1E1E35` | menu background |
| `C_MFOC` | `0x3C50A0` | focused menu item |
