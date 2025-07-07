#ifndef ARCH_ARM_CORE_GIC_H
#define ARCH_ARM_CORE_GIC_H

#include <kernel/interrupt.h> // For InterruptController, InterruptHandler, MAX_IRQS
#include <kstd/cstdint.h>
#include <kstd/cstddef.h> // For kstd::nullptr_t

namespace Arch {
namespace Arm {

// GIC constants (assuming GICv2 like GIC-400 on RPi4)
// These base addresses need to be discovered (e.g., from Device Tree Blob) or known for the platform.
// For RPi4 (BCM2711), GIC-400 distributor is at 0xFF841000 (ARM physical)
// GIC-400 CPU interface is at 0xFF842000 (ARM physical)
// These need to be mapped by MMU if enabled. For bare metal, use physical.
constexpr kstd::uintptr_t GICD_BASE = 0xFF841000; // Distributor interface
constexpr kstd::uintptr_t GICC_BASE = 0xFF842000; // CPU interface

// GIC Distributor (GICD) register offsets
constexpr kstd::uintptr_t GICD_CTLR      = 0x000; // Distributor Control Register
constexpr kstd::uintptr_t GICD_TYPER     = 0x004; // Interrupt Controller Type Register
constexpr kstd::uintptr_t GICD_IIDR      = 0x008; // Distributor Implementer ID Register
constexpr kstd::uintptr_t GICD_IGROUPR0  = 0x080; // Interrupt Group Registers (n=0-31 for 1024 IRQs)
constexpr kstd::uintptr_t GICD_ISENABLER0= 0x100; // Interrupt Set-Enable Registers (n=0-31)
constexpr kstd::uintptr_t GICD_ICENABLER0= 0x180; // Interrupt Clear-Enable Registers (n=0-31)
constexpr kstd::uintptr_t GICD_ISPENDR0  = 0x200; // Interrupt Set-Pend Registers (n=0-31)
constexpr kstd::uintptr_t GICD_ICPENDR0  = 0x280; // Interrupt Clear-Pend Registers (n=0-31)
constexpr kstd::uintptr_t GICD_ISACTIVER0= 0x300; // Interrupt Set-Active Registers (n=0-31)
constexpr kstd::uintptr_t GICD_ICACTIVER0= 0x380; // Interrupt Clear-Active Registers (n=0-31)
constexpr kstd::uintptr_t GICD_IPRIORITYR0=0x400; // Interrupt Priority Registers (n=0-254 for 1020 IRQs)
constexpr kstd::uintptr_t GICD_ITARGETSR0= 0x800; // Interrupt Processor Targets Registers (n=0-254) (SPIs only)
constexpr kstd::uintptr_t GICD_ICFGR0    = 0xC00; // Interrupt Configuration Registers (n=0-63 for 1024 IRQs)
// constexpr kstd::uintptr_t GICD_SGIR      = 0xF00; // Software Generated Interrupt Register (GICv1, v2)

// GIC CPU Interface (GICC) register offsets
constexpr kstd::uintptr_t GICC_CTLR      = 0x00;  // CPU Interface Control Register
constexpr kstd::uintptr_t GICC_PMR       = 0x04;  // Interrupt Priority Mask Register
constexpr kstd::uintptr_t GICC_BPR       = 0x08;  // Binary Point Register
constexpr kstd::uintptr_t GICC_IAR       = 0x0C;  // Interrupt Acknowledge Register
constexpr kstd::uintptr_t GICC_EOIR      = 0x10;  // End of Interrupt Register
constexpr kstd::uintptr_t GICC_RPR       = 0x14;  // Running Priority Register
constexpr kstd::uintptr_t GICC_HPPIR     = 0x18;  // Highest Priority Pending Interrupt Register
// constexpr kstd::uintptr_t GICC_IIDR      = 0xFC;  // CPU Interface Identification Register


class GICDriver : public Kernel::InterruptController {
public:
    GICDriver(kstd::uintptr_t dist_base, kstd::uintptr_t cpu_if_base);
    ~GICDriver() override = default;

    void init() override;
    void enable_irq(unsigned int irq_num) override;
    void disable_irq(unsigned int irq_num) override;
    void end_of_interrupt(unsigned int irq_num) override; // Also acknowledges the IRQ
    bool register_handler(unsigned int irq_num, Kernel::InterruptHandler handler, void* context) override;
    bool unregister_handler(unsigned int irq_num) override;
    void dispatch_interrupt(unsigned int dummy_irq_num) override; // Reads IAR to get actual IRQ

    void enable_cpu_interrupts() override;
    void disable_cpu_interrupts() override;

private:
    kstd::uintptr_t gicd_base_addr; // Distributor base
    kstd::uintptr_t gicc_base_addr; // CPU interface base

    Kernel::InterruptRegistration handlers[Kernel::MAX_IRQS];

    // MMIO helpers
    inline void gicd_write(kstd::uintptr_t offset, kstd::uint32_t value) {
        *(volatile kstd::uint32_t*)(gicd_base_addr + offset) = value;
    }
    inline kstd::uint32_t gicd_read(kstd::uintptr_t offset) {
        return *(volatile kstd::uint32_t*)(gicd_base_addr + offset);
    }
    inline void gicc_write(kstd::uintptr_t offset, kstd::uint32_t value) {
        *(volatile kstd::uint32_t*)(gicc_base_addr + offset) = value;
    }
    inline kstd::uint32_t gicc_read(kstd::uintptr_t offset) {
        return *(volatile kstd::uint32_t*)(gicc_base_addr + offset);
    }

    // Helper to set interrupt target to current CPU (core 0) for SPIs
    void set_irq_target_cpu0(unsigned int spi_num);

    // Helper to configure IRQ as level-sensitive or edge-triggered
    // GICD_ICFGRn: 2 bits per interrupt. 00=level-sensitive, 10=edge-triggered.
    // Default for SPIs is often level-sensitive. PPIs/SGIs are fixed.
    void configure_irq_trigger(unsigned int irq_num, bool edge_triggered);

    // Number of available IRQ lines (read from GICD_TYPER)
    unsigned int num_irq_lines = 0;
};

// Function to initialize and get the global GIC driver instance
void gic_init_global();

} // namespace Arm
} // namespace Arch

#endif // ARCH_ARM_CORE_GIC_H
