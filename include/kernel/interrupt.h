#ifndef KERNEL_INTERRUPT_H
#define KERNEL_INTERRUPT_H

#include <kstd/cstdint.h>

namespace Kernel {

// Maximum number of IRQs supported. Adjust as needed for the specific GIC version.
// GICv2 typically supports up to 1020 SPIs (Shared Peripheral Interrupts).
// SGIs (Software Generated Interrupts) are 0-15.
// PPIs (Private Peripheral Interrupts) are 16-31.
constexpr unsigned int MAX_IRQS = 256; // A reasonable starting point, can be increased.

// Define a type for interrupt handler functions
// The context parameter can be used to pass data to the handler (e.g., device instance)
// The irq_num is the ID of the interrupt that occurred.
using InterruptHandler = void (*)(unsigned int irq_num, void* context);

// Structure to hold interrupt registration details
struct InterruptRegistration {
    InterruptHandler handler;
    void* context;
    bool is_registered;
};


// Interface for an interrupt controller
class InterruptController {
public:
    virtual ~InterruptController() = default;

    // Initialize the interrupt controller
    virtual void init() = 0;

    // Enable a specific IRQ
    virtual void enable_irq(unsigned int irq_num) = 0;

    // Disable a specific IRQ
    virtual void disable_irq(unsigned int irq_num) = 0;

    // Signal the end of interrupt processing for a specific IRQ
    virtual void end_of_interrupt(unsigned int irq_num) = 0;

    // Register a handler for a specific IRQ
    virtual bool register_handler(unsigned int irq_num, InterruptHandler handler, void* context) = 0;

    // Unregister a handler for a specific IRQ
    virtual bool unregister_handler(unsigned int irq_num) = 0;

    // Dispatch an interrupt to its registered handler.
    // This would be called by the common IRQ entry point.
    virtual void dispatch_interrupt(unsigned int irq_num) = 0;

    // Globally enable interrupts at the CPU level
    virtual void enable_cpu_interrupts() = 0;

    // Globally disable interrupts at the CPU level
    virtual void disable_cpu_interrupts() = 0;
};

// Global access to the main interrupt controller instance
InterruptController* get_interrupt_controller();

} // namespace Kernel

#endif // KERNEL_INTERRUPT_H
