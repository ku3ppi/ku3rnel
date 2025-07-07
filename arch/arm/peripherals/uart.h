#ifndef ARCH_ARM_PERIPHERALS_UART_H
#define ARCH_ARM_PERIPHERALS_UART_H

#include <kstd/cstdint.h>
#include <kstd/cstddef.h> // For kstd::nullptr_t

namespace Arch {
namespace RaspberryPi {

// PL011 UART (UART0 on Raspberry Pi) Base Address
// RPi4: 0xFE201000 (ARM physical address for UART0/PL011)
// Other UARTs (UART2-5 are mini-UARTs or alternative PL011s) are at different addresses.
// We will focus on UART0 (often used for primary serial console).
constexpr kstd::uintptr_t UART0_BASE = 0xFE201000;

// PL011 Register Offsets
constexpr kstd::uintptr_t UART_DR_OFFSET   = 0x00; // Data Register
constexpr kstd::uintptr_t UART_FR_OFFSET   = 0x18; // Flag Register
constexpr kstd::uintptr_t UART_IBRD_OFFSET = 0x24; // Integer Baud Rate Divisor
constexpr kstd::uintptr_t UART_FBRD_OFFSET = 0x28; // Fractional Baud Rate Divisor
constexpr kstd::uintptr_t UART_LCRH_OFFSET = 0x2C; // Line Control Register
constexpr kstd::uintptr_t UART_CR_OFFSET   = 0x30; // Control Register
constexpr kstd::uintptr_t UART_IMSC_OFFSET = 0x38; // Interrupt Mask Set/Clear Register
constexpr kstd::uintptr_t UART_ICR_OFFSET  = 0x44; // Interrupt Clear Register

// Flag Register bits
constexpr kstd::uint32_t UART_FR_TXFE = (1 << 7); // Transmit FIFO empty
constexpr kstd::uint32_t UART_FR_RXFF = (1 << 6); // Receive FIFO full
constexpr kstd::uint32_t UART_FR_TXFF = (1 << 5); // Transmit FIFO full
constexpr kstd::uint32_t UART_FR_RXFE = (1 << 4); // Receive FIFO empty
constexpr kstd::uint32_t UART_FR_BUSY = (1 << 3); // UART busy

// Line Control Register bits
constexpr kstd::uint32_t UART_LCRH_WLEN_8BIT = (0b11 << 5); // Word length 8 bits
constexpr kstd::uint32_t UART_LCRH_FEN     = (1 << 4);   // Enable FIFOs

// Control Register bits
constexpr kstd::uint32_t UART_CR_UARTEN = (1 << 0); // UART enable
constexpr kstd::uint32_t UART_CR_TXE    = (1 << 8); // Transmit enable
constexpr kstd::uint32_t UART_CR_RXE    = (1 << 9); // Receive enable


class UART {
public:
    // Constructor - takes base address for flexibility, defaults to UART0
    explicit UART(kstd::uintptr_t base_addr = UART0_BASE);

    // Initialize UART
    // baud_rate: e.g., 115200
    // uart_clock_hz: The clock frequency supplied to the UART peripheral.
    // On RPi, this is often derived from the system clock (e.g., core clock / divisor).
    // For RPi4, the default UART clock is often 48MHz. This needs to be accurate.
    void init(unsigned int baud_rate = 115200, unsigned int uart_clock_hz = 48000000);

    // Write a single character
    void write_char(char c);

    // Read a single character (blocking)
    char read_char();

    // Write a null-terminated string
    void write_string(const char* str);

    // Check if receive FIFO has data
    bool has_data() const;

private:
    kstd::uintptr_t base_address;

    // Helper to write to memory-mapped register
    inline void mmio_write(kstd::uintptr_t offset, kstd::uint32_t val) const {
        *(volatile kstd::uint32_t*)(base_address + offset) = val;
    }

    // Helper to read from memory-mapped register
    inline kstd::uint32_t mmio_read(kstd::uintptr_t offset) const {
        return *(volatile kstd::uint32_t*)(base_address + offset);
    }

    // Simple delay function (busy wait) - for use during initialization if needed.
    // Count is arbitrary, depends on CPU speed.
    static void delay(int count) {
        for (volatile int i = 0; i < count; ++i) {
            asm volatile("nop");
        }
    }
};

// Global function to get a pointer to the primary UART instance (UART0)
// This allows other parts of the kernel to easily access the console UART.
UART* get_main_uart();

// Global init function, typically called from kernel_main
void uart_init_global();


} // namespace RaspberryPi
} // namespace Arch

#endif // ARCH_ARM_PERIPHERALS_UART_H
