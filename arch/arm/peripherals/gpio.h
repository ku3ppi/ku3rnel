#ifndef ARCH_ARM_PERIPHERALS_GPIO_H
#define ARCH_ARM_PERIPHERALS_GPIO_H

#include <kstd/cstdint.h>
#include <kstd/cstddef.h> // For kstd::nullptr_t

namespace Arch {
namespace RaspberryPi {

// Raspberry Pi GPIO Base Address (varies by Pi model, this is for RPi 2/3/4 peripheral base at 0x3F000000)
// The peripheral bus address seen by the ARM core.
// RPi 1 (BCM2835): 0x20000000
// RPi 2/3 (BCM2836/7): 0x3F000000
// RPi 4 (BCM2711): 0xFE000000 (but often mapped via bus translation to 0x7E000000 for ARM, check Broadcom docs)
// For simplicity, using a common base often seen in examples, but this MUST be verified for the specific RPi model and its memory map.
// The RPi4 uses a different peripheral base: 0xFE000000 physical.
// However, to access it from the ARM core, it's often mapped to a virtual address.
// Let's assume a mapping or use a value that needs to be configured (e.g., from DTB or #define).
// For kernel8.img on RPi4, peripherals are typically accessed via a bus address translation.
// A common base used for userland access via /dev/gpiomem is 0x7E200000 for GPIO on older Pis.
// For RPi4, the VideoCore VI peripheral base is 0xFC000000. The ARM peripheral base is 0xFE000000.
// GPIO controller is at ARM physical 0xFE200000.
// We need to ensure our MMU setup (later) correctly maps this physical address to a virtual one.
// For bare metal without MMU yet, we use physical addresses.
constexpr kstd::uintptr_t GPIO_BASE = 0xFE200000; // For BCM2711 (Raspberry Pi 4)

// GPIO Function Select Registers (GPFSEL0-GPFSEL5)
// Each register controls 10 pins, 3 bits per pin.
// GPFSELn offset = 0x00 + (n * 4)
// GPIO Pin Output Set Registers (GPSET0-GPSET1)
// GPSETn offset = 0x1C + (n * 4)
// GPIO Pin Output Clear Registers (GPCLR0-GPCLR1)
// GPCLRn offset = 0x28 + (n * 4)
// GPIO Pin Level Registers (GPLEV0-GPLEV1)
// GPLEVn offset = 0x34 + (n * 4)
// GPIO Pin Pull-up/down Enable Registers (GPPUD)
// GPPUD offset = 0x94 (deprecated on RPi4, use GPPUPPDN0-3)
// GPIO Pin Pull-up/down Clock Registers (GPPUDCLK0-GPPUDCLK1) - Deprecated
// New registers for RPi4: GPPUPPDN0-3 (GPIO Pull-up / Pull-down Register 0-3)
// GPPUPPDN0 (GPIO Pins 0-15) offset: 0xE4
// GPPUPPDN1 (GPIO Pins 16-31) offset: 0xE8
// GPPUPPDN2 (GPIO Pins 32-47) offset: 0xEC
// GPPUPPDN3 (GPIO Pins 48-57) offset: 0xF0
// Each register controls 16 pins, 2 bits per pin (00=No PUD, 01=Pull-up, 10=Pull-down, 11=Reserved)

enum class GPIOPinFunc : kstd::uint8_t {
    INPUT   = 0b000,
    OUTPUT  = 0b001,
    ALT0    = 0b100,
    ALT1    = 0b101,
    ALT2    = 0b110,
    ALT3    = 0b111,
    ALT4    = 0b011,
    ALT5    = 0b010
    // Note: Check BCM2711 datasheet for exact alt function mappings for specific pins.
};

enum class GPIOPullState : kstd::uint8_t {
    NONE      = 0b00,
    PULL_UP   = 0b01,
    PULL_DOWN = 0b10,
    RESERVED  = 0b11
};

class GPIO {
public:
    GPIO() = default; // Default constructor, assumes static access methods or later init.

    // Initialize the GPIO driver (optional, could be stateless with static methods)
    void init();

    // Set function for a GPIO pin
    static void set_pin_function(unsigned int pin_number, GPIOPinFunc func);

    // Set a GPIO pin high
    static void set_pin_output(unsigned int pin_number);

    // Set a GPIO pin low
    static void clear_pin_output(unsigned int pin_number);

    // Read pin level
    static bool read_pin_level(unsigned int pin_number);

    // Set pull state for a GPIO pin (RPi4 specific)
    static void set_pin_pull_state(unsigned int pin_number, GPIOPullState state);

private:
    // Helper to write to memory-mapped register
    static inline void mmio_write(kstd::uintptr_t reg, kstd::uint32_t val) {
        *(volatile kstd::uint32_t*)reg = val;
    }

    // Helper to read from memory-mapped register
    static inline kstd::uint32_t mmio_read(kstd::uintptr_t reg) {
        return *(volatile kstd::uint32_t*)reg;
    }

    // Max 57 GPIO pins on RPi4
    static constexpr unsigned int MAX_GPIO_PINS = 57;
};

} // namespace RaspberryPi
} // namespace Arch

#endif // ARCH_ARM_PERIPHERALS_GPIO_H
