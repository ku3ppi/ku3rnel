#ifndef LIBCXX_SUPPORT_H
#define LIBCXX_SUPPORT_H

#include <kstd/cstddef.h> // For size_t

// Basic new and delete operators
void* operator new(kstd::size_t size);
void* operator new[](kstd::size_t size);
void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;
void operator delete(void* ptr, kstd::size_t size) noexcept;
void operator delete[](void* ptr, kstd::size_t size) noexcept;

// Placement new/delete (usually provided by <new> header, but we are freestanding)
inline void* operator new(kstd::size_t, void* p) noexcept { return p; }
inline void* operator new[](kstd::size_t, void* p) noexcept { return p; }
inline void operator delete(void*, void*) noexcept { }
inline void operator delete[](void*, void*) noexcept { }


// ABI functions required by C++
extern "C" {
    // Called for pure virtual function calls
    void __cxa_pure_virtual();

    // Placeholder for atexit (not typically used in kernels, but linker might look for it)
    // We don't actually register any handlers.
    int __cxa_atexit(void (*func)(void *), void *arg, void *dso_handle);

    // Placeholder for guard variables for static initialization (not fully implemented here)
    // For simple cases, the linker might handle static initializers without these.
    // If complex static constructors are used, a more robust implementation is needed.
    // int __cxa_guard_acquire(long long *guard_object);
    // void __cxa_guard_release(long long *guard_object);
    // void __cxa_guard_abort(long long *guard_object);

    // Add other ABI functions if the linker complains, e.g., related to thread-local storage or exceptions.
}

namespace Kernel {
namespace LibCXX {

// Initializes the bump allocator for new/delete.
// Should be called once very early during kernel startup, after memory map is known.
void init_allocator(void* heap_start, kstd::size_t heap_size);

// Check if the allocator was successfully initialized.
bool is_allocator_initialized();

// A simple abort function.
void abort();

} // namespace LibCXX
} // namespace Kernel

#endif // LIBCXX_SUPPORT_H
