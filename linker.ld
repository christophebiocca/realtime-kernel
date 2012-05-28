ENTRY (main)

MEMORY
{
ram : ORIGIN = 0x00000000, LENGTH = 32M
rom : ORIGIN = 0x60000000, LENGTH = 8M
}

SECTIONS
{
    . = 0x218000;
    .text :
    {
        *(.text)
        *(.got)
        *(.got.plt)
        *(.rodata)
        *(.rodata.str1.4)
        *(.data.rel.ro.local)
        *(.glue_7)
        *(.glue_7t)
    } > ram
    .data : { *(.data) } > ram
    .bss : { *(.bss) } > ram
}
