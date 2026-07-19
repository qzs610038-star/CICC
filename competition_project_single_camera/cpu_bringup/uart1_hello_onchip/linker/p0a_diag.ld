OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY { ram (wxai!r) : ORIGIN = 0xF9000000, LENGTH = 16K }
PHDRS { ram PT_LOAD; }

SECTIONS
{
  __stack_size = DEFINED(__stack_size) ? __stack_size : 2048;
  .init : { KEEP(*(SORT_NONE(.init))) } >ram AT>ram :ram
  .text :
  {
    *libc.a:*(.text .text.*)
    *(.text.main)
    *(.text.unlikely .text.unlikely.*)
    *(.text.startup .text.startup.*)
    *(.text .text.*)
    *(.gnu.linkonce.t.*)
    *(.note.gnu.build-id)
  } >ram AT>ram :ram
  .fini : { KEEP(*(SORT_NONE(.fini))) } >ram AT>ram :ram
  . = ALIGN(4);
  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } >ram AT>ram :ram
  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
    KEEP(*(.init_array))
    PROVIDE_HIDDEN(__init_array_end = .);
  } >ram AT>ram :ram
  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
    KEEP(*(.fini_array))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } >ram AT>ram :ram
  .lalign : ALIGN(4) { PROVIDE(_data_lma = .); } >ram AT>ram :ram
  .dalign : ALIGN(4) { PROVIDE(_data = .); } >ram AT>ram :ram
  .data :
  {
    *(.rdata)
    *(.rodata .rodata.*)
    *(.gnu.linkonce.r.*)
    *(.data .data.*)
    *(.gnu.linkonce.d.*)
    . = ALIGN(8);
    PROVIDE(__global_pointer$ = . + 0x7F0);
    *(.sdata .sdata.*)
    *(.gnu.linkonce.s.*)
    *(.srodata .srodata.*)
    . += 10;
  } >ram AT>ram :ram
  . = ALIGN(4);
  PROVIDE(_edata = .);
  PROVIDE(edata = .);
  . = ALIGN(16);
  __p0a_canary_start = .;
  .p0a_canary (NOLOAD) : { KEEP(*(.p0a_canary)) } >ram AT>ram :ram
  __p0a_canary_end = .;
  . = ALIGN(4);
  __bss_start = .;
  .bss (NOLOAD) :
  {
    *(.sbss*)
    *(.gnu.linkonce.sb.*)
    *(.bss .bss.*)
    *(.gnu.linkonce.b.*)
    *(COMMON)
    . = ALIGN(4);
  } >ram AT>ram :ram
  _end = .;
  .stack (NOLOAD) : ALIGN(16)
  {
    __stack_bottom = .;
    . += __stack_size;
    _sp = .;
    __stack_top = .;
    __freertos_irq_stack_top = .;
  } >ram AT>ram :ram
  ASSERT(__p0a_canary_start >= ORIGIN(ram), "canary below RAM")
  ASSERT(__p0a_canary_end <= __stack_bottom, "canary overlaps stack")
  ASSERT(__stack_top <= ORIGIN(ram) + LENGTH(ram), "image exceeds RAM")
}
