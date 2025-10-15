OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
  .multiboot : {
    KEEP(*(.multiboot))
  }

  . = 2M;

  .text BLOCK(4K) : ALIGN(4K) { *(.text*) }
  .rodata BLOCK(4K) : ALIGN(4K) { *(.rodata*) }
  .data BLOCK(4K) : ALIGN(4K) { *(.data*) }
  .bss  BLOCK(4K) : ALIGN(4K) { *(COMMON) *(.bss*) }
  /DISCARD/ : { *(.comment) *(.eh_frame) }
}

