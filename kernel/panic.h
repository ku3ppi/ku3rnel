#ifndef KERNEL_PANIC_H
#define KERNEL_PANIC_H

// Function to handle unrecoverable kernel errors.
// It should print a message and halt the system.
// The 'noexcept' and 'noreturn' attributes are important.
namespace Kernel {
    [[noreturn]] void panic(const char* message) noexcept;
}

#endif // KERNEL_PANIC_H
