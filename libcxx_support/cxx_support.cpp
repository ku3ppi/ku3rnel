#include <libcxx_support/cxx_support.h>
#include <kernel/panic.h> // For kernel_panic
#include <kstd/cstdint.h> // For uintptr_t

// Simple bump allocator
static unsigned char* heap_current = nullptr;
static unsigned char* heap_end_addr = nullptr;
static bool allocator_initialized = false;

// Alignment helper
constexpr kstd::size_t ALIGNMENT = 8; // Assume 8-byte alignment for general purpose
inline kstd::size_t align_up(kstd::size_t size, kstd::size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

void Kernel::LibCXX::init_allocator(void* heap_start_ptr, kstd::size_t heap_size) {
    if (!heap_start_ptr || heap_size == 0) {
        // Cannot initialize allocator without a valid heap region
        // This is a critical error, but panic might not be available yet depending on call order.
        // For now, we'll just set a flag and new will fail.
        // A better approach is to ensure this is called after basic console for panic is up.
        allocator_initialized = false;
        return;
    }
    heap_current = static_cast<unsigned char*>(heap_start_ptr);
    // Ensure heap_current is aligned
    kstd::uintptr_t current_addr = reinterpret_cast<kstd::uintptr_t>(heap_current);
    kstd::uintptr_t aligned_addr = (current_addr + ALIGNMENT -1) & ~(ALIGNMENT -1);
    heap_current = reinterpret_cast<unsigned char*>(aligned_addr);

    heap_end_addr = static_cast<unsigned char*>(heap_start_ptr) + heap_size;
    if (heap_current >= heap_end_addr) { // Not enough space after alignment
        allocator_initialized = false;
    } else {
        allocator_initialized = true;
    }
}

void* operator new(kstd::size_t size) {
    if (!allocator_initialized || !heap_current) {
        // Consider a panic or a more graceful way to handle this.
        // For now, returning nullptr indicates failure.
        // Kernel::panic("Allocator not initialized or heap_current is null in new");
        return nullptr;
    }

    kstd::size_t aligned_size = align_up(size, ALIGNMENT);
    if (heap_current + aligned_size > heap_end_addr) {
        // Out of memory
        // Kernel::panic("Kernel heap OOM!"); // Or return nullptr
        return nullptr;
    }

    void* block = heap_current;
    heap_current += aligned_size;
    return block;
}

void* operator new[](kstd::size_t size) {
    return ::operator new(size);
}

// In a simple bump allocator, delete does nothing.
// This is obviously not suitable for long-running systems with frequent allocations/deallocations.
// For a bare-metal kernel, this might be acceptable if dynamic memory is used sparingly
// or if a more sophisticated allocator is implemented later.
void operator delete(void* ptr) noexcept {
    (void)ptr; // Unused
    // With a bump allocator, individual deallocations are typically not supported.
    // The entire heap region would be reset if needed, or a proper allocator used.
}

void operator delete[](void* ptr) noexcept {
    ::operator delete(ptr);
}

void operator delete(void* ptr, kstd::size_t size) noexcept {
    (void)ptr;  // Unused
    (void)size; // Unused
    // As above, bump allocator doesn't support individual deallocations.
}

void operator delete[](void* ptr, kstd::size_t size) noexcept {
    ::operator delete(ptr, size);
}


// ABI functions
extern "C" {
    void __cxa_pure_virtual() {
        // This function is called when a pure virtual function is called.
        // This should ideally trigger a kernel panic.
        Kernel::panic("Pure virtual function call!");
        while(1); // Loop indefinitely if panic returns or is not available
    }

    int __cxa_atexit(void (*func)(void *), void *arg, void *dso_handle) {
        (void)func;
        (void)arg;
        (void)dso_handle;
        // In a kernel, atexit is generally not used or supported.
        // Return 0 for success (as if the handler was registered).
        return 0;
    }

    // Basic stubs for guard variables. These are not thread-safe.
    // A real implementation would require synchronization primitives.
    // For single-threaded early kernel init, this might be sufficient.
    // `guard_object` is a pointer to a 64-bit integer.
    // The first byte is a flag: 0 = not initialized, 1 = initialized.
    /*
    int __cxa_guard_acquire(long long *guard_object) {
        // If the first byte is 0, set it to something else (e.g., current thread ID, or just -1)
        // and return 1 (meaning caller should initialize).
        // If already non-zero, return 0 (meaning someone else is initializing or it's done).
        // This is a placeholder and NOT THREAD SAFE.
        if (*(reinterpret_cast<char*>(guard_object)) == 0) {
            *(reinterpret_cast<char*>(guard_object)) = 1; // Mark as initialization started by this "thread"
            return 1; // Proceed with initialization
        }
        return 0; // Already initialized or being initialized
    }

    void __cxa_guard_release(long long *guard_object) {
        // Mark as fully initialized.
        *(reinterpret_cast<char*>(guard_object)) = 1; // Mark as initialized
    }

    void __cxa_guard_abort(long long *guard_object) {
        // Initialization failed, reset the flag.
        *(reinterpret_cast<char*>(guard_object)) = 0; // Mark as not initialized
    }
    */
    // Simpler stubs if the above are not needed by the toolchain for basic static init:
    // Sometimes these are not linked if there are no static C++ objects needing complex initialization.
    // If linker errors occur for these, they might need to be implemented or linked from a support library.
} // extern "C"


void Kernel::LibCXX::abort() {
    // This function is called for unrecoverable errors, e.g., by std::terminate.
    Kernel::panic("Kernel abort() called!");
    while(1); // Loop indefinitely
}

bool Kernel::LibCXX::is_allocator_initialized() {
    return allocator_initialized;
}

// Need to provide a definition for the kstd::size_t related items if they are not just typedefs
// However, kstd/cstddef.h should provide size_t.
// If linking errors for `kstd::size_t` occur, it means it's being treated as a distinct type
// needing its own definitions, which is unlikely if it's just a typedef.
// For now, assume `kstd::size_t` is correctly defined as a typedef (e.g., to `unsigned long`).

// We also need a basic `kstd/cstddef.h` for `size_t` and `nullptr_t`.
// And `kstd/cstdint.h` for `uintptr_t`.
// I will create placeholder for these in step 5.
// For now, this file assumes they will exist.
// Let's also create the `kernel/panic.h` include that is used.
// It will be fully implemented in step 5.
// And `kstd/cstdint.h`
