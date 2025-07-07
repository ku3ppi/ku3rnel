/*
 * Kernel entry point after initial bootloader stub (boot.S).
 * Resides in .text section.
 *
 * Responsibilities:
 * 1. Clear the .bss section.
 * 2. Call the C++ kernel_main function.
 * 3. Halt if kernel_main returns.
 */
.section ".text._start_kernel" // Keep this early in .text
.global _start_kernel         // Entry point called by boot.S

// Linker script symbols for .bss section
.extern BSS_START
.extern BSS_END

// External C++ kernel_main function
.extern kernel_main

_start_kernel:
    // 1. Clear the .bss section
    // Load addresses of BSS_START and BSS_END from linker script.
    ldr x1, =BSS_START
    ldr x2, =BSS_END

    // Loop to clear BSS.
    // If BSS_START == BSS_END, this loop will not execute.
clear_bss_loop:
    cmp x1, x2              // Compare current pointer (x1) with BSS_END (x2)
    b.eq clear_bss_done     // If x1 == x2, BSS is cleared
    str xzr, [x1]           // Store zero (xzr) into memory location pointed by x1
    add x1, x1, #8          // Increment pointer by 8 bytes (AArch64 general register size)
    b clear_bss_loop
clear_bss_done:

    // (Optional) Further stack pointer adjustment if needed.
    // For now, assume sp set in boot.S is sufficient.

    // (Optional) Setup Exception Vector Base Address Register (VBAR_EL1)
    // If not done by firmware and if we are in EL1.
    // Example:
    // adr x0, _exception_vectors // Assuming _exception_vectors is defined elsewhere
    // msr vbar_el1, x0

    // 2. Call the C++ kernel_main function.
    // The FDT address might be in x0 if passed by the bootloader.
    // If kernel_main needs it, it should be its first argument.
    // For now, assume kernel_main takes no arguments or x0 is correctly set.
    bl kernel_main

    // 3. Halt the CPU if kernel_main returns (it shouldn't).
    // This indicates a problem or that the kernel has intentionally exited.
halt_loop:
    wfi // Wait for interrupt (low power state)
    b halt_loop

// Ensure this file is part of the build by adding it to S_SOURCES in Makefile if not already.
// It is already listed in the Makefile provided in Step 1.
