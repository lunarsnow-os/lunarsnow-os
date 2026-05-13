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
├── config.h             → Definições centralizadas (versão, nome, etc.)
├── progs/              → Programas (cada um no seu .c)
│   ├── about.c         → About (CPU, RAM, uptime, resolução, initrd)
│   ├── controlpanel.c  → Painel de Controlo (rato, ecrã)
│   ├── filemgr.c       → Gestor de Ficheiros (initrd + disco FAT)
│   ├── viewfile.c      → Visualizador de texto (auto-size)
│   ├── inputname.c     → Input Name demo
│   ├── notepad.c       → Bloco de Notas
│   ├── paint.c         → Paint (desenho com rato)
│   ├── snake.c         → Jogo Snake (cobra)
│   └── minesweeper.c   → Jogo Campo Minado
├── disk.c / disk.h     → Driver ATA PIO (LBA28)
├── fat.c / fat.h       → Driver FAT32 (mount, read, iterate)
├── initrd/             → Ficheiros para o initrd.tar
│   ├── boot.txt, meow.txt, hii.txt, ...
└── lunarsnow.h         → SDK umbrella (inclui tudo)
```

## Programas

| Programa | Ficheiro | Descrição |
|---|---|---|
| Terminal | built-in (`apps.c`) | Comandos: `help`, `echo`, `cls`, `time`, `ver`, `shutdown`, `reboot`, `exit` |
| Calculadora | built-in (`apps.c`) | +, -, ×, ÷, teclado |
| Notepad | `progs/notepad.c` | Editor (72 col × 256 linhas, tema escuro) |
| About | `progs/about.c` | CPU, vendor, RAM, uptime, resolução, initrd, logo LunarSnow |
| Control Panel | `progs/controlpanel.c` | Definições do rato e ecrã |
| File Manager | `progs/filemgr.c` | Lista ficheiros do initrd + disco FAT |
| File Viewer | `progs/viewfile.c` | Visualizador de texto com auto-width |
| Input Name | `progs/inputname.c` | Exemplo com teclado |
| Paint | `progs/paint.c` | Desenho com rato, paleta de 12 cores |
| Snake | `progs/snake.c` | Jogo da cobra (setas, score, velocidade ajustável) |
| Minesweeper | `progs/minesweeper.c` | Campo minado (9×9, clique direito/tecla F para bandeira) |

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
| Setas | Navegação no menu / lista de ficheiros / jogos |
| Clique direito | Alterna modo bandeira/detetar no Minesweeper |
| F | Alterna modo bandeira/detetar no Minesweeper (teclado) |
| +/- | Aumenta/diminui velocidade no Snake |

## Como criar uma app

### 1. Ficheiro básico

Cria `progs/minha_app.c`:

```c
#include "../lunarsnow.h"

static void draw(int wi) {
    Win *w = &wins[wi];
    fb_txt(w->x + 10, w->y + 30, "Minha App!", C_TTT, w->bg);
}

void prog_minha_app(void) {
    int wi = gui_wnew("Minha App", 100, 100, 260, 140);
    wins[wi].draw = draw;
}
```

### 2. Registar

Em `progs.c`, adiciona:

```c
extern void prog_minha_app(void);
Prog progs[] = { ..., {"Minha App", prog_minha_app}, };
```

O `make` compila automaticamente (wildcard no Makefile) e o menu Start atualiza.

### 3. APIs disponíveis

#### Janela (`gui.h`)

```c
int wi = gui_wnew(título, x, y, largura, altura);
```

Devolve o índice da janela. A área de conteúdo começa em `y + 20`.

#### Botões

```c
gui_wbtn(wi, "Texto", x, y, largura, altura, callback);
```

O callback é uma função `void func(void)`.

#### Desenho (`fb.h`)

```c
fb_txt(x, y, "texto", cor_frente, cor_fundo);
fb_chr(x, y, 'A', cor_frente, cor_fundo);
fb_rect(x, y, largura, altura, cor);
fb_pixel(x, y, cor);
fb_border(x, y, largura, altura, cor);  // rectângulo só com contorno
```

Todas as coordenadas são absolutas no ecrã (`w->x + offset`).

#### Teclado

```c
static void key(int k) {
    if (k == '\n') { /* Enter */ }
    if (k == 8)     { /* Backspace */ }
    if (k >= 32 && k <= 126) { /* caracter imprimível */ }
}
wins[wi].on_key = key;
```

Setas: `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT` (definidos em `input.h`).

#### Rato

```c
static void click(int wi) {
    int mx = mouse_x, my = mouse_y;  // globais
    // ...
}
wins[wi].on_click = click;
```

Para desenho contínuo (ex: Paint), usa `gui_tick`:

```c
static void tick(void) {
    if (!(mouse_btn & 1)) return;  // botão esquerdo
    int mx = mouse_x, my = mouse_y;
    // desenha...
    need_render = 1;
}
gui_tick = tick;
```

#### Fechar

```c
static void on_close(int wi) { gui_tick = 0; /* limpar recursos */ }
wins[wi].on_close = on_close;
```

#### Dados por janela

```c
wins[wi].userdata = apontador;  // void* — dados específicos da janela
```

### 4. Cores (`gui.h`)

| Constante | Valor | Uso |
|---|---|---|
| `C_DSK` | `0x0F0F1C` | fundo do ambiente de trabalho |
| `C_WBG` | `0x1E1E32` | fundo de janela |
| `C_TAC` | `0x3C50A0` | barra de título ativa / acentos |
| `C_TIN` | `0x323246` | barra de título inativa |
| `C_BDR` | `0x505078` | borda de janela |
| `C_BN` | `0x327878` | botão normal |
| `C_BF` | `0x4A9696` | botão com foco |
| `C_BT` | `0xC8C8D2` | texto de botão |
| `C_LBL` | `0xB4B4BE` | texto de label |
| `C_TTT` | `0xE6E6F0` | texto de título (branco suave) |
| `C_TBAR` | `0x16162A` | barra de tarefas |
| `C_MBG` | `0x1E1E35` | fundo do menu |
| `C_MFOC` | `0x3C50A0` | item focado no menu |

### 5. Exemplo completo (com teclado e fechar)

```c
#include "../lunarsnow.h"

static char texto[64];
static int len;

static void draw(int wi) {
    Win *w = &wins[wi];
    fb_txt(w->x + 10, w->y + 28, "Escreve algo:", C_TTT, w->bg);
    fb_rect(w->x + 10, w->y + 48, 200, 18, C_TIN);
    if (len > 0)
        fb_txt(w->x + 12, w->y + 49, texto, C_TTT, C_TIN);
}

static void key(int k) {
    if (k == 8 && len > 0) texto[--len] = 0;
    if (k >= 32 && k <= 126 && len < 63) texto[len++] = k;
    texto[len] = 0;
}

void prog_minha_app(void) {
    int wi = gui_wnew("Minha App", 100, 100, 300, 140);
    wins[wi].draw = draw;
    wins[wi].on_key = key;
    len = 0; texto[0] = 0;
}
```
