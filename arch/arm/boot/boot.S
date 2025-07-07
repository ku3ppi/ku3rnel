/*
 * Minimal ARM AArch64 bootloader stub for Raspberry Pi 4.
 * Assumes the kernel (kernel8.img) is loaded at 0x80000 by the RPi firmware.
 *
 * Responsibilities:
 * 1. Set up an initial stack pointer.
 * 2. Branch to the main kernel entry point (_start_kernel).
 *
 * The RPi firmware typically initializes hardware to a reasonable state.
 * This stub does not handle EL changes; assumes started in EL1 or EL2.
 * If started in EL2/EL3, further setup in _start_kernel or early C++ code
 * would be needed to drop to EL1. For simplicity, we'll assume EL1 for now.
 */
.section ".boot", "ax" // Executable and allocated
.global _start         // Standard entry point symbol for RPi firmware

_start:
    // The RPi firmware loads kernel8.img at 0x80000 and jumps here.
    // x0-x3 might contain parameters from the bootloader (e.g., FDT address).
    // We should preserve x0 if it contains the FDT address for later use.
    // For now, we'll assume we don't need to parse it immediately.

    // 1. Set up the stack pointer (sp).
    // Choose a region of RAM that is guaranteed to be free.
    // Example: 0x100000 (1MB) as stack top. Ensure this is safe and available.
    // Or, more robustly, use a symbol from the linker script if stack space is reserved there.
    // For now, hardcoding a reasonable address.
    // A common practice is to place it high in available RAM.
    // Let's use 0x8000000 (128MB mark) as a temporary stack top if RAM is plentiful there.
    // Or, more conservatively, just above the kernel.
    // Let's use a linker-defined symbol for the stack top if possible,
    // or a hardcoded value known to be safe RAM.
    // The linker script has KERNEL_END. Stack should be above this.
    // For simplicity, let's pick a fixed address for initial stack.
    // RPi4 has RAM starting at 0x0. Let's put stack at 64MB.
    // Should be well above kernel image (loaded at 0x80000) and BSS.
    // Stack grows downwards.
    ldr x1, =0x4000000 // 64MB stack top (adjust as needed, ensure it's valid RAM)
    mov sp, x1

    // 2. Branch to the C++ kernel setup routine (_start_kernel)
    // _start_kernel will be defined in arch/arm/core/start.S
    // This function will handle further setup like clearing BSS.
    b _start_kernel

    // Should not return here. If it does, loop indefinitely.
.Lhang:
    wfe // Wait for event (low power halt)
    b .Lhang
