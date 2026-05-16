# LunarSnow OS

Sistema gráfico bare-metal 64-bit que boota diretamente no QEMU (sem SO).

Arquitetura: **x86_64**, modo longo, páginas de 2 MB.

## Compilar

### Pré-requisitos

| Programa | Debian/Ubuntu | Arch | Fedora |
|---|---|---|---|
| `gcc` + `ld` (x86_64) | `gcc` `binutils` | `gcc` `binutils` | `gcc` `binutils` |
| `as` (assembler) | `binutils` | `binutils` | `binutils` |
| `grub-mkrescue` | `grub2-common` `xorriso` `mtools` | `grub` `libisoburn` `mtools` | `grub2-tools` `xorriso` `mtools` |
| `qemu-system-x86_64` (teste) | `qemu-system-x86` | `qemu-system-x86` | `qemu-system-x86` |

### Build & Run

```sh
make run-sdl          # QEMU + display SDL2 (recomendado)
make run              # QEMU + display padrão
make run-uefi         # UEFI + QEMU (OVMF)
make iso              # gera ISO bootável (lunarsnow.iso)
make clean            # limpa objetos
```

### UEFI

```sh
make run-uefi-sdl
```

Requer `edk2-ovmf` instalado.

### Hardware real

```sh
make usb DEVICE=/dev/sdX   # grava ISO na USB (cuidado: apaga tudo!)
```

Boota via BIOS ou UEFI. Requer placa gráfica VESA (ou UEFI GOP).

## Estrutura

```
boot/               → Multiboot2 header + entry point (assembly x86_64)
kernel/kernel.c     → Inicialização: PCI, framebuffer, RTC, ACPI, boot screen, event loop
drv/                → Drivers
├── fb.c            → Framebuffer: pixel, rect, texto, flip (double buffer)
├── input.c         → Teclado + rato PS/2
gui/                → Interface gráfica
├── gui.c           → Gestor de janelas (minimizar _), botões, taskbar, menu Start, cursor
├── apps.c          → Terminal, calculadora, caixas de diálogo (msgbox), callbacks
├── progs.c         → Registo de programas
fs/fs.c             → Sistema de ficheiros unificado (ATA PIO, partições, FAT32, SnowFS)
vbe/                → VESA BIOS Extensions (modos de vídeo)
include/            → Headers centrais
├── lunarsnow.h     → SDK umbrella (inclui tudo)
├── config.h        → Definições centralizadas (versão, nome, etc.)
├── io.h            → Port I/O (inb/outb, inw/outw, inl/outl)
├── bmp.h           → Constantes BMP
├── fb.h, input.h, gui.h, apps.h, progs.h, fs.h
progs/              → Programas (cada um no seu .c)
│ ├── about.c       → About (CPU, RAM, uptime, resolução, initrd, logo)
│ ├── controlpanel.c→ Painel de Controlo (rato, ecrã)
│ ├── filemgr.c     → Gestor de Ficheiros (initrd + FAT32 + SnowFS)
│ ├── viewfile.c    → Visualizador de texto (auto-size)
│ ├── notepad.c     → Bloco de Notas com setas, scroll, status bar
│ ├── paint.c       → Paint (desenho com rato, guarda BMP no SnowFS)
│ ├── clock.c       → Relógio + Calendário
│ ├── bmpview.c     → Visualizador BMP (qualquer tamanho, scaling)
│ ├── taskmgr.c     → Gestor de Tarefas (RAM, CPU, disco)
│ ├── snake.c       → Jogo Snake
│ ├── minesweeper.c → Campo Minado
│ ├── pong.c        → Pong
│ ├── tetris.c      → Tetris
│ ├── starfield.c   → Starfield screensaver
│ └── inputname.c   → Input Name demo
initrd/             → Ficheiros para o initrd.tar
tools/              → Ferramentas host
├── mksnowfs.c      → Formata partição SnowFS
linker.ld           → Linker script (carrega em 1 MB)
Makefile
create_fat_disk.sh  → Cria imagem de disco 128MB (FAT32 + SnowFS)
```

## Programas

| Programa | Ficheiro | Descrição |
|---|---|---|
| Terminal | built-in (`gui/apps.c`) | Comandos: `help`, `echo`, `cls`, `time`, `ver`, `shutdown`, `reboot`, `exit` |
| Calculadora | built-in (`gui/apps.c`) | +, -, ×, ÷, teclado |
| Notepad | `progs/notepad.c` | Editor (72 col × 256 linhas, setas, Home/End, PgUp/PgDn, Delete, scrollbar, status bar) |
| Paint | `progs/paint.c` | Desenho com rato, paleta 12 cores, Save/Load BMP via SnowFS |
| File Manager | `progs/filemgr.c` | Navega initrd + FAT32 + SnowFS, abre .txt no Notepad, New/Del |
| About | `progs/about.c` | CPU, vendor, RAM, uptime, resolução, initrd, logo LunarSnow |
| Control Panel | `progs/controlpanel.c` | Rato (velocidade slide), ecrã (VBE), energia/bateria, CPU, RAM, uptime |
| Clock | `progs/clock.c` | Relógio analógico + calendário |
| Task Manager | `progs/taskmgr.c` | RAM, CPU vendor, disco, versão |
| BMP Viewer | `progs/bmpview.c` | Abre BMP de initrd/SnowFS/FAT com scaling |
| Snake | `progs/snake.c` | Jogo da cobra (setas, score, velocidade +/-) |
| Minesweeper | `progs/minesweeper.c` | Campo minado 9×9 (F para bandeira) |
| Pong | `progs/pong.c` | Pong vs CPU |
| Tetris | `progs/tetris.c` | Tetris clássico |
| Starfield | `progs/starfield.c` | Screensaver 3D starfield |
| Input Name | `progs/inputname.c` | Exemplo de input de texto |

## SnowFS (Sistema de Ficheiros Próprio)

O LunarSnow OS inclui o **SnowFS**, um sistema de ficheiros personalizado (tipo 0xDA) com:
- Superbloco 512B, diretório raiz plano (256 entradas, nomes até 55 chars)
- FAT 32-bit para alocação de clusters (4 sectores/cluster)
- Operações: criar, ler, escrever, apagar, listar

O disco `fat_disk.raw` tem duas partições:
- **FAT32** (LBA 2048, 32 MB) — compatibilidade
- **SnowFS** (LBA 67584, ~95 MB) — para salvar ficheiros do Paint, Notepad, etc.

O File Manager lista as 3 fontes (initrd, FAT32, SnowFS).

## Disco

```sh
make fat_disk.raw          # gera disco 128MB com FAT32 + SnowFS
bash create_fat_disk.sh    # ou manualmente
```

## Bateria

O LunarSnow OS lê a percentagem da bateria via **SMBus/Smart Battery** (presente na maioria dos portáteis Intel). O indicador aparece na taskbar entre os botões de janela e o relógio. Cores:
- **Verde** (>20%) — normal / a carregar (símbolo `~`)
- **Laranja** (10–20%) — atenção
- **Vermelho** (<10%) — crítico

Painel de Controlo → Power mostra estado detalhado.

Em VMs (VirtualBox, QEMU) a bateria é **simulada** (cicla 80%→0%→100%) para teste visual.

## Atalhos

| Tecla | Ação |
|---|---|
| Super (Win) | Abre/fecha menu Start |
| Tab | Navega entre janelas/botões |
| Botão _ | Minimiza janela ativa |
| Clicar taskbar (janela ativa) | Minimiza |
| Clicar taskbar (minimizada) | Restaura |
| Alt+F4 | Fecha janela ativa |
| Escape | Fecha menu / sub-janela |
| Enter/Space | Ativa botão / entrada do menu |
| Setas | Navegação no menu / lista / jogos |
| S (no Notepad) | Salva ficheiro |
| F (Minesweeper) | Alterna bandeira/detetar |
| +/- | Snake: velocidade; Tetris: rotação |

## Como criar uma app

### 1. Ficheiro básico

Cria `progs/minha_app.c`:

```c
#include "lunarsnow.h"

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

Em `gui/progs.c`, adiciona:

```c
extern void prog_minha_app(void);
Prog progs[] = { ..., {"Minha App", prog_minha_app}, };
```

O `make` compila automaticamente (wildcard no Makefile).

### 3. APIs disponíveis

#### Janela

```c
int wi = gui_wnew(título, x, y, largura, altura);
```

A área de conteúdo começa em `y + 20`.

#### Botões

```c
gui_wbtn(wi, "Texto", x, y, largura, altura, callback);
```

#### Desenho

```c
fb_txt(x, y, "texto", cor_frente, cor_fundo);
fb_rect(x, y, largura, altura, cor);
fb_pixel(x, y, cor);
fb_border(x, y, largura, altura, cor);
```

Coordenadas absolutas no ecrã (`w->x + offset`).

#### Teclado

```c
static void key(int k) {
    if (k == '\n') { /* Enter */ }
    if (k == 8)     { /* Backspace */ }
    if (k >= 32 && k <= 126) { /* caracter imprimível */ }
}
wins[wi].on_key = key;
```

Setas: `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT`, `KEY_HOME`, `KEY_END`, `KEY_PGUP`, `KEY_PGDN`, `KEY_DEL` (definidos em `input.h`).

#### Rato

```c
static void click(int wi) {
    int mx = mouse_x, my = mouse_y;
}
wins[wi].on_click = click;
```

Para desenho contínuo (ex: Paint), usa `gui_tick`:

```c
static void tick(void) {
    if (!(mouse_btn & 1)) return;
    int mx = mouse_x, my = mouse_y;
    need_render = 1;
}
gui_tick = tick;
```

#### Ficheiros (SnowFS / FAT32 / initrd)

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

#### Cores

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
