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
│   ├── about.c         → About (CPU, RAM, uptime, resolution, initrd, logo)
│   ├── controlpanel.c  → Control Panel (mouse, display)
│   ├── filemgr.c       → File Manager (initrd + FAT disk)
│   ├── viewfile.c      → Text viewer (auto-size width)
│   ├── inputname.c     → Input Name demo
│   ├── notepad.c       → Notepad text editor
│   ├── paint.c         → Paint (mouse drawing, 12-color palette)
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
| Terminal | built-in (`apps.c`) | Commands: `help`, `echo`, `cls`, `time`, `ver`, `shutdown`, `reboot`, `exit` |
| Calculator | built-in (`apps.c`) | +, -, ×, ÷, keyboard |
| Notepad | `progs/notepad.c` | Text editor (72 col × 256 lines, dark theme) |
| About | `progs/about.c` | CPU, vendor, RAM, uptime, resolution, initrd, LunarSnow logo |
| Control Panel | `progs/controlpanel.c` | Mouse & display settings |
| File Manager | `progs/filemgr.c` | Browse initrd + FAT disk files |
| File Viewer | `progs/viewfile.c` | Auto-sized text viewer |
| Input Name | `progs/inputname.c` | Keyboard input demo |
| Paint | `progs/paint.c` | Mouse drawing, 12-color palette |
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

### 1. Basic file

Create `progs/my_app.c`:

```c
#include "../lunarsnow.h"

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

In `progs.c`, add:

```c
extern void prog_my_app(void);
Prog progs[] = { ..., {"My App", prog_my_app}, };
```

`make` compiles automatically (wildcard in Makefile) and the Start menu picks it up.

### 3. Available APIs

#### Window (`gui.h`)

```c
int wi = gui_wnew(title, x, y, width, height);
```

Returns the window index. Content area starts at `y + 20`.

#### Buttons

```c
gui_wbtn(wi, "Text", x, y, width, height, callback);
```

The callback is a `void func(void)` function.

#### Drawing (`fb.h`)

```c
fb_txt(x, y, "text", fg_color, bg_color);
fb_chr(x, y, 'A', fg_color, bg_color);
fb_rect(x, y, width, height, color);
fb_pixel(x, y, color);
fb_border(x, y, width, height, color);  // outline only
```

All coordinates are absolute on screen (`w->x + offset`).

#### Keyboard

```c
static void key(int k) {
    if (k == '\n') { /* Enter */ }
    if (k == 8)     { /* Backspace */ }
    if (k >= 32 && k <= 126) { /* printable char */ }
}
wins[wi].on_key = key;
```

Arrows: `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT` (defined in `input.h`).

#### Mouse

```c
static void click(int wi) {
    int mx = mouse_x, my = mouse_y;  // globals
    // ...
}
wins[wi].on_click = click;
```

For continuous drawing (e.g., Paint), use `gui_tick`:

```c
static void tick(void) {
    if (!(mouse_btn & 1)) return;  // left button
    int mx = mouse_x, my = mouse_y;
    // draw...
    need_render = 1;
}
gui_tick = tick;
```

#### Close

```c
static void on_close(int wi) { gui_tick = 0; /* cleanup */ }
wins[wi].on_close = on_close;
```

#### Per-window data

```c
wins[wi].userdata = pointer;  // void* — window-specific data
```

### 4. Colors (`gui.h`)

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

### 5. Full example (with keyboard and close)

```c
#include "../lunarsnow.h"

static char text[64];
static int len;

static void draw(int wi) {
    Win *w = &wins[wi];
    fb_txt(w->x + 10, w->y + 28, "Type something:", C_TTT, w->bg);
    fb_rect(w->x + 10, w->y + 48, 200, 18, C_TIN);
    if (len > 0)
        fb_txt(w->x + 12, w->y + 49, text, C_TTT, C_TIN);
}

static void key(int k) {
    if (k == 8 && len > 0) text[--len] = 0;
    if (k >= 32 && k <= 126 && len < 63) text[len++] = k;
    text[len] = 0;
}

void prog_my_app(void) {
    int wi = gui_wnew("My App", 100, 100, 300, 140);
    wins[wi].draw = draw;
    wins[wi].on_key = key;
    len = 0; text[0] = 0;
}
```
