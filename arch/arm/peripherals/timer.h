#ifndef ARCH_ARM_PERIPHERALS_TIMER_H
#define ARCH_ARM_PERIPHERALS_TIMER_H

#include <kstd/cstdint.h>
#include <kernel/interrupt.h> // For InterruptHandler

namespace Arch {
namespace RaspberryPi {

// Using ARM Generic Timer (Architectural Timer) for EL1.
// This timer is part of the ARM CPU architecture, not a separate peripheral like the RPi System Timer.
// Access is via system registers CNTP_TVAL_EL0, CNTP_CTL_EL0 (for EL0 physical timer)
// or CNTV_TVAL_EL0, CNTV_CTL_EL0 (for EL0 virtual timer).
// We'll use the EL1 physical timer: CNTP_TVAL_EL1, CNTP_CTL_EL1.
// The interrupt for this timer is a Private Peripheral Interrupt (PPI), usually IRQ ID 30 for EL1 physical timer.

class GenericTimer {
public:
    GenericTimer();

    // Initialize the timer and set its interrupt frequency.
    // frequency_hz: Desired interrupt frequency (e.g., 100 for 100Hz).
    void init(unsigned int frequency_hz, Kernel::InterruptHandler handler, void* context);

    // Stop the timer
    void stop();

    // Get current timer frequency (counter frequency, not interrupt frequency)
    static kstd::uint64_t get_timer_frequency_hz();

    // Handle the timer interrupt (called by the common IRQ handler for this timer's IRQ)
    void handle_interrupt();


private:
    Kernel::InterruptHandler user_handler;
    void* user_context;
    unsigned int irq_number; // The IRQ number for this timer (e.g., 30 for CNTP_EL1)

    // Helper to set timer interval
    void set_interval_ticks(kstd::uint64_t ticks);

    // Helper to enable/disable timer control
    void set_control(bool enable, bool imask); // imask: true to mask interrupt, false to unmask

    // Store the interval for reloading, if needed for periodic behavior
    kstd::uint64_t current_interval_ticks;
};


// Global function to initialize the primary system timer.
void system_timer_init_global(unsigned int frequency_hz, Kernel::InterruptHandler handler, void* context);

// Global timer instance (if only one primary system timer is used this way)
// Or, Timer could be a member of a Peripherals class.
// For now, a global instance or access via init function.

} // namespace RaspberryPi
} // namespace Arch

#endif // ARCH_ARM_PERIPHERALS_TIMER_H
