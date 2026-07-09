OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS
{
  . = 0x00001000;
  .text : {
    *(.text)
    *(.text.*)
  }
  . = ALIGN(4);
  .rodata : {
    *(.rodata)
    *(.rodata.*)
  }
  . = ALIGN(4);
  .data : {
    *(.data)
    *(.data.*)
  }
  .bss : {
    *(.bss)
    *(.bss.*)
    *(COMMON)
  }
}
