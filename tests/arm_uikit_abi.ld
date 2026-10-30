ENTRY(fixture_setup)
SECTIONS {
 . = 0x10000;
 .text : { *(.text*) }
 .rodata : { *(.rodata*) }
 . = ALIGN(16);
 .data : { *(.data*) }
 . = ALIGN(16);
 .bss : { *(.bss*) *(COMMON) }
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) *(.note*) }
}
