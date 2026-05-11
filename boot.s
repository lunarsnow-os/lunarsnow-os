.section .text
.align 4
.global _start

/* Multiboot v1 header (for QEMU -kernel) */
.long 0x1BADB002
.long 0x02
.long -(0x1BADB002 + 0x02)

/* Multiboot2 header (for GRUB on real hardware — GRUB prefers this) */
.align 8
multiboot2_start:
.long 0xE85250D6
.long 0
.long multiboot2_end - multiboot2_start
.long -(0xE85250D6 + 0 + (multiboot2_end - multiboot2_start))

/* Framebuffer request tag: type=5, size=20, 800x600x32 */
.word 5
.word 0
.long 20
.long 800
.long 600
.long 32

/* End tag */
.align 8
.word 0
.word 0
.long 8
multiboot2_end:

_start:
    mov $stack_top, %esp
    push %ebx
    push %eax
    call kmain
    cli
1:  hlt
    jmp 1b

.section .bss
.align 16
    .space 16384
stack_top:
