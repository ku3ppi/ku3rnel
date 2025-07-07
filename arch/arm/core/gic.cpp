#include "gic.h"
#include <kernel/console.h> // For kprintf
#include <kstd/cstring.h>   // For kmemset for handlers array

// External assembly functions for CPU interrupt enable/disable
extern "C" void _enable_cpu_interrupts();
extern "C" void _disable_cpu_interrupts();


namespace Arch {
namespace Arm {

// Global GIC driver instance
static GICDriver g_gic_driver(GICD_BASE, GICC_BASE);

// Global access to the main interrupt controller instance
Kernel::InterruptController* Kernel::get_interrupt_controller() {
    return &g_gic_driver;
}

void gic_init_global() {
    g_gic_driver.init();
    // After GIC is initialized, enable CPU interrupts
    g_gic_driver.enable_cpu_interrupts();
    Kernel::kprintf("GIC initialized and CPU IRQs enabled.\n");
}


GICDriver::GICDriver(kstd::uintptr_t dist_base, kstd::uintptr_t cpu_if_base)
    : gicd_base_addr(dist_base), gicc_base_addr(cpu_if_base) {
    kstd::kmemset(handlers, 0, sizeof(handlers));
}

void GICDriver::init() {
    Kernel::kprintf("Initializing GIC Driver (Dist: 0x%llx, CPUIf: 0x%llx)...\n", gicd_base_addr, gicc_base_addr);

    // --- Initialize Distributor (GICD) ---
    // 1. Disable distributor
    gicd_write(GICD_CTLR, 0x00000000);

    // 2. Get number of interrupt lines from GICD_TYPER
    // ITLinesNumber field (bits 4:0): (N+1)*32 lines. Max N=31 for 1024 lines.
    kstd::uint32_t typer = gicd_read(GICD_TYPER);
    num_irq_lines = ((typer & 0x1F) + 1) * 32;
    Kernel::kprintf("GICD_TYPER: 0x%x, Num IRQ lines: %u\n", typer, num_irq_lines);
    if (num_irq_lines == 0 || num_irq_lines > Kernel::MAX_IRQS * 8) { // Sanity check, MAX_IRQS is array size
        Kernel::kprintf("Warning: GIC reports %u lines, clamping or check MAX_IRQS.\n", num_irq_lines);
        // num_irq_lines = Kernel::MAX_IRQS; // Or handle error
    }


    // 3. Configure all SPIs (Shared Peripheral Interrupts, IRQID 32 upwards)
    //    - Set to Group 1 (for Non-Secure EL1 handling, if applicable, or just default group)
    //    - Set to be level-triggered by default (can be changed per IRQ)
    //    - Set default priority (e.g., 0xA0 - lower value is higher priority)
    //    - Set target to CPU 0
    //    - Disable all SPIs initially
    for (unsigned int i = 32; i < num_irq_lines && i < Kernel::MAX_IRQS; ++i) {
        // Set to Group 1 (non-secure, if GICD_CTLR.DS (Disable Security) is 0)
        // If DS=1, IGROUPRn is RAZ/WI. Assume non-secure for now or single security state.
        // kstd::uint32_t group_reg_val = gicd_read(GICD_IGROUPR0 + (i / 32) * 4);
        // group_reg_val |= (1 << (i % 32)); // Set to Group 1
        // gicd_write(GICD_IGROUPR0 + (i / 32) * 4, group_reg_val);
        // For simplicity, let's assume Group 0 or that security extensions aren't primary focus here.

        // Default priority (0xA0 is a common default, lower value = higher prio)
        gicd_write(GICD_IPRIORITYR0 + (i / 4) * 4 + (i % 4), 0xA0);

        // Default trigger: level-sensitive for SPIs
        configure_irq_trigger(i, false); // false for level-sensitive

        // Target SPIs to CPU 0
        set_irq_target_cpu0(i);

        // Disable interrupt
        gicd_write(GICD_ICENABLER0 + (i / 32) * 4, (1 << (i % 32)));
    }

    // PPIs (16-31) and SGIs (0-15) are per-CPU and have fixed configurations for trigger type.
    // Priorities for PPIs/SGIs can be set if needed.

    // 4. Enable distributor (Group 0 and Group 1, if applicable)
    // GICD_CTLR: Bit 0 enables Group 0, Bit 1 enables Group 1 (non-secure)
    // For simple single EL1 setup, just enabling Group 0 might be enough if not using security extensions.
    // Or enable both if GIC is configured for two security states.
    // Let's enable Group 0 forwarding. Bit 0 for GICv2.
    gicd_write(GICD_CTLR, 0x00000001); // Enable Group 0 interrupts


    // --- Initialize CPU Interface (GICC) ---
    // 1. Set Interrupt Priority Mask Register (GICC_PMR)
    //    Allows all priorities (lowest priority value 0xFF)
    gicc_write(GICC_PMR, 0xFF);

    // 2. Set Binary Point Register (GICC_BPR)
    //    Controls preemption. Group priority field split.
    //    e.g., 0x03 for no preemption group, full subpriority. Or 0x07 for no subpriorities.
    //    For simplicity, let's use a value that allows all priorities to be distinct.
    gicc_write(GICC_BPR, 0x03); // Allows for priority grouping if needed.

    // 3. Enable CPU interface (and EOI mode if GICv2)
    //    GICC_CTLR: Bit 0 enables signaling of Group 0 interrupts to CPU.
    //    Bit 1 for Group 1. Bit 9 (EOImodeNS for GICv2) for non-secure EOI behavior.
    //    For GICv2, EOImode (bit 9 for non-secure, bit 10 for secure) should be 0 for IAR/EOIR.
    //    Let's enable Group 0 signaling.
    kstd::uint32_t cpu_ctlr = 0x00000001; // Enable Group 0 interrupt signaling
    // if (is_gicv2_and_security_extensions_active) cpu_ctlr |= (1<<9); // EOImodeNS for separate EOI/priority drop
    gicc_write(GICC_CTLR, cpu_ctlr);

    Kernel::kprintf("GIC Driver Initialized.\n");
}


void GICDriver::enable_irq(unsigned int irq_num) {
    if (irq_num >= num_irq_lines || irq_num >= Kernel::MAX_IRQS) {
        Kernel::kprintf("GIC: enable_irq: Invalid IRQ %u\n", irq_num);
        return;
    }
    // Kernel::kprintf("GIC: Enabling IRQ %u\n", irq_num);
    gicd_write(GICD_ISENABLER0 + (irq_num / 32) * 4, (1 << (irq_num % 32)));
}

void GICDriver::disable_irq(unsigned int irq_num) {
    if (irq_num >= num_irq_lines || irq_num >= Kernel::MAX_IRQS) {
        Kernel::kprintf("GIC: disable_irq: Invalid IRQ %u\n", irq_num);
        return;
    }
    // Kernel::kprintf("GIC: Disabling IRQ %u\n", irq_num);
    gicd_write(GICD_ICENABLER0 + (irq_num / 32) * 4, (1 << (irq_num % 32)));
}

void GICDriver::end_of_interrupt(unsigned int irq_num) {
    // This function signals completion of handling for the specified IRQ.
    // For GICv2, this involves writing to GICC_EOIR.
    // The value written should be the IRQ number that was read from GICC_IAR.
    // Kernel::kprintf("GIC: EOI for IRQ %u\n", irq_num);
    gicc_write(GICC_EOIR, irq_num);
}

bool GICDriver::register_handler(unsigned int irq_num, Kernel::InterruptHandler handler, void* context) {
    if (irq_num >= Kernel::MAX_IRQS) { // Check against our handler array size
        Kernel::kprintf("GIC: register_handler: IRQ %u out of bounds for handler array (max %u)\n", irq_num, Kernel::MAX_IRQS-1);
        return false;
    }
    if (handlers[irq_num].is_registered) {
        Kernel::kprintf("GIC: register_handler: IRQ %u already has a handler.\n", irq_num);
        return false; // Or allow overriding
    }
    handlers[irq_num].handler = handler;
    handlers[irq_num].context = context;
    handlers[irq_num].is_registered = true;
    // Kernel::kprintf("GIC: Registered handler for IRQ %u at 0x%p\n", irq_num, (void*)handler);
    return true;
}

bool GICDriver::unregister_handler(unsigned int irq_num) {
    if (irq_num >= Kernel::MAX_IRQS) return false;
    if (!handlers[irq_num].is_registered) return false; // No handler to unregister

    handlers[irq_num].is_registered = false;
    handlers[irq_num].handler = nullptr;
    handlers[irq_num].context = nullptr;
    return true;
}

void GICDriver::dispatch_interrupt(unsigned int dummy_irq_num) {
    (void)dummy_irq_num; // Not used, we read IAR

    // 1. Acknowledge interrupt and get IRQ ID by reading GICC_IAR
    //    Bits 9:0 give IRQ ID. Bits 12:10 give CPUID (for SGIs from other cores).
    kstd::uint32_t iar_val = gicc_read(GICC_IAR);
    unsigned int irq_id = iar_val & 0x3FF; // Mask for IRQ ID (10 bits for GICv2)

    // Kernel::kprintf("GIC: Dispatching IRQ. IAR=0x%x, IRQ_ID=%u\n", iar_val, irq_id);


    if (irq_id >= 1020 && irq_id <= 1023) { // Spurious interrupt or special ID
        Kernel::kprintf("GIC: Spurious interrupt or special ID %u. Ignoring.\n", irq_id);
        // For spurious interrupts, no EOI is typically needed, but check GIC spec.
        // If it's a real IRQ ID that's special, might need handling.
        // For now, we just print and don't EOI if it's clearly spurious (e.g. 1023).
        if (irq_id != 1023) { // Don't EOI spurious IRQ 1023
             gicc_write(GICC_EOIR, irq_id); // Still EOI other "valid" special IDs if any
        }
        return;
    }

    if (irq_id >= num_irq_lines && irq_id < 1020) { // Check against GIC reported lines, but allow up to GIC max for safety
        Kernel::kprintf("GIC: IRQ ID %u out of expected range (max %u from TYPER).\n", irq_id, num_irq_lines);
        gicc_write(GICC_EOIR, irq_id); // EOI it to be safe
        return;
    }
    if (irq_id >= Kernel::MAX_IRQS) { // Check against our handler array size
         Kernel::kprintf("GIC: IRQ ID %u out of bounds for handler array (max %u).\n", irq_id, Kernel::MAX_IRQS-1);
         gicc_write(GICC_EOIR, irq_id); // EOI it
         return;
    }


    // 2. Call registered handler
    if (handlers[irq_id].is_registered && handlers[irq_id].handler) {
        handlers[irq_id].handler(irq_id, handlers[irq_id].context);
    } else {
        Kernel::kprintf("GIC: Unhandled IRQ %u\n", irq_id);
        // Optionally disable it to prevent interrupt storms
        // disable_irq(irq_id);
    }

    // 3. Signal End of Interrupt (EOI) by writing IRQ ID to GICC_EOIR
    //    This is done by the handler calling end_of_interrupt(irq_id)
    //    OR, if handlers don't call it, it must be done here.
    //    It's generally better if the handler (or a common wrapper) calls EOI.
    //    For now, let's assume handlers will call end_of_interrupt().
    //    If a handler panics or doesn't EOI, the system might hang on that IRQ.
    //    The current InterruptController interface has end_of_interrupt(), so handlers should use it.
    //    If we EOI here, then handlers calling it too would be an error.
    //    Let's assume for now the handler is responsible for EOI.
    //    If no handler, we should EOI here.
    if (!handlers[irq_id].is_registered || !handlers[irq_id].handler) {
        gicc_write(GICC_EOIR, irq_id); // EOI for unhandled IRQs
    }
    // Note: The logic for EOI (here vs. in handler) needs to be consistent.
    // The current model of `InterruptController::end_of_interrupt` implies the handler or
    // a common C IRQ dispatcher (that calls the handler) would call it.
    // Our `c_irq_handler` calls `ic->dispatch_interrupt`, and `dispatch_interrupt` calls the handler.
    // So, if the registered handler is expected to call `ic->end_of_interrupt(irq_id)`, then
    // this `dispatch_interrupt` function should NOT EOI itself, UNLESS there's no handler.
}


void GICDriver::enable_cpu_interrupts() {
    _enable_cpu_interrupts();
}

void GICDriver::disable_cpu_interrupts() {
    _disable_cpu_interrupts();
}

void GICDriver::set_irq_target_cpu0(unsigned int spi_num) {
    if (spi_num < 32) return; // Only for SPIs (Shared Peripheral Interrupts)
    kstd::uintptr_t reg_offset = GICD_ITARGETSR0 + (spi_num / 4) * 4; // Each reg holds 4 targets (8 bits each)
    unsigned int shift = (spi_num % 4) * 8;

    kstd::uint32_t val = gicd_read(reg_offset);
    val &= ~(0xFF << shift); // Clear current target for this SPI
    val |= (0x01 << shift);  // Target CPU interface 0 (bitmask, 0x01 for CPU0, 0x02 for CPU1 etc.)
    gicd_write(reg_offset, val);
}

void GICDriver::configure_irq_trigger(unsigned int irq_num, bool edge_triggered) {
    // SGIs (0-15) are edge-triggered. PPIs (16-31) are configurable by SoC (often level).
    // SPIs (32+) are configurable via GICD_ICFGRn.
    if (irq_num < 16) return; // SGIs trigger type is fixed.
                              // PPIs trigger type is implementation defined by SoC.
                              // We only configure SPIs here.
    if (irq_num < 32 && irq_num >=16) {
        // Kernel::kprintf("GIC: Trigger for PPI %u is SoC defined.\n", irq_num);
        return; // PPI trigger configuration is SoC specific, not via GICD_ICFGRn for some GICs.
                // However, GICv2 spec says GICD_ICFGR0-1 are for SGIs/PPIs.
                // GICD_ICFGR0 for SGIs 0-15 (fixed as edge)
                // GICD_ICFGR1 for PPIs 16-31 (implementation defined, RAZ/WI if fixed)
                // For RPi, these are typically fixed by SoC design.
    }


    // Each GICD_ICFGRn register configures 16 interrupts (2 bits per interrupt).
    // Bit [2m+1]: 0=Level-sensitive, 1=Edge-triggered for interrupt ID m.
    // Bit [2m] is RAZ/WI.
    kstd::uintptr_t reg_offset = GICD_ICFGR0 + (irq_num / 16) * 4;
    unsigned int shift = ((irq_num % 16) * 2) + 1; // Point to the second bit of the pair

    kstd::uint32_t val = gicd_read(reg_offset);
    if (edge_triggered) {
        val |= (1 << shift);
    } else {
        val &= ~(1 << shift);
    }
    gicd_write(reg_offset, val);
}


// Update Makefile:
// Add $(ARCH_CORE_DIR)/gic.cpp to CPP_SOURCES
//
// Update Linker Script rpi.ld:
// Add a dedicated, aligned section for .vectors BEFORE .text section.
// .vectors ALIGN(2048) : { KEEP(*(.vectors)) }
// This ensures VBAR_EL1 alignment requirements are met.
// I'll do this as the next action.

} // namespace Arm
} // namespace Arch
