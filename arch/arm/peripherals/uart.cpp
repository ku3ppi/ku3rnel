#include "uart.h"
#include "gpio.h" // For configuring GPIO pins for UART use
#include <kstd/cstdint.h>

namespace Arch {
namespace RaspberryPi {

// UART0 is typically on GPIO 14 (TXD0) and GPIO 15 (RXD0)
constexpr unsigned int UART0_TX_PIN = 14;
constexpr unsigned int UART0_RX_PIN = 15;

// Define a static instance for the main UART (UART0)
static UART main_uart_instance(UART0_BASE);

UART* get_main_uart() {
    return &main_uart_instance;
}

void uart_init_global() {
    // Default RPi4 UART clock is 48MHz. Baud rate 115200.
    // This function should be called once during kernel initialization.
    main_uart_instance.init();
}

UART::UART(kstd::uintptr_t base_addr) : base_address(base_addr) {}

void UART::init(unsigned int baud_rate, unsigned int uart_clock_hz) {
    // 1. Disable UART before configuration
    mmio_write(UART_CR_OFFSET, 0); // Clear control register

    // 2. Configure GPIO pins for UART0 (TXD0, RXD0)
    //    GPIO 14 (TXD0) -> ALT0 (PL011 TXD)
    //    GPIO 15 (RXD0) -> ALT0 (PL011 RXD)
    //    Refer to BCM2711 datasheet for alternate function mapping.
    //    For PL011 UART0 on RPi4:
    //    GPIO14 (TXD0) is ALT0 (value 0b100)
    //    GPIO15 (RXD0) is ALT0 (value 0b100)
    GPIO::set_pin_function(UART0_TX_PIN, GPIOPinFunc::ALT0);
    GPIO::set_pin_function(UART0_RX_PIN, GPIOPinFunc::ALT0);

    // 3. Set pull state for UART pins (important for reliable communication)
    //    Typically, TX should be pull-up or none, RX should be pull-up.
    //    BCM2711 manual suggests pull states are important.
    //    The GPPUPPDN registers are used for this on RPi4.
    //    No pull for TX, Pull-up for RX is a common configuration.
    //    Let's try no pull on both first, or pull-up on RX as per some recommendations.
    GPIO::set_pin_pull_state(UART0_TX_PIN, GPIOPullState::NONE); // Or PULL_UP
    GPIO::set_pin_pull_state(UART0_RX_PIN, GPIOPullState::PULL_UP);


    // 4. Clear pending interrupts (writing 1s to relevant bits)
    mmio_write(UART_ICR_OFFSET, 0x7FF); // Clear all relevant PL011 interrupt sources

    // 5. Calculate baud rate divisor
    //    BAUDDIV = (FUARTCLK / (16 * baud_rate))
    //    IBRD = integer part of BAUDDIV
    //    FBRD = integer part of ((fractional part of BAUDDIV * 64) + 0.5)
    // Example for 115200 baud with 48MHz clock:
    // BAUDDIV = 48,000,000 / (16 * 115200) = 48,000,000 / 1,843,200 = 26.041666...
    // IBRD = 26
    // FBRD = int((0.041666... * 64) + 0.5) = int(2.666... + 0.5) = int(3.166...) = 3
    double baud_divisor = static_cast<double>(uart_clock_hz) / (16.0 * baud_rate);
    kstd::uint32_t ibrd = static_cast<kstd::uint32_t>(baud_divisor);
    kstd::uint32_t fbrd = static_cast<kstd::uint32_t>((baud_divisor - ibrd) * 64.0 + 0.5);

    mmio_write(UART_IBRD_OFFSET, ibrd);
    mmio_write(UART_FBRD_OFFSET, fbrd);

    // 6. Configure Line Control Register (LCRH)
    //    Enable FIFO, 8-bit word length, 1 stop bit, no parity.
    kstd::uint32_t lcrh_val = UART_LCRH_WLEN_8BIT | UART_LCRH_FEN;
    mmio_write(UART_LCRH_OFFSET, lcrh_val);

    // 7. Configure Interrupt Mask Set/Clear Register (IMSC)
    //    Disable all interrupts for now (polling mode).
    mmio_write(UART_IMSC_OFFSET, 0); // Mask all interrupts

    // 8. Enable UART, TX, and RX
    kstd::uint32_t cr_val = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
    mmio_write(UART_CR_OFFSET, cr_val);

    // A small delay might be good practice after major configuration changes.
    delay(150); // Arbitrary small delay
}

void UART::write_char(char c) {
    // Wait until TX FIFO is not full (TXFF bit in FR is 0)
    while (mmio_read(UART_FR_OFFSET) & UART_FR_TXFF) {
        // Busy wait
        asm volatile("nop");
    }
    // Write the character to the data register
    mmio_write(UART_DR_OFFSET, (kstd::uint32_t)c);

    // If it's a newline, also send a carriage return for compatibility with some terminals
    if (c == '\n') {
        while (mmio_read(UART_FR_OFFSET) & UART_FR_TXFF) {
            asm volatile("nop");
        }
        mmio_write(UART_DR_OFFSET, (kstd::uint32_t)'\r');
    }
}

char UART::read_char() {
    // Wait until RX FIFO is not empty (RXFE bit in FR is 0)
    while (mmio_read(UART_FR_OFFSET) & UART_FR_RXFE) {
        // Busy wait
        asm volatile("nop");
    }
    // Read the character from the data register
    // The DR also contains error flags in bits 8-11, but we ignore them for simplicity here.
    return (char)(mmio_read(UART_DR_OFFSET) & 0xFF);
}

void UART::write_string(const char* str) {
    if (!str) return;
    for (kstd::size_t i = 0; str[i] != '\0'; ++i) {
        write_char(str[i]);
    }
}

bool UART::has_data() const {
    // Check if RX FIFO is not empty (RXFE bit in FR is 0)
    return !(mmio_read(UART_FR_OFFSET) & UART_FR_RXFE);
}

} // namespace RaspberryPi
} // namespace Arch
