#include <kstd/cstddef.h>    // For kstd::nullptr_t, kstd::size_t
#include <kstd/cstdint.h>   // For kstd::uintptr_t etc.
#include <libcxx_support/cxx_support.h> // For Kernel::LibCXX::init_allocator
#include <kernel/console.h> // For Kernel::global_console()
#include <kernel/interrupt.h> // For Kernel::get_interrupt_controller()
#include <arch/arm/peripherals/uart.h> // For Arch::RaspberryPi::uart_init_global()
#include <arch/arm/peripherals/timer.h> // For Arch::RaspberryPi::system_timer_init_global
#include <arch/arm/core/gic.h>      // For Arch::Arm::gic_init_global()
#include <kernel/filesystem/filesystem.h> // For Kernel::global_filesystem()

// Forward declare init_exceptions if not in a common Arch header
// extern "C" void init_exceptions(); // Defined in exceptions.cpp
namespace Arch { namespace Arm { void init_exceptions_impl(); } } // Wrapper in C++
void init_exceptions() { // Simple C-style wrapper if needed by pure C contexts
    Arch::Arm::init_exceptions_impl();
}
// Actually, exceptions.cpp provides `init_exceptions()`, so just declare it.
extern "C" void init_exceptions();


// Linker script symbols for heap region.
// These are typically defined as char arrays to get their addresses.
extern "C" {
    extern char HEAP_START[]; // Address of the start of the heap
    extern char HEAP_END[];   // Address of the end of the heap
}


// Kernel main function - entry point from assembly (_start_kernel)
extern "C" void kernel_main(kstd::uintptr_t dtb_ptr32, kstd::uint64_t x1, kstd::uint64_t x2, kstd::uint64_t x3) {
    // Arguments from AArch64 boot sequence (x0-x3)
    // x0 (dtb_ptr32) often holds the 32-bit physical address of the Device Tree Blob (DTB).
    // For RPi, it's usually passed in r0/x0.
    // The other registers (x1-x3) might hold other info or be zero.
    (void)dtb_ptr32; (void)x1; (void)x2; (void)x3; // Mark as unused for now

    // 1. Initialize the C++ dynamic memory allocator (new/delete).
    // The HEAP_START and HEAP_END symbols are defined in the linker script.
    // Their addresses give us the bounds of our pre-allocated kernel heap.
    kstd::size_t heap_size = (kstd::uintptr_t)HEAP_END - (kstd::uintptr_t)HEAP_START;
    Kernel::LibCXX::init_allocator(HEAP_START, heap_size);

    // 2. Initialize the main console.
    // This will internally initialize the UART and GPIO pins for UART.
    Kernel::global_console().init();
    Kernel::kprintf("Kernel Console Initialized.\n");

    // 3. Initialize and Enable MMU
    // This sets up identity mapping for the first 2GB.
    Arch::Arm::MMU::init_and_enable();
    Kernel::kprintf("MMU Initialized and Enabled.\n");


    // 4. Initialize exception handling (set VBAR_EL1)
    // VBAR_EL1 should point to the virtual address of _exception_vectors.
    // With identity mapping, virtual == physical for this kernel region.
    init_exceptions(); // Calls function from exceptions.cpp to set VBAR_EL1

    // 5. Initialize Interrupt Controller (GIC)
    // This also enables CPU interrupts AFTER GIC is ready.
    Arch::Arm::gic_init_global();


    // 5. Initialize System Timer (ARM Generic Timer via CNTP_EL1)
    // Example: 1 Hz timer tick
    void timer_callback(unsigned int irq, void* ctx); // Forward declaration
    Arch::RaspberryPi::system_timer_init_global(1, timer_callback, nullptr); // 1 Hz timer
    Kernel::kprintf("System timer initialized (1 Hz).\n");

    // 6. Initialize Filesystem
    Kernel::global_filesystem().init();
    Kernel::kprintf("In-memory filesystem initialized.\n");
    // global_filesystem().list_files_to_console(); // Optional: list files at boot for debug


    // Print a welcome message using the new Console
    Kernel::kprintf("KEKOS C++ Kernel: Booting...\n");
    Kernel::kprintf("kernel_main reached. DTB at 0x%llx (passed as x0/dtb_ptr32)\n", dtb_ptr32);

    if (heap_size > 0 && Kernel::LibCXX::init_allocator_was_successful_for_testing()) {
         Kernel::kprintf("Heap allocator initialized (size: %u bytes).\n", heap_size);
         // Test allocation
         int* test_alloc = new int;
         if (test_alloc) {
             *test_alloc = 12345;
             if (*test_alloc == 12345) {
                 Kernel::global_console().println("Dynamic allocation test PASSED.");
             } else {
                 Kernel::global_console().println("Dynamic allocation test FAILED (value check).");
             }
             delete test_alloc; // Bump allocator delete is a NOP
         } else {
             Kernel::global_console().println("Dynamic allocation test FAILED (nullptr returned by new).");
         }
    } else {
        Kernel::global_console().println("Heap allocator NOT initialized or size is zero.");
    }

    Kernel::global_console().println("Kernel setup complete. Entering idle loop.");
    Kernel::global_console().println("You should see this text on your serial console (e.g., minicom, PuTTY).");
    Kernel::global_console().println("---");


    // Simple echo test loop
    Kernel::global_console().println("Starting echo test. Type something:");
    char input_buffer[128];
    while(true) {
        Kernel::global_console().print("> ");
        kstd::size_t len = Kernel::global_console().read_line(input_buffer, 128);
        if (len > 0) {
            Kernel::global_console().print("Echo: ");
            Kernel::global_console().println(input_buffer);
        }
        if (kstd::strcmp(input_buffer, "exit") == 0) {
             Kernel::global_console().println("Exiting echo test loop.");
             break;
        }
    }

    Kernel::global_console().println("Kernel idle loop (after echo test). Timer ticks should print every second.");
    Kernel::global_console().println("---"); // Separator before shell starts

    // 7. Start the Kernel Shell
    // This function will typically not return unless the shell is explicitly exited.
    Kernel::start_kernel_shell();


    // If shell exits (e.g., via a special "exit" or "shutdown" command that stops the shell loop),
    // we'll fall through to here.
    Kernel::kprintf("Kernel main: Shell has exited. Halting system.\n");
    Kernel::panic("Kernel shell exited normally."); // Or just halt indefinitely.
    // while (true) {
    //     asm volatile("wfi");
    // }
}


// Timer callback function
static volatile kstd::uint64_t timer_tick_count = 0;
void timer_callback(unsigned int irq, void* ctx) {
    (void)irq; // Should be the timer's IRQ
    (void)ctx; // Context not used in this simple example
    timer_tick_count++;
    Kernel::kprintf("Timer tick %llu\n", timer_tick_count);

    // Note: kprintf in an ISR is generally okay for debugging but can cause issues
    // if kprintf itself is not re-entrant or uses locks that might be held by interrupted code.
    // Our current kprintf is fairly simple and writes directly to UART (via Console).
}


// Helper for LibCXX to check initialization status for the test print
// This is defined in libcxx_support/cxx_support.cpp and declared in its header.
// No need to redefine it here. We just need to ensure it's linked.
// If it's not found, add `bool Kernel::LibCXX::allocator_ready = false;` to cxx_support.cpp
// and `extern bool allocator_ready;` to cxx_support.h if it's not already there.
// For the function:
namespace Kernel {
namespace LibCXX {
    // This function is defined in cxx_support.cpp, just need to ensure it's declared if used.
    // extern bool allocator_ready; // This should be in cxx_support.cpp
    bool init_allocator_was_successful_for_testing() { // Keep old name for now to minimize changes here
        return Kernel::LibCXX::is_allocator_initialized(); // Call the proper function
    }
}
}

// kstd::strcmp is now provided by lib/kstd/cstring.cpp
// No need for the temporary version here.
