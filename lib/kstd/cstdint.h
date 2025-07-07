#ifndef KSTD_CSTDINT_H
#define KSTD_CSTDINT_H

// Fixed-width integer types
// These definitions assume a typical LP64 or ILP32 data model.
// AArch64 is LP64 (long, pointer are 64-bit).

namespace kstd {

// Exact-width integer types
typedef signed char         int8_t;
typedef unsigned char       uint8_t;
typedef short               int16_t;
typedef unsigned short      uint16_t;
typedef int                 int32_t;
typedef unsigned int        uint32_t;

#if defined(__LP64__) || defined(_LP64) || defined(__aarch64__)
    typedef long                int64_t;
    typedef unsigned long       uint64_t;
#elif defined(__GNUC__) || defined(__clang__)
    typedef long long           int64_t;
    typedef unsigned long long  uint64_t;
#else
    // Fallback for compilers where long long might not be available or long is not 64-bit
    // This might need adjustment based on the specific toolchain if not GCC/Clang compatible.
    #error "Unsupported compiler for 64-bit integer types. Please define int64_t and uint64_t."
#endif

// Minimum-width integer types
typedef int8_t    int_least8_t;
typedef uint8_t   uint_least8_t;
typedef int16_t   int_least16_t;
typedef uint16_t  uint_least16_t;
typedef int32_t   int_least32_t;
typedef uint32_t  uint_least32_t;
typedef int64_t   int_least64_t;
typedef uint64_t  uint_least64_t;

// Fastest minimum-width integer types
// These are usually the same as the exact-width types for common architectures.
typedef int8_t    int_fast8_t;
typedef uint8_t   uint_fast8_t;
typedef int16_t   int_fast16_t;
typedef uint16_t  uint_fast16_t;
typedef int32_t   int_fast32_t;
typedef uint32_t  uint_fast32_t;
typedef int64_t   int_fast64_t;
typedef uint64_t  uint_fast64_t;

// Integer types capable of holding object pointers
#if defined(__LP64__) || defined(_LP64) || defined(__aarch64__)
    typedef long            intptr_t;
    typedef unsigned long   uintptr_t;
#else // Assume 32-bit pointers
    typedef int             intptr_t;
    typedef unsigned int    uintptr_t;
#endif

// Greatest-width integer types
typedef int64_t   intmax_t;
typedef uint64_t  uintmax_t;

} // namespace kstd

// Macros for integer constants (optional, but good practice)
#define INT8_C(value)   (value)
#define UINT8_C(value)  (value ## U)
#define INT16_C(value)  (value)
#define UINT16_C(value) (value ## U)
#define INT32_C(value)  (value)
#define UINT32_C(value) (value ## U)

#if defined(__LP64__) || defined(_LP64) || defined(__aarch64__)
    #define INT64_C(value)  (value ## L)
    #define UINT64_C(value) (value ## UL)
#else
    #define INT64_C(value)  (value ## LL)
    #define UINT64_C(value) (value ## ULL)
#endif

#define INTMAX_C(value)  INT64_C(value)
#define UINTMAX_C(value) UINT64_C(value)


#endif // KSTD_CSTDINT_H
