#include <kernel/panic.h>
#include <kernel/console.h> // Assuming console is available for panic messages

// extern "C" void _hang(); // External assembly function to halt CPU

namespace Kernel {

[[noreturn]] void panic(const char* message) noexcept {
    // Attempt to print the panic message to the console.
    // This assumes the console has been initialized.
    // If console is not ready, this might not work, but it's the best effort.
    // A more robust panic might try a very raw, direct UART write if console fails.

    // Disable interrupts (platform specific) - important to prevent further execution
    // For AArch64: msr daifset, #0xf
    asm volatile("msr daifset, #0xf" ::: "memory");

    global_console().println("\n*** KERNEL PANIC ***");
    if (message) {
        global_console().print("Message: ");
        global_console().println(message);
    } else {
        global_console().println("No message provided.");
    }
    global_console().println("System halted.");

    // Halt the system.
    // This can be an infinite loop with interrupts disabled.
    // Or a specific halt instruction if available and appropriate.
    while (true) {
        // WFI (Wait For Interrupt) is a low-power way to halt.
        // Since interrupts are disabled, this effectively halts.
        asm volatile("wfi");
    }
}

} // namespace Kernel

/*
// Optional assembly helper for a clean halt, could be in start.S or a separate .S file
// Define _hang if you want to call an assembly halt.
.global _hang
_hang:
    wfi
    b _hang
*/
