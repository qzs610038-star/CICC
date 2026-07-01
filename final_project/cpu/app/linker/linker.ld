/*==========================================================================
 *  linker.ld  -  Linker script for SapphireSoC (QCRV32) on TJ375N529
 *
 *  Memory map:
 *    - DDR: 0x00000000 - 0x1FFFFFFF (512MB LPDDR4x)
 *    - Peripherals: 0xF0000000 and above
 *
 *  The RISC-V code runs from DDR. Frame buffer is at 0x04000000 (64MB).
 *  Application code is placed at the beginning of DDR (0x00000000).
 *==========================================================================*/

OUTPUT_ARCH("riscv")

/* Entry point */
ENTRY(_start)

/* Memory regions */
MEMORY
{
    /* DDR memory - application code and data */
    DDR (rwx) : ORIGIN = 0x00000000, LENGTH = 64M
    
    /* Frame buffer region (reserved, not used by linker) */
    /* FRAME_BUFFER : ORIGIN = 0x04000000, LENGTH = 64M */
    
    /* Peripheral region */
    PERIPH (rw) : ORIGIN = 0xF0000000, LENGTH = 256M
}

/* Stack and heap sizes */
__stack_size = 0x2000;    /* 8KB stack */
__heap_size  = 0x1000;    /* 4KB heap */

SECTIONS
{
    /* Reset vector and exception handlers */
    .init :
    {
        KEEP(*(.init))
        KEEP(*(.init.rvfp))
    } > DDR

    /* Exception vectors */
    .vectors :
    {
        KEEP(*(.vectors))
    } > DDR

    /* Text section (code) */
    .text :
    {
        . = ALIGN(4);
        _stext = .;
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.gnu.linkonce.t.*)
        . = ALIGN(4);
        _etext = .;
    } > DDR

    /* Read-only data */
    .rodata :
    {
        . = ALIGN(4);
        _srodata = .;
        *(.rodata)
        *(.rodata*)
        *(.gnu.linkonce.r.*)
        . = ALIGN(4);
        _erodata = .;
    } > DDR

    /* Initialized data */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        __global_pointer$ = . + 0x800;
        *(.sdata)
        *(.sdata*)
        *(.gnu.linkonce.s.*)
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > DDR

    /* BSS section (zero-initialized) */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.sbss)
        *(.sbss*)
        *(.gnu.linkonce.sb.*)
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > DDR

    /* Heap */
    .heap :
    {
        . = ALIGN(4);
        __heap_start = .;
        . = . + __heap_size;
        __heap_end = .;
    } > DDR

    /* Stack */
    .stack :
    {
        . = ALIGN(16);
        __stack_start = .;
        . = . + __stack_size;
        _sp = .;
        __stack_end = .;
    } > DDR

    /* Debug info */
    .debug_info     0 : { *(.debug_info) }
    .debug_line     0 : { *(.debug_line) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_str      0 : { *(.debug_str) }

    /* End of all sections (required by libc) */
    _end = .;
}

/* Provide _exit for libc (infinite loop) */
PROVIDE(_exit = halt_loop);
