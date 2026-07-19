OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY { profile (rwx) : ORIGIN = 0, LENGTH = 16K }

SECTIONS
{
  .text : { *(.text._start) *(.text .text.*) } > profile
  .rodata : { *(.rodata .rodata.*) *(.srodata .srodata.*) } > profile
  .data : { *(.data .data.*) *(.sdata .sdata.*) } > profile
  . = ALIGN(16);
  __bss_start = .;
  .bss (NOLOAD) : { *(.bss .bss.*) *(.sbss .sbss.*) *(COMMON) } > profile
  __bss_end = .;
  . = ALIGN(16);
  __stack_bottom = .;
  . += 2048;
  __stack_top = .;
  __image_end = .;
  ASSERT(__image_end <= ORIGIN(profile) + LENGTH(profile), "F1 profile exceeds 16 KiB")
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
