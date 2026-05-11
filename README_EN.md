# LunarSnow OS

A bare-metal graphical system that boots directly in QEMU (no OS).

## Building

### Prerequisites

| Program | Debian/Ubuntu | Arch | Fedora |
|---|---|---|---|
| `gcc` + `ld` (i386) | `gcc` `binutils` | `gcc` `binutils` | `gcc` `binutils` |
| `as` (assembler) | `binutils` | `binutils` | `binutils` |
| `grub2-mkrescue` | `grub2-common` `xorriso` | `grub` `libisoburn` | `grub2-tools` `xorriso` |
| `qemu-system-x86_64` (testing) | `qemu-system-x86` | `qemu-system-x86` | `qemu-system-x86` |

### Build & Run

```sh
make            # builds lunarsnow.elf (multiboot kernel)
make run        # QEMU with -kernel
make iso        # generates bootable ISO (lunarsnow.iso)
make run-iso    # QEMU with ISO
make clean      # cleans object files
```

### Real hardware

Write `lunarsnow.iso` to a USB/CD and boot via BIOS. Requires a VESA BIOS
capable graphics card (handled by GRUB) — works on Pentium II, III, IV and up.

## Structure

```
boot.s              → Multiboot header + entry point (assembly)
linker.ld           → Linker script (loads at 1 MB)
kernel.c            → Initialization: PCI, VBE, CMOS, boot screen, event loop
├── fb.c / fb.h         → Framebuffer: pixel, rect, text, flip (double buffer)
├── input.c / input.h   → Keyboard + PS/2 mouse
├── gui.c / gui.h       → Window manager, buttons, taskbar, clock, menu, cursor
├── apps.c / apps.h     → Infrastructure: terminal, calculator, msgbox, callbacks
├── progs.c / progs.h   → External program registry
├── progs/              → Programs (each in its own .c)
│   ├── about.c         → About window (CPU, RAM, uptime)
│   ├── hello.c         → Hello World demo
│   ├── inputname.c     → Input Name demo
│   ├── notepad.c       → Notepad text editor
│   └── msgboxdemo.c    → MessageBox demo
└── lunarsnow.h         → SDK umbrella (includes everything)
```

## SDK API

### Drawing (`fb.h`)
```c
fb_pixel(x, y, color)              → draw a pixel
fb_rect(x, y, w, h, color)         → filled rectangle
fb_chr(x, y, char, fg, bg)         → 8×16 character
fb_txt(x, y, "text", fg, bg)       → text string
fb_border(x, y, w, h, color)       → 1px border
fb_clear(color)                     → clear screen
fb_flip()                           → show frame (double buffer)
```

### Input (`input.h`)
```c
kb_poll()          → read keyboard/mouse (call once per frame)
kb_pop()           → return next key (-1 if empty)
mouse_init()       → initialize PS/2 mouse

mouse_x, mouse_y   → mouse position
mouse_btn          → button state (bit 0 = left)
```

### Windows (`gui.h`)
```c
gui_wnew("title", x, y, w, h)              → create window, return index
gui_wbtn(win, "text", x, y, w, h, cb)      → add button
gui_wclose(win)                              → close window
gui_render()                                 → draw everything
gui_mouse_click()                            → handle mouse click
gui_set_dirty()                              → force full redraw

wins[win].bg = color;                        → set background color
wins[win].draw = my_func;                    → draw callback (receives win)
wins[win].on_key = my_func;                  → keyboard callback (receives key)
```

### MessageBox (`apps.h`)
```c
msgbox("title", "message");    → open popup window with OK button
```

## Current Programs

| Program | File | Description |
|---|---|---|
| Terminal | built-in (`apps.c`) | Command line with `help`, `echo`, `about`, `cls`, `time`, `ver`, `shutdown`, `newwin` |
| Calculator | built-in (`apps.c`) | Basic operations (+, -, ×, ÷) |
| Notepad | `progs/notepad.c` | Text editor (72 col × 256 lines, auto scroll) |
| About | `progs/about.c` | System info (CPU, vendor, RAM, uptime) |
| Hello Demo | `progs/hello.c` | Minimal example |
| Input Name | `progs/inputname.c` | Keyboard input example |
| Message Demo | `progs/msgboxdemo.c` | MessageBox demo |

## How to create an external app

### 1. Create `progs/my_app.c`

```c
#include "../lunarsnow.h"

static void draw(int wi)
{
    Win *w = &wins[wi];
    fb_txt(w->x + 10, w->y + 30, "My App!", C_TTT, w->bg);
}

void prog_my_app(void)
{
    int wi = gui_wnew("My App", 100, 100, 260, 140);
    gui_wbtn(wi, "Close", 100, 80, 60, 26, app_close);
    wins[wi].draw = draw;
}
```

### 2. Register in `progs.c`

```c
extern void prog_my_app(void);

Prog progs[] = {
    {"Hello Demo", prog_hello},
    {"My App",     prog_my_app},
};
```

### 3. `make` and you're done

The Start menu updates automatically.

### Apps with keyboard

```c
static void my_key(int k)
{
    if (k >= 32 && k <= 126) { /* typing */ }
    if (k == '\n') { /* execute */ }
}

void prog_my_app(void)
{
    int wi = gui_wnew("App", 100, 100, 400, 300);
    wins[wi].on_key = my_key;
    wins[wi].draw = my_draw;
}
```

The kernel routes keys automatically to the active window (`act`).
