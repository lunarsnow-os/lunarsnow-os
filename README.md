# LunarSnow OS

Sistema gráfico bare-metal 64-bit que boota diretamente no QEMU (sem SO).

Arquitetura: **x86_64**, modo longo, páginas de 1 GB.

## Compilar

### Pré-requisitos

| Programa | Pacote (Debian/Ubuntu) | Pacote (Arch) | Pacote (Fedora) |
|---|---|---|---|
| `gcc` + `ld` (x86_64) | `gcc` `binutils` | `gcc` `binutils` | `gcc` `binutils` |
| `as` (assembler) | `binutils` | `binutils` | `binutils` |
| `grub-mkrescue` | `grub2-common` `xorriso` `mtools` | `grub` `libisoburn` `mtools` | `grub2-tools` `xorriso` `mtools` |
| `qemu-system-x86_64` (teste) | `qemu-system-x86` | `qemu-system-x86` | `qemu-system-x86` |

### Build & Run

```sh
make              # compila lunarsnow.elf + initrd.tar
make run          # BIOS + QEMU com ISO + disco FAT32
make run-uefi     # UEFI + QEMU (OVMF)
make run-uefi-gtk # UEFI + QEMU com display GTK
make iso          # gera ISO bootável (lunarsnow.iso)
make clean        # limpa objetos
```

### UEFI

```sh
make run-uefi-gtk
```

Requer `edk2-ovmf` instalado.

### Hardware real

```sh
make usb DEVICE=/dev/sdX   # grava ISO na USB (cuidado: apaga tudo!)
```

Boota via BIOS ou UEFI. Requer placa gráfica VESA (ou UEFI GOP).

## Estrutura

```
boot.s              → Multiboot2 header + entry point (assembly x86_64)
linker.ld           → Linker script (carrega em 1 MB)
kernel.c            → Inicialização: PCI, framebuffer, RTC, ACPI, boot screen, event loop
├── fb.c / fb.h         → Framebuffer: pixel, rect, texto, flip (double buffer)
├── input.c / input.h   → Teclado + rato PS/2
├── gui.c / gui.h       → Gestor de janelas, botões, taskbar, relógio, menu, cursor
├── apps.c / apps.h     → Infraestrutura: terminal, calculadora, msgbox, callbacks
├── progs.c / progs.h   → Registo de programas
├── progs/              → Programas (cada um no seu .c)
│   ├── about.c         → About (CPU, RAM, uptime, resolução)
│   ├── controlpanel.c  → Painel de Controlo (CPU, vídeo, versão)
│   ├── filemgr.c       → Gestor de Ficheiros (initrd + disco FAT)
│   ├── viewfile.c      → Visualizador de texto (auto-size)
│   ├── hello.c         → Hello World demo
│   ├── inputname.c     → Input Name demo
│   ├── notepad.c       → Bloco de Notas
│   └── msgboxdemo.c    → Demonstração de MessageBox
├── disk.c / disk.h     → Driver ATA PIO (LBA28)
├── fat.c / fat.h       → Driver FAT32 (mount, read, iterate)
├── initrd/             → Ficheiros para o initrd.tar
│   ├── boot.txt, meow.txt, hii.txt, ...
└── lunarsnow.h         → SDK umbrella (inclui tudo)
```

## Programas

| Programa | Ficheiro | Descrição |
|---|---|---|
| Terminal | built-in (`apps.c`) | Comandos: `help`, `echo`, `cls`, `time`, `ver`, `shutdown`, `newwin` |
| Calculadora | built-in (`apps.c`) | +, -, ×, ÷ |
| Notepad | `progs/notepad.c` | Editor (72 col × 256 linhas, scroll) |
| About | `progs/about.c` | CPU, vendor, RAM, uptime, resolução |
| Control Panel | `progs/controlpanel.c` | Info do sistema, versão, ecrã |
| File Manager | `progs/filemgr.c` | Lista ficheiros do initrd + disco FAT |
| File Viewer | `progs/viewfile.c` | Visualizador de texto com auto-width |
| Hello Demo | `progs/hello.c` | Exemplo mínimo |
| Input Name | `progs/inputname.c` | Exemplo com teclado |
| Message Demo | `progs/msgboxdemo.c` | Demonstração de MessageBox |

## Disco FAT32

O `make run` anexa `fat_disk.raw` (64 MB, FAT32) ao QEMU via IDE.
O kernel monta a primeira partição FAT32 e disponibiliza os ficheiros
no File Manager e no File Viewer.

```sh
make fat_disk.raw   # gera o disco com os ficheiros de initrd/
bash create_fat_disk.sh   # ou manualmente
```

## Atalhos

| Tecla | Ação |
|---|---|
| Super (Win) | Abre/fecha menu Start |
| Tab | Navega entre janelas/botões |
| Alt+F4 | Fecha janela ativa |
| Escape | Fecha menu / sub-janela |
| Enter/Space | Ativa botão / entrada do menu |
| Setas | Navegação no menu / lista de ficheiros |

## Como criar uma app

1. Cria `progs/minha_app.c`:
```c
#include "../lunarsnow.h"

static void draw(int wi) {
    Win *w = &wins[wi];
    fb_txt(w->x + 10, w->y + 30, "Minha App!", C_TTT, w->bg);
}

void prog_minha_app(void) {
    int wi = gui_wnew("Minha App", 100, 100, 260, 140);
    gui_wbtn(wi, "Fechar", 100, 80, 60, 26, app_close);
    wins[wi].draw = draw;
}
```

2. Regista em `progs.c`:
```c
extern void prog_minha_app(void);
Prog progs[] = { ..., {"Minha App", prog_minha_app}, };
```

3. `make` — o menu Start atualiza automaticamente.
