#include "cstring.h"

namespace kstd {

void* kmemmove(void* dest, const void* src, kstd::size_t count) {
    unsigned char* d = static_cast<unsigned char*>(dest);
    const unsigned char* s = static_cast<const unsigned char*>(src);

    if (d == s) {
        return dest;
    }

    if (d < s) {
        // Copy forwards
        for (kstd::size_t i = 0; i < count; ++i) {
            d[i] = s[i];
        }
    } else {
        // Copy backwards (handles overlap if dest > src)
        for (kstd::size_t i = count; i > 0; --i) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

void* kmemcpy(void* dest, const void* src, kstd::size_t count) {
    // For non-overlapping regions, kmemmove works fine.
    // Some implementations might offer a slightly faster version if no overlap is guaranteed.
    // For simplicity, we can just call kmemmove or implement it directly.
    unsigned char* d = static_cast<unsigned char*>(dest);
    const unsigned char* s = static_cast<const unsigned char*>(src);
    for (kstd::size_t i = 0; i < count; ++i) {
        d[i] = s[i];
    }
    return dest;
}

void* kmemset(void* dest, int ch, kstd::size_t count) {
    unsigned char* d = static_cast<unsigned char*>(dest);
    unsigned char val = static_cast<unsigned char>(ch);
    for (kstd::size_t i = 0; i < count; ++i) {
        d[i] = val;
    }
    return dest;
}

int kmemcmp(const void* lhs, const void* rhs, kstd::size_t count) {
    const unsigned char* l = static_cast<const unsigned char*>(lhs);
    const unsigned char* r = static_cast<const unsigned char*>(rhs);
    for (kstd::size_t i = 0; i < count; ++i) {
        if (l[i] < r[i]) return -1;
        if (l[i] > r[i]) return 1;
    }
    return 0;
}

kstd::size_t kstrlen(const char* str) {
    kstd::size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

int kstrcmp(const char* lhs, const char* rhs) {
    while (*lhs && (*lhs == *rhs)) {
        lhs++;
        rhs++;
    }
    return static_cast<int>(*reinterpret_cast<const unsigned char*>(lhs)) -
           static_cast<int>(*reinterpret_cast<const unsigned char*>(rhs));
}

int kstrncmp(const char* lhs, const char* rhs, kstd::size_t count) {
    if (count == 0) {
        return 0;
    }
    while (count-- > 0 && *lhs && (*lhs == *rhs)) {
        if (count == 0) break; // Reached max count
        lhs++;
        rhs++;
    }
    // If loop finished due to count, or one string ended but they were equal up to that point.
    if (count == static_cast<kstd::size_t>(-1) || (count > 0 && *lhs == *rhs) ) { // count wrapped for -- > 0 on last iteration
         return 0;
    }
     if (count == 0 && *lhs == *rhs) return 0;


    return static_cast<int>(*reinterpret_cast<const unsigned char*>(lhs)) -
           static_cast<int>(*reinterpret_cast<const unsigned char*>(rhs));
}


char* kstrcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++) != '\0') {
        // Loop body is empty
    }
    return dest;
}

char* kstrncpy(char* dest, const char* src, kstd::size_t count) {
    char* d = dest;
    kstd::size_t i;

    // Copy characters from src to dest
    for (i = 0; i < count && src[i] != '\0'; ++i) {
        d[i] = src[i];
    }

    // If src was shorter than count, pad with nulls
    for (; i < count; ++i) {
        d[i] = '\0';
    }
    return dest;
}

char* kstrcat(char* dest, const char* src) {
    char* d = dest;
    // Move to the end of dest string
    while (*d != '\0') {
        d++;
    }
    // Copy src to the end of dest
    while ((*d++ = *src++) != '\0') {
        // Loop body is empty
    }
    return dest;
}

const char* kstrchr(const char* str, int ch) {
    char c = static_cast<char>(ch);
    while (*str != '\0') {
        if (*str == c) {
            return str;
        }
        str++;
    }
    if (c == '\0') { // Also find null terminator
        return str;
    }
    return nullptr;
}

char* kstrchr(char* str, int ch) {
    // Call the const version and cast away constness.
    // This is safe if the original char* was non-const.
    return const_cast<char*>(kstrchr(static_cast<const char*>(str), ch));
}


} // namespace kstd
