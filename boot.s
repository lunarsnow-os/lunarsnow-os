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
pdp:   .space 4096
/* 4 × Page Directory tables = 16KB, each maps 1GB via 512 × 2MB pages */
pd:    .space 4096 * 4
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

    /* Zero page tables: PML4 + PDP + 4 PDs = 6 × 4K = 0x6000 bytes */
    lea pml4, %edi
    xor %eax, %eax
    mov $0x6000, %ecx
    rep stosb

    /* PML4[0] → PDP0 */
    lea pdp, %eax
    or $3, %eax
    mov %eax, pml4

    /* Setup 4 PDP entries and 4 PD tables, identity-mapping 0-4GB with 2MB pages.
       ECX = PD table index (0..3), each maps 1GB.
       EBX = address of current PD table. */
    lea pd, %ebx
    xor %ecx, %ecx
.Louter:
    /* PDP[ECX] → PD[ECX] (high dword already 0 from BSS zero-fill) */
    mov %ebx, %eax
    or $3, %eax
    mov %eax, pdp(, %ecx, 8)

    /* Fill PD table: 512 entries, each mapping a 2MB page.
       Physical address = ECX * 1GB + EDX * 2MB.
       Since ECX < 4, the address fits in 32 bits.
       PD entry: low dword = (addr & 0xFFE00000) | 0x83, high dword = 0 */
    xor %edx, %edx
.Linner:
    mov %ecx, %esi
    shl $30, %esi          /* ESI = ECX * 1GB (base address for this PD) */
    mov %edx, %edi
    shl $21, %edi          /* EDI = EDX * 2MB (offset within this PD) */
    add %edi, %esi         /* ESI = full physical address */

    and $0xFFE00000, %esi  /* keep only bits 21-31 */
    or $0x83, %esi         /* flags: Present + Writable + PS(2MB) */

    mov %esi, (%ebx, %edx, 8)    /* low dword */
    mov $0, 4(%ebx, %edx, 8)     /* high dword = 0 */

    inc %edx
    cmp $512, %edx
    jne .Linner

    add $4096, %ebx
    inc %ecx
    cmp $4, %ecx
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
