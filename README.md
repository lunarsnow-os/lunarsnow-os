# LunarSnow OS

Sistema gráfico bare-metal que boota diretamente no QEMU (sem SO).

## Estrutura

```
boot.s              → Multiboot header + entry point (assembly)
linker.ld           → Linker script (carrega em 1 MB)
kernel.c            → Inicialização: PCI, VBE, CMOS, boot screen, event loop
├── fb.c / fb.h         → Framebuffer: pixel, rect, texto, flip (double buffer)
├── input.c / input.h   → Teclado + rato PS/2
├── gui.c / gui.h       → Gestor de janelas, botões, taskbar, menu, cursor
├── apps.c / apps.h     → Programas built-in (terminal, calculadora, about)
├── progs.c / progs.h   → Registo de programas externos
├── progs/              → Programas externos (cada um no seu .c)
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

wins[win].bg = cor;                        → muda cor de fundo
```

### Ganchos para apps
```c
app_win        → índice da janela ativa (-1 se nenhuma)
app_on_key     → função chamada quando tecla é pressionada (ou 0)
app_on_draw    → função chamada para desenho extra (ou 0)
```

## Como criar uma app externa

### 1. Cria `progs/minha_app.c`

```c
#include "../lunarsnow.h"

static void draw(void)
{
    int wi = app_win;
    if (wi < 0) return;
    fb_txt(wins[wi].x + 10, wins[wi].y + 30, "Minha App!", C_TTT, wins[wi].bg);
}

void prog_minha_app(void)
{
    app_on_key = 0; app_on_draw = 0;
    int wi = gui_wnew("Minha App", 100, 100, 260, 140);
    gui_wbtn(wi, "Fechar", 100, 80, 60, 26, app_close);
    app_win = wi;
    app_on_draw = draw;
}
```

### 2. Regista em `progs.c`

```c
extern void prog_minha_app(void);    /* <-- declara */

Prog progs[] = {
    {"Hello Demo",  prog_hello},
    {"Minha App",   prog_minha_app}, /* <-- regista */
};
```

### 3. `make` e ta pronto

O menu (Start) e a navegação por teclado atualizam-se automaticamente.

### Apps com teclado

Segue o padrão do terminal:

```c
static void minha_key(int key)
{
    if (key >= 32 && key <= 126) { /* digitação */ }
    if (key == '\n') { /* executar */ }
}

void prog_minha_app(void)
{
    ...
    app_on_key = minha_key;
    app_on_draw = minha_draw;
}
```

O kernel roteia as teclas automaticamente quando `act == app_win`.
