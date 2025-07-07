#include "printf.h"
#include <kernel/console.h> // For Kernel::global_console()
#include <kstd/cstring.h>   // For kstrlen
#include <kstd/cstddef.h>   // For kstd::size_t
#include <kstd/cstdint.h>   // For integer types

namespace Kernel {

// Context for kprintf outputting to global_console
struct KPrintfContext {
    // No specific context needed for global_console, but could be used
    // if kprintf needed to write to different console instances.
};

// Output char function for kprintf (writes to global console)
static void kprintf_output_char_func(char c, void* context) {
    (void)context; // Unused for global_console
    Kernel::global_console().put_char(c);
}


// Context for ksnprintf outputting to a buffer
struct KsnprintfContext {
    char* buffer;
    kstd::size_t buffer_pos;
    kstd::size_t buffer_max_size; // Max characters to write (excluding null terminator)
    kstd::size_t chars_written_total; // Total chars that *would* be written
};

// Output char function for ksnprintf (writes to buffer)
static void ksnprintf_output_char_func(char c, void* context) {
    KsnprintfContext* ctx = static_cast<KsnprintfContext*>(context);

    if (ctx->buffer_pos < ctx->buffer_max_size) {
        ctx->buffer[ctx->buffer_pos++] = c;
    }
    ctx->chars_written_total++;
}


// Helper to print an integer (decimal, hex, binary)
// Returns number of characters printed.
static int print_integer(void (*output_char_func)(char, void*), void* output_context,
                         kstd::size_t& current_chars_count, kstd::size_t buffer_limit,
                         long long val, int base, bool uppercase_hex, bool is_signed) {

    char buffer[65]; // Max for 64-bit binary + null terminator
    int buf_idx = 64;
    buffer[buf_idx--] = '\0';

    unsigned long long u_val;
    bool negative = false;

    if (is_signed && val < 0) {
        negative = true;
        u_val = static_cast<unsigned long long>(-val);
    } else {
        u_val = static_cast<unsigned long long>(val);
    }

    if (u_val == 0) {
        buffer[buf_idx--] = '0';
    } else {
        while (u_val > 0) {
            unsigned int digit = u_val % base;
            if (digit < 10) {
                buffer[buf_idx--] = (char)('0' + digit);
            } else {
                buffer[buf_idx--] = (char)((uppercase_hex ? 'A' : 'a') + (digit - 10));
            }
            u_val /= base;
        }
    }

    if (negative && base == 10) { // Only print sign for base 10 signed
        buffer[buf_idx--] = '-';
    }

    int chars_printed_this_num = 0;
    for (int i = buf_idx + 1; buffer[i] != '\0'; ++i) {
        if (buffer_limit == 0 || current_chars_count < buffer_limit) {
            output_char_func(buffer[i], output_context);
            current_chars_count++; // This is for ksnprintf's internal buffer tracking
        }
        chars_printed_this_num++; // This is for the return value of kvprintf_core
    }
    return chars_printed_this_num;
}


int kvprintf_core(void (*output_char_func)(char, void*), void* output_context,
                  kstd::size_t buffer_limit, // 0 for unlimited (kprintf), >0 for ksnprintf (actual buffer size - 1)
                  const char* format, __builtin_va_list args) {

    kstd::size_t total_chars_emitted = 0; // For ksnprintf, this is chars that would be written to an infinite buffer
                                        // For kprintf, this is actual chars written
    kstd::size_t chars_written_to_buffer = 0; // For ksnprintf, actual chars placed in buffer

    if (!format) return -1; // Invalid format string

    for (const char* p = format; *p; ++p) {
        if (*p != '%') {
            if (buffer_limit == 0 || chars_written_to_buffer < buffer_limit) {
                output_char_func(*p, output_context);
                chars_written_to_buffer++;
            }
            total_chars_emitted++;
            continue;
        }

        // We encountered a '%'
        p++; // Move past '%'

        // TODO: Add support for length modifiers (l, ll, h, hh, z, j, t) if needed.
        // For now, assume default sizes (int for d/i/u/x/X/b, char* for s, void* for p)

        switch (*p) {
            case '\0': // Format string ends with '%'
                if (buffer_limit == 0 || chars_written_to_buffer < buffer_limit) {
                     output_char_func('%', output_context); // Output the literal '%'
                     chars_written_to_buffer++;
                }
                total_chars_emitted++;
                goto end_loop; // End processing

            case '%': // Literal '%%'
                if (buffer_limit == 0 || chars_written_to_buffer < buffer_limit) {
                    output_char_func('%', output_context);
                    chars_written_to_buffer++;
                }
                total_chars_emitted++;
                break;

            case 'c': {
                char c_val = (char)__builtin_va_arg(args, int); // char promotes to int
                if (buffer_limit == 0 || chars_written_to_buffer < buffer_limit) {
                    output_char_func(c_val, output_context);
                    chars_written_to_buffer++;
                }
                total_chars_emitted++;
                break;
            }

            case 's': {
                const char* s_val = __builtin_va_arg(args, const char*);
                if (!s_val) s_val = "(null)";
                while (*s_val) {
                    if (buffer_limit == 0 || chars_written_to_buffer < buffer_limit) {
                        output_char_func(*s_val, output_context);
                        chars_written_to_buffer++;
                    }
                    total_chars_emitted++;
                    s_val++;
                }
                break;
            }

            case 'd':
            case 'i': {
                int val = __builtin_va_arg(args, int);
                total_chars_emitted += print_integer(output_char_func, output_context, chars_written_to_buffer, buffer_limit, val, 10, false, true);
                break;
            }

            case 'u': {
                unsigned int val = __builtin_va_arg(args, unsigned int);
                total_chars_emitted += print_integer(output_char_func, output_context, chars_written_to_buffer, buffer_limit, val, 10, false, false);
                break;
            }

            case 'x':
            case 'X': {
                unsigned int val = __builtin_va_arg(args, unsigned int);
                total_chars_emitted += print_integer(output_char_func, output_context, chars_written_to_buffer, buffer_limit, val, 16, (*p == 'X'), false);
                break;
            }

            case 'p': { // Pointer - print as hex. Assume void* is same size as unsigned long or long long.
                        // Prefix with "0x"
                void* ptr_val = __builtin_va_arg(args, void*);
                if (buffer_limit == 0 || chars_written_to_buffer < buffer_limit) { output_char_func('0', output_context); chars_written_to_buffer++; }
                total_chars_emitted++;
                if (buffer_limit == 0 || chars_written_to_buffer < buffer_limit) { output_char_func('x', output_context); chars_written_to_buffer++; }
                total_chars_emitted++;

                // Treat pointer as unsigned long long for printing its value
                total_chars_emitted += print_integer(output_char_func, output_context, chars_written_to_buffer, buffer_limit,
                                       reinterpret_cast<kstd::uintptr_t>(ptr_val), 16, true, false); // Uppercase hex often standard for pointers
                break;
            }

            case 'b': { // Binary
                unsigned int val = __builtin_va_arg(args, unsigned int);
                total_chars_emitted += print_integer(output_char_func, output_context, chars_written_to_buffer, buffer_limit, val, 2, false, false);
                break;
            }

            default: // Unknown format specifier, print it literally
                if (buffer_limit == 0 || chars_written_to_buffer < buffer_limit) {
                    output_char_func('%', output_context);
                    chars_written_to_buffer++;
                }
                total_chars_emitted++;
                if (*p) { // If not end of string
                     if (buffer_limit == 0 || chars_written_to_buffer < buffer_limit) {
                        output_char_func(*p, output_context);
                        chars_written_to_buffer++;
                     }
                     total_chars_emitted++;
                }
                break;
        }
    }

end_loop:
    return static_cast<int>(total_chars_emitted);
}


void kprintf(const char* format, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, format);
    // For kprintf, buffer_limit is 0 (unlimited actual output)
    // The KPrintfContext is not strictly needed here as global_console is used directly.
    kvprintf_core(kprintf_output_char_func, nullptr, 0, format, args);
    __builtin_va_end(args);
}

int ksnprintf(char* buffer, kstd::size_t buffer_size, const char* format, ...) {
    if (!buffer || buffer_size == 0) {
        // Need to calculate length even if buffer is null/zero, as per snprintf spec.
        // However, our current kvprintf_core needs a valid output func.
        // For simplicity, if buffer is invalid, we can return error or 0.
        // Let's mimic typical snprintf: if buffer_size is 0, buffer may be null,
        // and it calculates required length.
        // If buffer is NULL and buffer_size > 0, behavior is undefined by POSIX.
        // We will require a valid (even if zero-sized) buffer context for calculation.
        if (buffer_size == 0) buffer = nullptr; // Allow null buffer if size is 0
        else if (!buffer) return -1; // Invalid arguments
    }

    KsnprintfContext ctx;
    ctx.buffer = buffer;
    ctx.buffer_pos = 0;
    // buffer_max_size is actual chars that can be written before null terminator
    ctx.buffer_max_size = (buffer_size > 0) ? (buffer_size - 1) : 0;
    ctx.chars_written_total = 0;

    __builtin_va_list args;
    __builtin_va_start(args, format);
    // Pass ctx.buffer_max_size as the limit for kvprintf_core
    kvprintf_core(ksnprintf_output_char_func, &ctx, ctx.buffer_max_size, format, args);
    __builtin_va_end(args);

    // Null-terminate the buffer if space allows
    if (buffer && buffer_size > 0) {
        if (ctx.buffer_pos < buffer_size) {
            buffer[ctx.buffer_pos] = '\0';
        } else {
            // Ensure null termination even if content was truncated
            buffer[buffer_size - 1] = '\0';
        }
    }
    return static_cast<int>(ctx.chars_written_total);
}


// Update Console::kprintf to use the new kprintf
// This will be done by modifying console.cpp next.

} // namespace Kernel
