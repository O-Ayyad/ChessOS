ENTRY(kernel_start)
SECTIONS
{
    /* KERNEL_ADDRESS in boot/layout.inc */
    . = 0x100000;
    kernel_begin = .;
    .text   : { *(.text.start) *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data   : { *(.data .data.*) }

    .bss    : { bss_begin = .; *(.bss .bss.*) *(COMMON) bss_end = .; }
    kernel_end = .;

    /DISCARD/ : { *(.eh_frame*) *(.note*) *(.comment) }
}
