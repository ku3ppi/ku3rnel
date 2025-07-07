#ifndef KSTD_CSTDDEF_H
#define KSTD_CSTDDEF_H

// Defines size_t and ptrdiff_t
// On AArch64, size_t and ptrdiff_t are typically 64-bit.
namespace kstd {

#if defined(__LP64__) || defined(_LP64) || defined(__aarch64__)
    typedef unsigned long size_t;
    typedef long ptrdiff_t;
#else // Assume 32-bit if not explicitly 64-bit
    typedef unsigned int size_t;
    typedef int ptrdiff_t;
#endif

    // nullptr_t definition
    // The type of `nullptr`
    typedef decltype(nullptr) nullptr_t;

    // offsetof macro (simplified, might need compiler intrinsic for full C++ compliance)
    // Using __builtin_offsetof if available, otherwise a common trick.
    #ifdef __GNUC__
        #define koffsetof(type, member) __builtin_offsetof(type, member)
    #else
        // This is a common way to implement offsetof but can have issues with non-POD types in C++.
        // For kernel code, types are usually simpler.
        #define koffsetof(type, member) ((kstd::size_t)&reinterpret_cast<const volatile char&>((((type*)0)->member)))
    #endif

} // namespace kstd


// Define NULL, typically in cstddef or cstdlib
#ifndef NULL
    #ifdef __cplusplus
        #define NULL nullptr
    #else
        #define NULL ((void*)0)
    #endif
#endif


#endif // KSTD_CSTDDEF_H
