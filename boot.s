.section .multiboot
.align 4

.long 0x1BADB002
.long 0x02
.long -(0x1BADB002 + 0x02)

.align 8
multiboot2_start:
.long 0xE85250D6
.long 0
.long multiboot2_end - multiboot2_start
.long -(0xE85250D6 + 0 + (multiboot2_end - multiboot2_start))

.word 5
.word 0
.long 20
.long 800
.long 600
.long 32

.align 8
.word 0
.word 0
.long 8
multiboot2_end:

.section .bss
.align 4096
pml4:  .space 4096
/* 16 PDP tables, each 4K, together cover 64GB (16 × 4 × 1GB) */
pdp:   .space 4096 * 16
    .align 16
    .space 16384
stack_top:

.section .data
.align 8
gdt:
    .quad 0
    .quad 0x00209A0000000000
    .quad 0x0000920000000000
gdt_end:

gdt_ptr:
    .word gdt_end - gdt - 1
    .quad gdt

.align 8
mb_magic: .quad 0
mb_info:  .quad 0

.section .text
.code32
.global _start
_start:
    mov $stack_top, %esp
    mov %eax, mb_magic
    mov %ebx, mb_info

    mov $0x80000000, %eax
    cpuid
    cmp $0x80000001, %eax
    jb .Lno64

    mov $0x80000001, %eax
    cpuid
    test $0x20000000, %edx
    jz .Lno64

    /* Zero all page table pages: PML4 + 16 PDPs = 17 × 4K = 0x11000 bytes */
    lea pml4, %edi
    xor %eax, %eax
    mov $0x11000, %ecx
    rep stosb

    /* Fill 16 PML4 entries and their PDP tables in a loop.
       Each PDP table has 4 × 1GB page entries covering 4GB.
       ecx = PML4 index, ebx = current PDP table address */
    xor %ecx, %ecx
    lea pdp, %ebx
.Louter:
    mov %ebx, %eax
    or $3, %eax
    mov %eax, pml4(, %ecx, 8)

    /* Fill 4 PDP entries: (edx << 30) | 0x83  in low dword,
       ecx in high dword (since entries (ecx*4 .. ecx*4+3) all >> 2 = ecx) */
    xor %edx, %edx
.Linner:
    mov %edx, %esi
    shl $30, %esi
    or $0x83, %esi
    mov %esi, (%ebx, %edx, 8)
    mov %ecx, 4(%ebx, %edx, 8)
    inc %edx
    cmp $4, %edx
    jne .Linner

    add $4096, %ebx
    inc %ecx
    cmp $16, %ecx
    jne .Louter

    mov %cr4, %eax
    or $0x620, %eax
    mov %eax, %cr4

    lea pml4, %eax
    mov %eax, %cr3

    mov $0xC0000080, %ecx
    rdmsr
    bts $8, %eax
    wrmsr

    mov %cr0, %eax
    and $~4, %eax
    bts $31, %eax
    mov %eax, %cr0

    lgdt gdt_ptr

    push $0x08
    push $.L64
    lret

.code64
.L64:
    mov $stack_top, %rsp
    mov mb_magic, %rdi
    mov mb_info, %rsi
    call kmain
    cli
1:  hlt
    jmp 1b

.code32
.Lno64:
    cli
1:  hlt
    jmp 1b
