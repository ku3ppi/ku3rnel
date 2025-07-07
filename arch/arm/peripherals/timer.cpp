#include "timer.h"
#include <kernel/interrupt.h> // For get_interrupt_controller
#include <kernel/console.h>   // For kprintf
#include <kstd/cstdint.h>

namespace Arch {
namespace RaspberryPi {

// IRQ number for EL1 Physical Timer (CNTP_EL1)
// This is typically a PPI. For Cortex-A CPUs, it's often IRQ 30.
// This should be confirmed with the specific CPU and GIC documentation.
// GIC PPIs are IRQs 16-31. IRQ 30 is CNTPNSIRQ (Non-secure EL1 physical timer).
// IRQ 29 is CNTPSIRQ (Secure EL1 physical timer).
// IRQ 27 is CNTVIRQ (EL1 virtual timer).
// IRQ 26 is CNTHPIRQ (EL2 physical timer).
// We'll use IRQ 30 for non-secure EL1 physical timer.
constexpr unsigned int EL1_PHYSICAL_TIMER_IRQ = 30;


// Static instance of the timer handler for the global timer.
// This is needed because the GIC callback is a C-style function pointer.
static GenericTimer global_el1_timer_instance;

// C-style trampoline for the interrupt handler
static void generic_timer_irq_trampoline(unsigned int irq_num, void* context) {
    (void)irq_num; // Should match EL1_PHYSICAL_TIMER_IRQ
    GenericTimer* timer_instance = static_cast<GenericTimer*>(context);
    if (timer_instance) {
        timer_instance->handle_interrupt();
    }
}


GenericTimer::GenericTimer()
    : user_handler(nullptr), user_context(nullptr),
      irq_number(EL1_PHYSICAL_TIMER_IRQ), current_interval_ticks(0) {
}

kstd::uint64_t GenericTimer::get_timer_frequency_hz() {
    kstd::uint64_t cntfrq_el0;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(cntfrq_el0));
    return cntfrq_el0;
}

void GenericTimer::set_control(bool enable, bool imask) {
    kstd::uint32_t ctl_val = 0;
    if (enable) ctl_val |= (1 << 0); // ENABLE bit
    if (imask)  ctl_val |= (1 << 1); // IMASK bit (mask interrupt)
    // Bit 2 ISTATUS is Read-Only.
    asm volatile("msr cntp_ctl_el0, %0" : : "r"(ctl_val)); // Using EL0 registers for EL1 physical timer access
                                                          // EL1 Physical Timer is CNTP. Access via CNTP_*_EL0 from EL1.
}

void GenericTimer::set_interval_ticks(kstd::uint64_t ticks) {
    current_interval_ticks = ticks;
    asm volatile("msr cntp_tval_el0, %0" : : "r"(ticks)); // Write to TimerValue register
}


void GenericTimer::init(unsigned int frequency_hz, Kernel::InterruptHandler handler, void* context) {
    if (frequency_hz == 0) {
        Kernel::kprintf("GenericTimer: Cannot initialize with 0 Hz frequency.\n");
        return;
    }

    this->user_handler = handler;
    this->user_context = context;

    Kernel::InterruptController* ic = Kernel::get_interrupt_controller();
    if (!ic) {
        Kernel::kprintf("GenericTimer: Interrupt controller not available!\n");
        Kernel::panic("Timer init failed: no IC.");
        return;
    }

    // Register this timer's IRQ with the GIC
    // Pass 'this' as context so the trampoline can call the member function.
    if (!ic->register_handler(this->irq_number, generic_timer_irq_trampoline, this)) {
        Kernel::kprintf("GenericTimer: Failed to register IRQ %u with GIC.\n", this->irq_number);
        Kernel::panic("Timer init failed: GIC registration.");
        return;
    }
    Kernel::kprintf("GenericTimer: Registered handler for IRQ %u.\n", this->irq_number);


    // Calculate ticks for desired frequency
    kstd::uint64_t counter_freq = get_timer_frequency_hz();
    if (counter_freq == 0) {
        Kernel::kprintf("GenericTimer: Counter frequency (CNTFRQ_EL0) is 0! Cannot set timer.\n");
        Kernel::panic("Timer init failed: CNTFRQ_EL0 is 0.");
        return;
    }
    Kernel::kprintf("GenericTimer: CNTFRQ_EL0 = %llu Hz.\n", counter_freq);

    kstd::uint64_t ticks = counter_freq / frequency_hz;
    if (ticks == 0) {
        Kernel::kprintf("GenericTimer: Calculated ticks are 0 (frequency too high or counter too slow).\n");
        ticks = 1; // Minimum possible ticks
    }
    Kernel::kprintf("GenericTimer: Setting interval to %llu ticks for %u Hz.\n", ticks, frequency_hz);

    set_interval_ticks(ticks); // Set initial interval

    // Enable the timer and unmask its interrupt
    set_control(true /*enable*/, false /*unmask interrupt*/);

    // Enable the IRQ in the GIC
    ic->enable_irq(this->irq_number);

    Kernel::kprintf("GenericTimer initialized for %u Hz (IRQ %u).\n", frequency_hz, this->irq_number);
}

void GenericTimer::stop() {
    set_control(false /*disable*/, true /*mask interrupt*/);
    Kernel::InterruptController* ic = Kernel::get_interrupt_controller();
    if (ic) {
        ic->disable_irq(this->irq_number);
    }
}

void GenericTimer::handle_interrupt() {
    // Kernel::kprintf("Timer IRQ %u triggered!\n", this->irq_number); // Debug

    // IMPORTANT: The ARM Generic Timer (CNTP) is a one-shot timer.
    // To make it periodic, we must re-arm it by writing to CNTP_TVAL_EL0 again.
    // Write the same interval value.
    if (current_interval_ticks > 0) {
        asm volatile("msr cntp_tval_el0, %0" : : "r"(current_interval_ticks));
    } else {
        // This shouldn't happen if init was correct. Stop timer if interval is zero.
        set_control(false, true); // Disable and mask
    }

    // Call the user-provided handler, if any
    if (user_handler) {
        user_handler(this->irq_number, user_context);
    }

    // Signal End Of Interrupt to the GIC
    Kernel::InterruptController* ic = Kernel::get_interrupt_controller();
    if (ic) {
        ic->end_of_interrupt(this->irq_number);
    }
}


void system_timer_init_global(unsigned int frequency_hz, Kernel::InterruptHandler handler, void* context) {
    global_el1_timer_instance.init(frequency_hz, handler, context);
}

// The Makefile needs to be updated to include timer.cpp in CPP_SOURCES.
// It is already listed there.

// In kernel_main.cpp, call:
// Arch::RaspberryPi::system_timer_init_global(100, my_timer_callback_func, nullptr); // For 100Hz
// And define `my_timer_callback_func`.
// This will be done in the next step (updating kernel_main.cpp).

} // namespace RaspberryPi
} // namespace Arch
