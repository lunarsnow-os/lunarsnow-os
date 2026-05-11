# LunarSnow OS

Sistema gráfico bare-metal que boota diretamente no QEMU (sem SO).

## Compilar

### Pré-requisitos

| Programa | Pacote (Debian/Ubuntu) | Pacote (Arch) | Pacote (Fedora) |
|---|---|---|
| `gcc` + `ld` (i386) | `gcc` `binutils` | `gcc` `binutils` | `gcc` `binutils` |
| `as` (assembler) | `binutils` | `binutils` | `binutils` |
| `grub2-mkrescue` | `grub2-common` `xorriso` `mtools` | `grub` `libisoburn` `mtools` | `grub2-tools` `xorriso` `mtools` |
| `qemu-system-x86_64` (teste) | `qemu-system-x86` | `qemu-system-x86` | `qemu-system-x86` |

### Build & Run

```sh
make            # compila lunarsnow.elf (kernel multiboot)
make run        # QEMU com -kernel
make iso        # gera ISO bootável (lunarsnow.iso)
make run-iso    # QEMU com ISO
make clean      # limpa objetos
```

### Hardware real

Grava `lunarsnow.iso` numa USB/CD e boota via BIOS. Requer placa gráfica com
VESA BIOS (suportada por GRUB) — funciona em Pentium II, III, IV e superior.

## Estrutura

```
boot.s              → Multiboot header + entry point (assembly)
linker.ld           → Linker script (carrega em 1 MB)
kernel.c            → Inicialização: PCI, VBE, CMOS, boot screen, event loop
├── fb.c / fb.h         → Framebuffer: pixel, rect, texto, flip (double buffer)
├── input.c / input.h   → Teclado + rato PS/2
├── gui.c / gui.h       → Gestor de janelas, botões, taskbar, relógio, menu, cursor
├── apps.c / apps.h     → Infraestrutura: terminal, calculadora, msgbox, callbacks
├── progs.c / progs.h   → Registo de programas externos
├── progs/              → Programas (cada um no seu .c)
│   ├── about.c         → Janela About (CPU, RAM, uptime)
│   ├── hello.c         → Hello World demo
│   ├── inputname.c     → Input Name demo
│   ├── notepad.c       → Bloco de Notas
│   └── msgboxdemo.c    → Demonstração de MessageBox
└── lunarsnow.h         → SDK umbrella (inclui tudo)
```

## API SDK

### Desenho (`fb.h`)
```c
fb_pixel(x, y, cor)               → pinta um pixel
fb_rect(x, y, w, h, cor)          → retângulo preenchido
fb_chr(x, y, char, fg, bg)        → caractere 8×16
fb_txt(x, y, "texto", fg, bg)     → texto
fb_border(x, y, w, h, cor)        → borda de 1px
fb_clear(cor)                      → limpa ecrã
fb_flip()                          → mostra o frame (double buffer)
```

### Input (`input.h`)
```c
kb_poll()          → lê teclado/rato (chamar 1× por frame)
kb_pop()           → devolve próxima tecla (-1 se vazio)
mouse_init()       → inicializa rato PS/2

mouse_x, mouse_y   → posição do rato
mouse_btn          → estado dos botões (bit 0 = esquerdo)
```

### Janelas (`gui.h`)
```c
gui_wnew("título", x, y, w, h)            → cria janela, devolve índice
gui_wbtn(win, "texto", x, y, w, h, cb)    → adiciona botão
gui_wclose(win)                            → fecha janela
gui_render()                               → desenha tudo
gui_mouse_click()                          → processa clique do rato
gui_set_dirty()                            → força redesenho total

wins[win].bg = cor;                        → muda cor de fundo
wins[win].draw = minha_func;               → callback de desenho (recebe win)
wins[win].on_key = minha_func;             → callback de teclado (recebe key)
```

### MessageBox (`apps.h`)
```c
msgbox("título", "mensagem");    → abre janela popup com OK
```

## Programas Atuais

| Programa | Ficheiro | Descrição |
|---|---|---|
| Terminal | built-in (`apps.c`) | Linha de comandos com `help`, `echo`, `about`, `cls`, `time`, `ver`, `shutdown`, `newwin` |
| Calculadora | built-in (`apps.c`) | Operações básicas (+, -, ×, ÷) |
| Notepad | `progs/notepad.c` | Editor de texto (72 col × 256 linhas, scroll automático) |
| About | `progs/about.c` | Informações do sistema (CPU, vendor, RAM, uptime) |
| Hello Demo | `progs/hello.c` | Exemplo mínimo |
| Input Name | `progs/inputname.c` | Exemplo com teclado |
| Message Demo | `progs/msgboxdemo.c` | Demonstração de MessageBox |

## Como criar uma app externa

### 1. Cria `progs/minha_app.c`

```c
#include "../lunarsnow.h"

static void draw(int wi)
{
    Win *w = &wins[wi];
    fb_txt(w->x + 10, w->y + 30, "Minha App!", C_TTT, w->bg);
}

void prog_minha_app(void)
{
    int wi = gui_wnew("Minha App", 100, 100, 260, 140);
    gui_wbtn(wi, "Fechar", 100, 80, 60, 26, app_close);
    wins[wi].draw = draw;
}
```

### 2. Regista em `progs.c`

```c
extern void prog_minha_app(void);

Prog progs[] = {
    {"Hello Demo",   prog_hello},
    {"Minha App",    prog_minha_app},
};
```

### 3. `make` e ta pronto

O menu (Start) atualiza-se automaticamente.

### Apps com teclado

```c
static void minha_key(int k)
{
    if (k >= 32 && k <= 126) { /* digitação */ }
    if (k == '\n') { /* executar */ }
}

void prog_minha_app(void)
{
    int wi = gui_wnew("App", 100, 100, 400, 300);
    wins[wi].on_key = minha_key;
    wins[wi].draw = minha_draw;
}
```

O kernel roteia as teclas automaticamente para a janela ativa (`act`).
