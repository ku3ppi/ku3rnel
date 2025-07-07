#ifndef LIB_PRINTF_H
#define LIB_PRINTF_H

#include <kstd/cstddef.h> // For kstd::size_t
#include <kstd/cstdint.h> // For integer types

// Define __builtin_va_list, __builtin_va_start, __builtin_va_arg, __builtin_va_end
// if not available. For GCC/Clang, these are usually built-in.
// If targeting another compiler, specific va_list macros would be needed.
#ifndef __builtin_va_list
    // This is a simplified example, a real va_list is compiler-specific
    typedef char* __builtin_va_list;
    #define __builtin_va_start(v,l) v = (char*)((&l) + 1)
    // Align to a type's size. For simplicity, assume alignment is sizeof(int).
    #define __VA_ALIGN(type) ((sizeof(type) + sizeof(int) - 1) & ~(sizeof(int) - 1))
    #define __builtin_va_arg(v,type) (v += __VA_ALIGN(type), *(type*)(v - __VA_ALIGN(type)))
    #define __builtin_va_end(v) v = nullptr
#endif


namespace Kernel {

// The kprintf function will output to the global console.
// It supports a limited set of format specifiers:
// %c - char
// %s - string (char*)
// %d, %i - signed int
// %u - unsigned int
// %x, %X - hexadecimal unsigned int (lowercase/uppercase)
// %p - pointer (void*) (prints as hex)
// %b - binary unsigned int
// %% - literal '%'
// Length modifiers (l, ll) are not supported in this minimal version.
// Width and precision are not supported.
void kprintf(const char* format, ...);

// A version of ksnprintf that writes to a character buffer.
// Returns the number of characters that would have been written if buffer was large enough,
// (excluding null terminator), or a negative value on error.
// Similar to standard snprintf.
int ksnprintf(char* buffer, kstd::size_t buffer_size, const char* format, ...);

// Core formatting function used by kprintf and ksnprintf.
// Takes a function pointer 'output_char_func' to output characters one by one.
// 'output_context' is a user-defined pointer passed to output_char_func.
int kvprintf_core(void (*output_char_func)(char, void*), void* output_context,
                  kstd::size_t buffer_limit, // 0 for unlimited (like kprintf), >0 for ksnprintf
                  const char* format, __builtin_va_list args);

} // namespace Kernel


// Make kprintf available in the global Kernel namespace for easy use.
// Also, potentially a global ::kprintf alias if desired, though namespacing is good.

#endif // LIB_PRINTF_H
