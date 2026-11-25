ENTRY(_start32)

SECTIONS {
    . = 0x10000;

    .text ALIGN(0x1000) : {
        *(.text.trampoline)
        *(.text.entry)
        *(.text .text.*)
    }

    .rdata ALIGN(0x1000) : {
        *(.rdata .rdata.*)
        *(.rodata .rodata.*)
    }

    .data ALIGN(0x1000) : {
        *(.data .data.*)
    }

    . = 0x200000;
    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    }

    /DISCARD/ : {
        *(.pdata)
        *(.xdata)
        *(.reloc)
        *(.eh_frame)
        *(.comment)
    }
}
