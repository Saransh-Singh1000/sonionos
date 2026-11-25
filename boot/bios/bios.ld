ENTRY(_start32)
SECTIONS {
    . = 0x10000;
    .text : { *(.text.trampoline) *(.text.entry) *(.text .text.*) }
    .rdata : { *(.rdata .rdata.*) *(.rodata .rodata.*) }
    .data : { *(.data .data.*) }
    . = 0x200000;
    .bss : { *(COMMON) *(.bss .bss.*) }
    /DISCARD/ : { *(.pdata) *(.xdata) *(.reloc) *(.eh_frame) *(.comment) }
}
