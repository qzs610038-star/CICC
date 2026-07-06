/*==========================================================================
 *  startup_qcrv32.S  -  Startup code for SapphireSoC (QCRV32)
 *
 *  This is the first code executed after reset. It:
 *    1. Sets up the stack pointer
 *    2. Initializes .data section from flash/ROM
 *    3. Zeros the .bss section
 *    4. Calls main()
 *    5. Handles exceptions/interrupts
 *==========================================================================*/

    .section .init, "ax"
    .globl _start
    .type _start, @function

_start:
    /* Setup stack pointer */
    la sp, _sp

    /* Disable interrupts during initialization */
    csrc mstatus, 0x8

    /* Initialize .data section */
    la t0, _sdata
    la t1, _edata
    la t2, _sidata
    copy_data:
        beq t0, t1, copy_data_done
        lw t3, 0(t2)
        sw t3, 0(t0)
        addi t0, t0, 4
        addi t2, t2, 4
        j copy_data
    copy_data_done:

    /* Zero .bss section */
    la t0, _sbss
    la t1, _ebss
    zero_bss:
        beq t0, t1, zero_bss_done
        sw zero, 0(t0)
        addi t0, t0, 4
        j zero_bss
    zero_bss_done:

    /* Setup global pointer */
    la gp, __global_pointer$

    /* Call main() */
    call main

    /* If main returns, loop forever */
    halt_loop:
        j halt_loop

    .size _start, . - _start

/*==========================================================================
 *  Exception and interrupt handlers
 *==========================================================================*/

    .section .vectors, "ax"
    .globl trap_handler
    .type trap_handler, @function

trap_handler:
    /* Save context */
    addi sp, sp, -32*4
    
    /* Save all registers */
    sw x1,  1*4(sp)
    sw x3,  3*4(sp)
    sw x4,  4*4(sp)
    sw x5,  5*4(sp)
    sw x6,  6*4(sp)
    sw x7,  7*4(sp)
    sw x8,  8*4(sp)
    sw x9,  9*4(sp)
    sw x10, 10*4(sp)
    sw x11, 11*4(sp)
    sw x12, 12*4(sp)
    sw x13, 13*4(sp)
    sw x14, 14*4(sp)
    sw x15, 15*4(sp)
    sw x16, 16*4(sp)
    sw x17, 17*4(sp)
    sw x18, 18*4(sp)
    sw x19, 19*4(sp)
    sw x20, 20*4(sp)
    sw x21, 21*4(sp)
    sw x22, 22*4(sp)
    sw x23, 23*4(sp)
    sw x24, 24*4(sp)
    sw x25, 25*4(sp)
    sw x26, 26*4(sp)
    sw x27, 27*4(sp)
    sw x28, 28*4(sp)
    sw x29, 29*4(sp)
    sw x30, 30*4(sp)
    sw x31, 31*4(sp)

    /* Read mcause to determine exception/interrupt type */
    csrr t0, mcause
    bgez t0, handle_exception

    /* Handle interrupt */
    j handle_interrupt

handle_exception:
    /* Exception handler - for now, just halt */
    j exception_halt

handle_interrupt:
    /* Interrupt handler - for now, just halt */
    j interrupt_halt

exception_halt:
    /* Restore context and return from trap */
    lw x1,  1*4(sp)
    lw x3,  3*4(sp)
    lw x4,  4*4(sp)
    lw x5,  5*4(sp)
    lw x6,  6*4(sp)
    lw x7,  7*4(sp)
    lw x8,  8*4(sp)
    lw x9,  9*4(sp)
    lw x10, 10*4(sp)
    lw x11, 11*4(sp)
    lw x12, 12*4(sp)
    lw x13, 13*4(sp)
    lw x14, 14*4(sp)
    lw x15, 15*4(sp)
    lw x16, 16*4(sp)
    lw x17, 17*4(sp)
    lw x18, 18*4(sp)
    lw x19, 19*4(sp)
    lw x20, 20*4(sp)
    lw x21, 21*4(sp)
    lw x22, 22*4(sp)
    lw x23, 23*4(sp)
    lw x24, 24*4(sp)
    lw x25, 25*4(sp)
    lw x26, 26*4(sp)
    lw x27, 27*4(sp)
    lw x28, 28*4(sp)
    lw x29, 29*4(sp)
    lw x30, 30*4(sp)
    lw x31, 31*4(sp)
    addi sp, sp, 32*4
    mret

interrupt_halt:
    j interrupt_halt

    .size trap_handler, . - trap_handler

/*==========================================================================
 *  Data section initialization values
 *==========================================================================*/

    .section .data.init, "a"
    .globl _sidata
_sidata:
    /* This section will be filled by linker */
