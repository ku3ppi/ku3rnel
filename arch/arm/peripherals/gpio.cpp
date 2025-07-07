#include "gpio.h"
#include <kstd/cstdint.h> // For kstd::uintptr_t, kstd::uint32_t

// Raspberry Pi GPIO Register Offsets from GPIO_BASE
constexpr kstd::uintptr_t GPFSEL0_OFFSET = 0x00; // GPIO Function Select 0
// constexpr kstd::uintptr_t GPFSEL1_OFFSET = 0x04; // GPIO Function Select 1 (Pins 10-19)
// ... up to GPFSEL5 for pins 50-57

constexpr kstd::uintptr_t GPSET0_OFFSET  = 0x1C; // GPIO Pin Output Set 0 (Pins 0-31)
// constexpr kstd::uintptr_t GPSET1_OFFSET  = 0x20; // GPIO Pin Output Set 1 (Pins 32-57)

constexpr kstd::uintptr_t GPCLR0_OFFSET  = 0x28; // GPIO Pin Output Clear 0 (Pins 0-31)
// constexpr kstd::uintptr_t GPCLR1_OFFSET  = 0x2C; // GPIO Pin Output Clear 1 (Pins 32-57)

constexpr kstd::uintptr_t GPLEV0_OFFSET  = 0x34; // GPIO Pin Level 0 (Pins 0-31)
// constexpr kstd::uintptr_t GPLEV1_OFFSET  = 0x38; // GPIO Pin Level 1 (Pins 32-57)

// RPi4 Pull-up/down registers
constexpr kstd::uintptr_t GPPUPPDN0_OFFSET = 0xE4; // Pins 0-15
constexpr kstd::uintptr_t GPPUPPDN1_OFFSET = 0xE8; // Pins 16-31
constexpr kstd::uintptr_t GPPUPPDN2_OFFSET = 0xEC; // Pins 32-47
constexpr kstd::uintptr_t GPPUPPDN3_OFFSET = 0xF0; // Pins 48-57


namespace Arch {
namespace RaspberryPi {

void GPIO::init() {
    // GPIO initialization, if any specific setup is needed globally.
    // For now, individual pin setup is sufficient.
}

void GPIO::set_pin_function(unsigned int pin_number, GPIOPinFunc func) {
    if (pin_number > MAX_GPIO_PINS) return;

    kstd::uintptr_t reg_offset = GPFSEL0_OFFSET + (pin_number / 10) * 4;
    kstd::uintptr_t reg_addr = GPIO_BASE + reg_offset;

    unsigned int shift = (pin_number % 10) * 3;
    kstd::uint32_t current_val = mmio_read(reg_addr);

    current_val &= ~(0b111 << shift); // Clear current function bits for the pin
    current_val |= (static_cast<kstd::uint32_t>(func) << shift); // Set new function bits

    mmio_write(reg_addr, current_val);
}

void GPIO::set_pin_output(unsigned int pin_number) {
    if (pin_number > MAX_GPIO_PINS) return;

    kstd::uintptr_t reg_offset = (pin_number < 32) ? GPSET0_OFFSET : (GPSET0_OFFSET + 4); // GPSET1 is at 0x20
    kstd::uintptr_t reg_addr = GPIO_BASE + reg_offset;

    unsigned int bit = (pin_number < 32) ? pin_number : (pin_number - 32);
    mmio_write(reg_addr, (1 << bit));
}

void GPIO::clear_pin_output(unsigned int pin_number) {
    if (pin_number > MAX_GPIO_PINS) return;

    kstd::uintptr_t reg_offset = (pin_number < 32) ? GPCLR0_OFFSET : (GPCLR0_OFFSET + 4); // GPCLR1 is at 0x2C
    kstd::uintptr_t reg_addr = GPIO_BASE + reg_offset;

    unsigned int bit = (pin_number < 32) ? pin_number : (pin_number - 32);
    mmio_write(reg_addr, (1 << bit));
}

bool GPIO::read_pin_level(unsigned int pin_number) {
    if (pin_number > MAX_GPIO_PINS) return false; // Or handle error appropriately

    kstd::uintptr_t reg_offset = (pin_number < 32) ? GPLEV0_OFFSET : (GPLEV0_OFFSET + 4); // GPLEV1 is at 0x38
    kstd::uintptr_t reg_addr = GPIO_BASE + reg_offset;

    unsigned int bit = (pin_number < 32) ? pin_number : (pin_number - 32);
    return (mmio_read(reg_addr) & (1 << bit)) != 0;
}

void GPIO::set_pin_pull_state(unsigned int pin_number, GPIOPullState state) {
    if (pin_number > MAX_GPIO_PINS) return;

    // Determine which GPPUPPDN register to use (0-3)
    // Each register controls 16 pins, 2 bits per pin.
    unsigned int reg_index = pin_number / 16; // 0 for pins 0-15, 1 for 16-31, etc.
    kstd::uintptr_t pupd_reg_offset;

    switch (reg_index) {
        case 0: pupd_reg_offset = GPPUPPDN0_OFFSET; break;
        case 1: pupd_reg_offset = GPPUPPDN1_OFFSET; break;
        case 2: pupd_reg_offset = GPPUPPDN2_OFFSET; break;
        case 3: pupd_reg_offset = GPPUPPDN3_OFFSET; break;
        default: return; // Invalid pin for these registers
    }

    kstd::uintptr_t reg_addr = GPIO_BASE + pupd_reg_offset;

    // Determine the shift for the 2 bits within the register
    // Each pin uses 2 bits. (pin_number % 16) gives the pin index within the 16-pin group.
    unsigned int shift = (pin_number % 16) * 2;

    kstd::uint32_t current_val = mmio_read(reg_addr);
    current_val &= ~(0b11 << shift); // Clear current pull state for the pin
    current_val |= (static_cast<kstd::uint32_t>(state) << shift); // Set new pull state

    mmio_write(reg_addr, current_val);

    // Note: The BCM2711 datasheet mentions a specific sequence for applying pull states
    // which involves waiting for a certain number of clock cycles.
    // For simplicity, this explicit wait is omitted here but might be necessary for reliable operation.
    // The old method (GPPUD and GPPUDCLK) required:
    // 1. Write to GPPUD to set desired control signal (00=off, 01=pull-down, 10=pull-up)
    // 2. Wait 150 cycles
    // 3. Write to GPPUDCLK0/1 to clock the control signal into the target pins
    // 4. Wait 150 cycles
    // 5. Write to GPPUD to remove the control signal
    // 6. Write to GPPUDCLK0/1 to remove the clock
    // The new GPPUPPDN registers are simpler and directly set the state.
    // A short delay might still be beneficial after changing pull states on some peripherals.
    // For UART pins, this is usually done once at init.
}


} // namespace RaspberryPi
} // namespace Arch
