.section .text
.align 4
.global _start

multiboot_header:
    .long 0x1BADB002
    .long 0x02
    .long -(0x1BADB002 + 0x02)

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
