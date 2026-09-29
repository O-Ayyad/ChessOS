ENTRY(kernel_start)

SECTIONS
{
    . = 0x100000;

    kernel_begin = .;
    .text   : { *(.text.start) *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data   : { *(.data .data.*) }

    /* .bss = global variables that start as zero. It is not stored in kernel.bin,
       so the kernel clears this memory itself when it starts. */
    .bss    : { bss_begin = .; *(.bss .bss.*) *(COMMON) bss_end = .; }
    kernel_end = .;

    /DISCARD/ : { *(.eh_frame*) *(.note*) *(.comment) }
}
