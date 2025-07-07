#ifndef KERNEL_CONSOLE_H
#define KERNEL_CONSOLE_H

#include <kstd/cstddef.h> // For kstd::nullptr_t
#include <arch/arm/peripherals/uart.h> // For Arch::RaspberryPi::UART

// Forward declaration for kprintf arguments if needed
// struct PrintfArg; // If you have a custom printf that takes complex args

namespace Kernel {

class Console {
public:
    Console();

    // Initialize the console (sets up the underlying UART)
    void init();

    // Write a single character
    void put_char(char c);

    // Write a null-terminated string
    void print(const char* str);

    // Write a null-terminated string followed by a newline
    void println(const char* str);

    // Read a single character (blocking)
    char get_char();

    // Read a line of input from the console until newline or buffer full.
    // Stores the null-terminated string in 'buffer'.
    // Returns the number of characters read (excluding null terminator).
    kstd::size_t read_line(char* buffer, kstd::size_t buffer_size);

    // Formatted print (to be implemented with a simple printf implementation later)
    // For now, it can just take a string.
    // void kprintf(const char* format, ...); // Variadic functions are tricky without stdarg.h
    // A safer version:
    // void kprintf(const char* format, PrintfArg args[], kstd::size_t arg_count);
    // Simplest version for now:
    void kprintf(const char* message);


private:
    Arch::RaspberryPi::UART* uart_device; // Pointer to the UART device
    bool initialized;
};

// Global accessor for the main kernel console
Console& global_console();

} // namespace Kernel

#endif // KERNEL_CONSOLE_H
