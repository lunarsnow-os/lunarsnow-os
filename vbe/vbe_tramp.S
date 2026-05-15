/* Generic real-mode int 0x10 trampoline.
   int10_real(uint16_t ax, uint16_t bx, uint16_t cx, uint16_t di, uint16_t es)
   — switch to real mode, call int 0x10 with given registers, return AX.

   Registers are stored in low memory at 0x7020-0x7028 and read by
   the real-mode code (copied to 0x7100 because real-mode CS limit is
   64KB — code at 1MB+ would triple-fault). */

.section .text

.global int10_real
.type int10_real, @function

int10_real:
    pushq %rbp
    movq %rsp, %rbp

    /* Store registers for real-mode int10 call */
    movw %di, (0x7020)     /* AX (arg1) */
    movw %si, (0x7022)     /* BX (arg2) */
    movw %dx, (0x7024)     /* CX (arg3) */
    movw %cx, (0x7026)     /* DI (arg4) */
    movw %r8w, (0x7028)    /* ES (arg5) */

    /* Backup original GDT */
    sgdt (0x7048)

    /* Build temporary GDT at 0x5000 */
    xorq %rax, %rax
    movq %rax, 0x5000
    movq $0x00209A0000000000, %rax
    movq %rax, 0x5008
    movq $0x0000920000000000, %rax
    movq %rax, 0x5010
    movq $0x00CF9A000000FFFF, %rax
    movq %rax, 0x5018
    movq $0x00CF92000000FFFF, %rax
    movq %rax, 0x5020
    movq $0x008F9A000000FFFF, %rax
    movq %rax, 0x5028
    movq $0x008F92000000FFFF, %rax
    movq %rax, 0x5030

    /* Load temporary GDT */
    movw $55, (0x7048)
    movq $0x5000, (0x704A)
    lgdt (0x7048)

    /* Far jump to 32-bit compatibility mode (selector 0x18) */
    pushq $0x18
    leaq .Lpm32(%rip), %rax
    pushq %rax
    lretq

.code32
.Lpm32:
    movw $0x20, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    /* Disable paging */
    movl %cr0, %eax
    andl $~0x80000000, %eax
    movl %eax, %cr0

    /* Disable PAE */
    movl %cr4, %eax
    andl $~0x20, %eax
    movl %eax, %cr4

    /* Disable long mode */
    movl $0xC0000080, %ecx
    rdmsr
    andl $~0x100, %eax
    wrmsr

    /* Copy real-mode code (low_start..low_end) to low memory 0x7100 */
    movl $.Llow_start, %esi
    movl $.Llow_end, %ecx
    subl %esi, %ecx
    movl $0x7100, %edi
    rep movsb

    /* Far jump to 16-bit PM (selector 0x28) */
    pushl $0x28
    pushl $.Lpm16
    lret

.code16
.Lpm16:
    movw $0x30, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    /* Enter real mode */
    movl %cr0, %eax
    andl $~1, %eax
    movl %eax, %cr0

    /* Far jump to low-memory copy at 0x7100 (16-bit offset fits in 64K) */
    ljmp $0, $0x7100

/* ============================================================
   Real-mode code — runs at physical address 0x7100 (copied)
   Loads registers from 0x7020-0x7028, calls int 0x10,
   stores result AX at 0x702A, returns to 16-bit PM.
   ============================================================ */
.Llow_start:

    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7000, %sp

    /* Load AX, BX, CX, DI, ES from generic register locations */
    movw (0x7020), %ax
    movw (0x7022), %bx
    movw (0x7024), %cx
    movw (0x7026), %di
    pushw (0x7028)
    popw %es

    int $0x10

    /* Re-load DS/ES (BIOS may have changed them) and store result */
    pushw %ax
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    popw %ax
    movw %ax, (0x702A)

    /* Back to 16-bit protected mode */
    movl %cr0, %eax
    orl $1, %eax
    movl %eax, %cr0

    /* Far jump to high-memory .Lpm16b (32-bit offset — in PM the limit is 4GB) */
    pushw $0x28
    pushl $.Lpm16b
    .byte 0x66
    lret

.Llow_end:

/* ============================================================
   Back in 16-bit protected mode at original high address
   ============================================================ */
.Lpm16b:
    movw $0x30, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    pushw $0x18
    pushl $.Lpm32b
    .byte 0x66
    lret

.code32
.Lpm32b:
    movw $0x20, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    /* Re-enable PAE */
    movl %cr4, %eax
    orl $0x20, %eax
    movl %eax, %cr4

    /* Reload CR3 to flush TLB */
    movl %cr3, %eax
    movl %eax, %cr3

    /* Re-enable long mode */
    movl $0xC0000080, %ecx
    rdmsr
    orl $0x100, %eax
    wrmsr

    /* Re-enable paging */
    movl %cr0, %eax
    orl $0x80000000, %eax
    movl %eax, %cr0

    /* Far jump to 64-bit long mode */
    .byte 0xEA
    .long .Ll64
    .word 0x08

.code64
.Ll64:
    /* Restore original GDT */
    movq (0x704A), %rax
    movq %rax, 2(%rsp)
    movzwq (0x7048), %rax
    movq %rax, (%rsp)
    lgdt (%rsp)

    /* Reload 64-bit segments */
    pushq $0x08
    leaq 1f(%rip), %rax
    pushq %rax
    lretq
1:
    movq $0x10, %rax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    /* Return AX result from 0x702A */
    movzwq (0x702A), %rax

    popq %rbp
    ret
