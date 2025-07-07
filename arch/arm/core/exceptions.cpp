#include <kernel/interrupt.h>
#include <kernel/console.h> // For Kernel::kprintf
#include <kstd/cstdint.h>
// #include "gic.h" // Will be created next (GICDriver)

// Trap frame structure - matches what is saved by exception_stub in exceptions.S
// This needs to be kept in sync with the assembly.
// Order: SPSR_EL1, ELR_EL1, LR (x30), XZR (padding for x30 pair), x28-x29, ..., x0-x1.
// Stack grows downwards. So, [sp] points to SPSR_EL1, ELR_EL1.
// [sp, #16] points to LR, XZR.
// [sp, #32] points to x28, x29 etc.
// The `mov x0, sp` passes this base address.
struct TrapFrame {
    // Saved by stp x0, x1, [sp, #-16]! sequences, then LR, then SPSR/ELR
    // This order is if we pushed SPSR/ELR *last* onto the stack pointed by SP.
    // If SPSR/ELR pushed first, then GPRs, order is different.
    // The current exceptions.S pushes GPRs first, then SPSR/ELR.
    // So SPSR/ELR are at the lowest address (pointed to by final SP).
    kstd::uint64_t spsr_el1;
    kstd::uint64_t elr_el1;
    kstd::uint64_t x30_lr; // x30 (Link Register)
    kstd::uint64_t xzr_pad;  // Padding for x30 pair store
    kstd::uint64_t gpr[31]; // x0-x29 (x28 was used to hold sp_base)
                            // More accurately, this should reflect the actual save order.
                            // Current stub saves x0-x29, x30.
                            // Let's define it based on the order they are on stack, from low to high address:
                            // SPSR_EL1, ELR_EL1 (at sp)
                            // LR, XZR_PAD (at sp+16)
                            // X28, X29 (at sp+32)
                            // ...
                            // X0, X1 (at sp+32 + 14*16)
};
// A simpler definition if we just pass SP as a uint64_t array pointer:
// struct TrapFrame {
//    kstd::uint64_t registers[33]; // 31 GPRs + SPSR + ELR
// };


extern "C" {

// These are defined in exceptions.S
void _enable_cpu_interrupts();
void _disable_cpu_interrupts();

// Function to initialize exception handling (e.g., set VBAR_EL1)
void init_exceptions() {
    extern char _exception_vectors[]; // Symbol from exceptions.S
    asm volatile("msr vbar_el1, %0" : : "r" (_exception_vectors) : "memory");
    Kernel::kprintf("VBAR_EL1 set to 0x%p\n", _exception_vectors);
}

// Generic C handlers called from assembly stubs
// The 'frame' argument points to the saved registers on the stack.
void c_sync_handler(TrapFrame* frame) {
    kstd::uint64_t esr_el1, far_el1;
    asm volatile("mrs %0, esr_el1" : "=r"(esr_el1)); // Exception Syndrome Register
    asm volatile("mrs %0, far_el1" : "=r"(far_el1)); // Fault Address Register

    Kernel::kprintf("\n--- Synchronous Exception ---\n");
    Kernel::kprintf("SPSR_EL1: 0x%016llx  ELR_EL1: 0x%016llx\n", frame->spsr_el1, frame->elr_el1);
    Kernel::kprintf("ESR_EL1:  0x%016llx  FAR_EL1: 0x%016llx\n", esr_el1, far_el1);

    // Decode ESR_EL1 (EC: Exception Class bits 31-26)
    unsigned int ec = (esr_el1 >> 26) & 0x3F;
    const char* ec_desc = "Unknown";
    switch(ec) {
        case 0b000000: ec_desc = "Unknown reason"; break;
        case 0b000111: ec_desc = "Trapped SVE, SIMD or floating-point instruction"; break;
        // case 0b001100: ec_desc = "Trapped MSR, MRS or System instruction"; break; // EL0/EL1
        case 0b010001: ec_desc = "Instruction Abort from lower EL"; break;
        case 0b010010: ec_desc = "Instruction Abort from same EL"; break;
        case 0b010101: ec_desc = "PC alignment fault"; break;
        case 0b011000: ec_desc = "Trapped MSR, MRS or System instruction in AArch64"; break;
        case 0b100000: ec_desc = "Data Abort from lower EL (AArch32)"; break;
        case 0b100001: ec_desc = "Data Abort from lower EL (AArch64)"; break;
        case 0b100010: ec_desc = "Data Abort from same EL (AArch32)"; break; // SP alignment fault
        case 0b100011: ec_desc = "Data Abort from same EL (AArch64)"; break; // SP alignment fault
        case 0b100100: ec_desc = "Data Abort, not SP alignment, from same EL"; break;
        case 0b100101: ec_desc = "Data Abort, not SP alignment, from same EL"; break;

        case 0b010110: ec_desc = "SVC instruction execution in AArch32 state"; break;
        case 0b010111: ec_desc = "SVC instruction execution in AArch64 state"; break; // SVC (syscall)
        // Many more...
        default: break;
    }
    Kernel::kprintf("EC: 0x%02x (%s)\n", ec, ec_desc);

    // For critical ones like Data Abort, print more details
    // if (ec == 0b100100 || ec == 0b100101) {
        // ISS field contains details
    // }

    Kernel::panic("Unhandled Synchronous Exception.");
}

void c_irq_handler(TrapFrame* frame) {
    (void)frame; // Trap frame might be useful for context switching later
    // Kernel::kprintf("IRQ received! ELR=0x%llx\n", frame->elr_el1); // Debug

    // Get the interrupt controller and dispatch the IRQ
    Kernel::InterruptController* ic = Kernel::get_interrupt_controller();
    if (ic) {
        // The GIC driver's dispatch_interrupt will read the GICC_IAR
        // to get the IRQ number and call the registered handler.
        ic->dispatch_interrupt(0); // Pass a dummy IRQ number for now, GIC will find the real one.
    } else {
        Kernel::kprintf("IRQ: No interrupt controller available!\n");
        // Optionally, could try a default EOI to GIC if its address is known,
        // but without a driver, it's risky.
    }
}

void c_fiq_handler(TrapFrame* frame) {
    (void)frame;
    Kernel::kprintf("FIQ received! SPSR_EL1: 0x%016llx ELR_EL1: 0x%016llx\n", frame->spsr_el1, frame->elr_el1);
    Kernel::panic("Unhandled FIQ Exception.");
}

void c_serror_handler(TrapFrame* frame) {
    kstd::uint64_t disr_el1; // For AArch64, SError Interrupt Status Register
    asm volatile("mrs %0, disr_el1" : "=r"(disr_el1)); // Or ISR_EL1 for some SError types

    Kernel::kprintf("SError received! SPSR_EL1: 0x%016llx ELR_EL1: 0x%016llx\n", frame->spsr_el1, frame->elr_el1);
    Kernel::kprintf("DISR_EL1: 0x%016llx\n", disr_el1);
    Kernel::panic("Unhandled SError Exception.");
}

void c_default_handler(TrapFrame* frame) {
    (void)frame;
    Kernel::kprintf("Default/Unknown exception caught!\n");
    Kernel::kprintf("SPSR_EL1: 0x%016llx ELR_EL1: 0x%016llx\n", frame->spsr_el1, frame->elr_el1);
    Kernel::panic("Unhandled Exception (default handler).");
}

} // extern "C"

// Implementation for enabling/disabling CPU interrupts (wrappers around assembly)
namespace Kernel {
    void enable_cpu_interrupts_platform() {
        _enable_cpu_interrupts();
    }

    void disable_cpu_interrupts_platform() {
        _disable_cpu_interrupts();
    }
} // namespace Kernel

// The Makefile needs to include exceptions.cpp in CPP_SOURCES
// It's currently commented out or missing from the initial Makefile.
// $(ARCH_CORE_DIR)/exceptions.cpp should be added.
// (Actually, it's not in the initial Makefile's CPP_SOURCES, this needs to be added)
//
// The TrapFrame struct also needs to be accurate.
// The order in exceptions.S is:
// PUSH x0-x29, x30 (LR)
// PUSH SPSR_EL1, ELR_EL1
// So on stack (low address to high):
// SPSR_EL1, ELR_EL1  <- sp points here when c_handler is called
// x30 (LR), XZR_PAD
// x28, x29
// ...
// x0, x1
// So, the TrapFrame struct should be:
// struct TrapFrame {
//   kstd::uint64_t spsr_el1;
//   kstd::uint64_t elr_el1;
//   kstd::uint64_t x30_lr;
//   kstd::uint64_t xzr_pad; // For the x30 pair store
//   kstd::uint64_t x28_x29[2]; // Or individual x28, x29
//   // ... down to x0, x1
//   // This is complex to define perfectly without seeing the full stack layout.
//   // A simpler way is to pass 'sp' as `void*` or `kstd::uint64_t*`
//   // and then cast and access offsets carefully in the C handler.
//   // For example:
//   // void c_irq_handler(kstd::uint64_t* stack_ptr) {
//   //    kstd::uint64_t spsr = stack_ptr[0];
//   //    kstd::uint64_t elr  = stack_ptr[1];
//   //    kstd::uint64_t lr   = stack_ptr[2];
//   //    ...
//   // }
//   // The current TrapFrame* frame assumes frame points to the base of where SPSR/ELR are.
// }
// The current TrapFrame definition will be used, assuming 'sp' passed to C handler correctly points to SPSR_EL1.
// The GPRs part of TrapFrame is simplified; a full definition would list all saved GPRs.
// For now, only SPSR_EL1 and ELR_EL1 are explicitly used from the frame in these handlers.
// If other registers are needed, the TrapFrame struct and assembly must be perfectly synced.
// Let's refine TrapFrame to match the save order more explicitly for clarity.
// The current definition `kstd::uint64_t gpr[31];` is problematic for indexing.
//
// The `exception_stub` macro saves:
// SPSR_EL1, ELR_EL1 (at sp)
// x30, XZR (at sp+16)
// x28, x29 (at sp+32) ... x0, x1 (at sp+32 + 14*16 = sp+256)
// Total size: 16 (SPSR/ELR) + 16 (LR/XZR) + 15*16 (x0-x29) = 16+16+240 = 272 bytes.
// So, TrapFrame should reflect this.
// struct TrapFrame {
//    kstd::uint64_t spsr_el1;
//    kstd::uint64_t elr_el1;
//    kstd::uint64_t x30; // LR
//    kstd::uint64_t xzr_padding_for_x30;
//    kstd::uint64_t x29; kstd::uint64_t x28;
//    // ... and so on for x27 down to x0.
//    // This is error prone. A union or direct array access from base `sp` is safer.
//    // For now, we only use spsr_el1 and elr_el1 from it, which are at the base.
// }
// The simple TrapFrame with just spsr_el1 and elr_el1 used for now is fine as only those are accessed.
// If GPRs from frame are needed later, this must be revisited.
//
// The Makefile needs to be updated to include this file.
// Add `$(ARCH_CORE_DIR)/exceptions.cpp` to `CPP_SOURCES`.
//
// Also, the linker script change mentioned in exceptions.S:
// Add `.vectors : ALIGN(2K) { KEEP(*(.vectors)) }`
// I will do this after the GIC driver.
