#include "console.h"
#include <arch/arm/peripherals/uart.h> // For get_main_uart, uart_init_global
#include <kstd/cstring.h> // For kstd::strlen if used for read_line, or implement locally

namespace Kernel {

// Define the global console instance
static Console main_console_instance;

Console& global_console() {
    return main_console_instance;
}

Console::Console() : uart_device(nullptr), initialized(false) {
    // Constructor: UART device will be acquired during init()
}

void Console::init() {
    if (initialized) {
        return;
    }
    // Initialize the global UART instance if not already done (idempotent)
    // This assumes uart_init_global() sets up the main UART.
    Arch::RaspberryPi::uart_init_global();
    uart_device = Arch::RaspberryPi::get_main_uart();

    if (uart_device) {
        initialized = true;
        // println("Kernel Console Initialized (UART0)."); // Self-print test
    } else {
        // This is a problem: console cannot function.
        // A panic might be too early if panic itself uses console.
        // Consider a very raw, direct UART write for error, or a CPU halt.
    }
}

void Console::put_char(char c) {
    if (!initialized || !uart_device) return;
    uart_device->write_char(c);
}

void Console::print(const char* str) {
    if (!initialized || !uart_device || !str) return;
    uart_device->write_string(str);
}

void Console::println(const char* str) {
    if (!initialized || !uart_device) return;
    if (str) {
        uart_device->write_string(str);
    }
    uart_device->write_char('\n');
}

char Console::get_char() {
    if (!initialized || !uart_device) return 0; // Or some error indicator
    return uart_device->read_char();
}

kstd::size_t Console::read_line(char* buffer, kstd::size_t buffer_size) {
    if (!initialized || !uart_device || !buffer || buffer_size == 0) {
        return 0;
    }

    kstd::size_t count = 0;
    char c;

    while (count < buffer_size - 1) { // Leave space for null terminator
        c = get_char();

        if (c == '\r' || c == '\n') { // Handle Enter key (CR or LF)
            // Echo newline to move cursor to next line on terminal
            if (c == '\r') { // If CR, also expect LF or just echo CR+LF
                put_char('\r'); // Echo CR
                put_char('\n'); // Echo LF
            } else { // If LF
                 put_char('\n'); // Echo LF
            }
            break; // End of line
        } else if (c == 0x7F || c == '\b') { // Handle Backspace/Delete
            if (count > 0) {
                count--;
                // Echo backspace, space, backspace to erase character on terminal
                put_char('\b');
                put_char(' ');
                put_char('\b');
            }
        } else if (c >= ' ' && c <= '~') { // Printable characters
            buffer[count++] = c;
            put_char(c); // Echo character
        }
        // Ignore other control characters for simplicity
    }
    buffer[count] = '\0'; // Null-terminate the string
    return count;
}

// Basic kprintf, now forwards to Kernel::kprintf
// Note: Console::kprintf itself is not variadic here.
// If a variadic Console::kprintf is desired, it would need its own va_list handling
// or call the global Kernel::kprintf. For simplicity, users can call Kernel::kprintf directly.
// This Console::kprintf is more like a simple print wrapper.
void Console::kprintf(const char* message) {
    // This version of Console::kprintf is not variadic.
    // It just prints a simple string.
    // For formatted printing, call Kernel::kprintf directly.
    print(message);
}

} // namespace Kernel

// The global Kernel::kprintf (variadic) is now available from <lib/printf/printf.h>.
// Users should include that and call Kernel::kprintf(...) for formatted output.
// The Console::kprintf(const char*) remains as a simple string printer.
