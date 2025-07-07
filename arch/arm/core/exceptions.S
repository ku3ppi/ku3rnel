/*
 * AArch64 Exception Vector Table and Low-Level Handlers
 *
 * This file defines the exception vector table and provides minimal assembly
 * stubs that save context and call higher-level C/C++ handlers.
 *
 * Assumes running in EL1. VBAR_EL1 should point to _exception_vectors.
 */
.section ".vectors", "ax" // Ensure this section is page-aligned if VBAR requires it. Typically 2KB for AArch64.
.align 11 // 2^11 = 2048 byte alignment for VBAR_EL1

.global _exception_vectors
_exception_vectors:
    // Current EL with SP0 (Stack Pointer 0)
    // Synchronous Exception from Current EL (e.g., SVC, Data Abort, Undefined Instruction)
    b handle_sync_sp0         // Offset 0x000
    .align 7                  // Each entry is 128 bytes (0x80)
    // IRQ or vIRQ (Virtual IRQ) from Current EL
    b handle_irq_sp0          // Offset 0x080
    .align 7
    // FIQ or vFIQ (Virtual FIQ) from Current EL
    b handle_fiq_sp0          // Offset 0x100
    .align 7
    // SError or vSError (System Error) from Current EL
    b handle_serror_sp0       // Offset 0x180
    .align 7

    // Current EL with SPx (Stack Pointer x, where x > 0, usually SP_EL1 for EL1)
    // Synchronous Exception from Current EL
    b handle_sync_spx         // Offset 0x200
    .align 7
    // IRQ or vIRQ from Current EL
    b handle_irq_spx          // Offset 0x280  -> This is the one we'll focus on for peripheral interrupts
    .align 7
    // FIQ or vFIQ from Current EL
    b handle_fiq_spx          // Offset 0x300
    .align 7
    // SError or vSError from Current EL
    b handle_serror_spx       // Offset 0x380
    .align 7

    // Lower EL using AArch64
    // Synchronous Exception from Lower EL
    b handle_sync_lower_aarch64 // Offset 0x400
    .align 7
    // IRQ or vIRQ from Lower EL
    b handle_irq_lower_aarch64  // Offset 0x480
    .align 7
    // FIQ or vFIQ from Lower EL
    b handle_fiq_lower_aarch64  // Offset 0x500
    .align 7
    // SError or vSError from Lower EL
    b handle_serror_lower_aarch64 // Offset 0x580
    .align 7

    // Lower EL using AArch32
    // Synchronous Exception from Lower EL
    b handle_sync_lower_aarch32 // Offset 0x600
    .align 7
    // IRQ or vIRQ from Lower EL
    b handle_irq_lower_aarch32  // Offset 0x680
    .align 7
    // FIQ or vFIQ from Lower EL
    b handle_fiq_lower_aarch32  // Offset 0x700
    .align 7
    // SError or vSError from Lower EL
    b handle_serror_lower_aarch32 // Offset 0x780
    .align 7
    // Fill remaining space if any (up to 2048 bytes)
    // .space (2048 - ( . - _exception_vectors))


// Define common C-callable exception handler functions
.extern c_sync_handler
.extern c_irq_handler       // This will call GIC and dispatch
.extern c_fiq_handler
.extern c_serror_handler
.extern c_default_handler   // For unhandled types for now


// Macro to create a simple exception handler stub
// It saves general purpose registers, calls a C function, then restores registers and returns.
// For IRQs, we need to handle ELR_EL1 (return address) and SPSR_EL1 (saved program status) carefully.
// x0-x18 are caller-saved, x19-x29 are callee-saved. Link register x30.
// Full context save would include all GPRs (x0-x30), SP_EL0, ELR_EL1, SPSR_EL1, and FP/SIMD regs if used.
// For now, a simple stub. A more robust one would save more state or use a dedicated kernel stack for handlers.
.macro exception_stub name, c_handler_func
handle_\name\():
    // Basic context save: preserve link register (x30) and some GPRs if needed.
    // For simplicity, assuming the C handler doesn't corrupt too much, or uses a separate stack.
    // A real handler would save x0-x18 (caller-saved by AAPCS64) if the C function is complex.
    // It also needs to save ELR_EL1 and SPSR_EL1.
    // The C handler will be passed a pointer to the saved state (trap frame).

    // Allocate space on current stack for trap frame
    // (Example: 32 registers * 8 bytes/reg = 256 bytes) + SPSR + ELR + SP_EL0 + other info
    // For now, let's assume a simpler model where the C handler is careful or we switch stacks.
    // A common approach is to save all GPRs.
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    stp x6, x7, [sp, #-16]!
    stp x8, x9, [sp, #-16]!
    stp x10, x11, [sp, #-16]!
    stp x12, x13, [sp, #-16]!
    stp x14, x15, [sp, #-16]!
    stp x16, x17, [sp, #-16]!
    stp x18, x19, [sp, #-16]!
    stp x20, x21, [sp, #-16]!
    stp x22, x23, [sp, #-16]!
    stp x24, x25, [sp, #-16]!
    stp x26, x27, [sp, #-16]!
    stp x28, x29, [sp, #-16]!
    mov x28, sp // Save current SP (which is the base of the trap frame) into x28 (callee-saved)
    stp x30, xzr, [sp, #-16]! // Save LR (x30), pad with xzr

    // Save SPSR_EL1 and ELR_EL1
    mrs x0, spsr_el1
    mrs x1, elr_el1
    stp x0, x1, [sp, #-16]!

    // Pass pointer to trap frame (current SP) to C handler in x0
    mov x0, sp
    bl \c_handler_func

    // Restore SPSR_EL1 and ELR_EL1
    ldp x0, x1, [sp], #16
    msr spsr_el1, x0
    msr elr_el1, x1

    // Restore GPRs
    ldp x30, xzr, [sp], #16 // Restore LR
    mov sp, x28 // Restore original SP before GPR pushes
    ldp x28, x29, [sp], #16
    ldp x26, x27, [sp], #16
    ldp x24, x25, [sp], #16
    ldp x22, x23, [sp], #16
    ldp x20, x21, [sp], #16
    ldp x18, x19, [sp], #16
    ldp x16, x17, [sp], #16
    ldp x14, x15, [sp], #16
    ldp x12, x13, [sp], #16
    ldp x10, x11, [sp], #16
    ldp x8, x9, [sp], #16
    ldp x6, x7, [sp], #16
    ldp x4, x5, [sp], #16
    ldp x2, x3, [sp], #16
    ldp x0, x1, [sp], #16

    eret // Return from exception
.endm

// Generate stubs for SPx handlers (Current EL, using SP_EL1)
exception_stub sync_spx, c_sync_handler
exception_stub irq_spx, c_irq_handler    // This is the main one for peripheral IRQs
exception_stub fiq_spx, c_fiq_handler
exception_stub serror_spx, c_serror_handler

// For SP0 and Lower EL handlers, they might need different context saving
// or might not be used in our simple kernel if everything runs in EL1 with SP_EL1.
// For now, SP0 handlers can point to the same C stubs or dedicated ones.
handle_sync_sp0:
    mov x0, sp; bl c_sync_handler; eret
handle_irq_sp0:
    mov x0, sp; bl c_irq_handler; eret
handle_fiq_sp0:
    mov x0, sp; bl c_fiq_handler; eret
handle_serror_sp0:
    mov x0, sp; bl c_serror_handler; eret

// Default handlers for Lower EL types for now (can point to a generic "unexpected exception" C handler)
handle_sync_lower_aarch64:
    mov x0, sp; bl c_default_handler; eret
handle_irq_lower_aarch64:
    mov x0, sp; bl c_default_handler; eret
handle_fiq_lower_aarch64:
    mov x0, sp; bl c_default_handler; eret
handle_serror_lower_aarch64:
    mov x0, sp; bl c_default_handler; eret

handle_sync_lower_aarch32:
    mov x0, sp; bl c_default_handler; eret
handle_irq_lower_aarch32:
    mov x0, sp; bl c_default_handler; eret
handle_fiq_lower_aarch32:
    mov x0, sp; bl c_default_handler; eret
handle_serror_lower_aarch32:
    mov x0, sp; bl c_default_handler; eret

// Note: The Makefile needs to be updated to compile exceptions.S
// The S_SOURCES variable in the Makefile already has a placeholder for it.
// Uncomment: $(ARCH_CORE_DIR)/exceptions.S
// Also, ensure .vectors section is correctly placed by linker script (usually very early).
// The linker script rpi.ld has KEEP(*(.vectors)) in .text.
// It's better to give it its own section for alignment if VBAR_EL1 has strict alignment rules (e.g. 2KB for AArch64).
// The .section ".vectors", "ax" and .align 11 handles this.
// The linker script should be updated:
// .vectors : ALIGN(2K) { KEEP(*(.vectors)) } before .text or as part of its own segment.
// Current rpi.ld has KEEP(*(.vectors)) inside .text. This might be okay if .text itself is aligned.
// For safety, a dedicated aligned section for vectors is better.
// I will update the linker script after creating the C handler file.

.global _enable_cpu_interrupts
_enable_cpu_interrupts:
    msr daifclr, #2 // Clear IRQ mask bit (I) in DAIF register
    ret

.global _disable_cpu_interrupts
_disable_cpu_interrupts:
    msr daifset, #2 // Set IRQ mask bit (I) in DAIF register
    ret
