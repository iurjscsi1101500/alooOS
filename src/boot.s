.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:
.align 16
stack64_bottom:
    .skip 16384
stack64_top:
.align 4096
pml4_table:
    .skip 4096
.align 4096
pdpt_table:
    .skip 4096

.section .data
.align 8
gdt:
    .quad 0x0000000000000000
    .quad 0x00AF9A000000FFFF
    .quad 0x00CF92000000FFFF
gdt_end:
gdt_descriptor:
    .word gdt_end - gdt - 1
    .long gdt

.code32
.text
.global _start
.type _start, @function
_start:
    cli
    inb $0x92, %al
    orb $0x02, %al
    outb %al, $0x92
    outb %al, $0x80
    movl $pdpt_table, %edi
    xorl %eax, %eax
    movl $1024, %ecx
    rep stosl
    movl $pdpt_table, %edi
    movl $0x00000083, (%edi)
    movl $0, 4(%edi)
    movl $pml4_table, %edi
    xorl %eax, %eax
    movl $1024, %ecx
    rep stosl
    movl $pml4_table, %edi
    movl $pdpt_table, %eax
    orl $0x3, %eax
    movl %eax, (%edi)
    movl $0, 4(%edi)
    lgdt gdt_descriptor
    movl %cr4, %eax
    orl $(1 << 5), %eax
    movl %eax, %cr4
    movl $0xC0000080, %ecx
    rdmsr
    orl $(1 << 8), %eax
    wrmsr
    movl $pml4_table, %eax
    movl %eax, %cr3
    movl %cr0, %eax
    orl $0x80000000, %eax
    movl %eax, %cr0
    nop
    ljmp $0x08, $long_mode_entry

.code64
.text
.global long_mode_entry
.type long_mode_entry, @function
long_mode_entry:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    lea stack64_top(%rip), %rsp
    call kernel_main
1:
    cli
    hlt
    jmp 1b
.size _start, . - _start

