#ifndef KSTD_CSTRING_H
#define KSTD_CSTRING_H

#include <kstd/cstddef.h> // For kstd::size_t

namespace kstd {

// Copies 'count' bytes from 'src' to 'dest'. The memory areas may overlap.
void* kmemmove(void* dest, const void* src, kstd::size_t count);

// Copies 'count' bytes from 'src' to 'dest'. The memory areas must not overlap.
void* kmemcpy(void* dest, const void* src, kstd::size_t count);

// Fills the first 'count' bytes of the memory area pointed to by 'dest' with the constant byte 'ch'.
void* kmemset(void* dest, int ch, kstd::size_t count);

// Compares the first 'count' bytes of the memory areas 'lhs' and 'rhs'.
// Returns 0 if equal, <0 if lhs < rhs, >0 if lhs > rhs.
int kmemcmp(const void* lhs, const void* rhs, kstd::size_t count);

// Returns the length of the null-terminated string 'str'.
kstd::size_t kstrlen(const char* str);

// Compares two null-terminated strings 'lhs' and 'rhs'.
// Returns 0 if equal, <0 if lhs < rhs, >0 if lhs > rhs.
int kstrcmp(const char* lhs, const char* rhs);

// Compares at most 'count' characters of two null-terminated strings 'lhs' and 'rhs'.
// Returns 0 if equal, <0 if lhs < rhs, >0 if lhs > rhs.
int kstrncmp(const char* lhs, const char* rhs, kstd::size_t count);

// Copies the null-terminated string 'src' (including null terminator) to 'dest'.
// Assumes 'dest' is large enough. Returns 'dest'.
char* kstrcpy(char* dest, const char* src);

// Copies at most 'count' characters from 'src' to 'dest'.
// If 'src' is shorter than 'count', 'dest' is padded with nulls until 'count' chars are written.
// If 'src' is 'count' or more, 'dest' will not be null-terminated if src[count-1] is not null.
// Returns 'dest'.
char* kstrncpy(char* dest, const char* src, kstd::size_t count);

// Concatenates 'src' string to the end of 'dest' string.
// 'dest' must be large enough. Null terminator from 'dest' is overwritten by first char of 'src'.
// Returns 'dest'.
char* kstrcat(char* dest, const char* src);

// Searches for the first occurrence of 'ch' (converted to char) in 'str'.
// Returns a pointer to the first occurrence, or nullptr if not found.
const char* kstrchr(const char* str, int ch);
char* kstrchr(char* str, int ch); // Overload for non-const


// For compatibility with the existing codebase that used fs_strncpy etc.
// These can be aliases or direct calls to the k versions.
inline void fs_strncpy(char *dest, const char *src, unsigned int n) {
    kstrncpy(dest, src, static_cast<kstd::size_t>(n));
}

inline void fs_memset(void *s, int c, unsigned int n) {
    kmemset(s, c, static_cast<kstd::size_t>(n));
}

// The original fs_strncmp was in fs.c, and global.
// We'll use kstrncmp and assume the old fs_strncmp can be replaced.
// If direct replacement is not possible, an alias can be provided.
// int fs_strncmp(const char *s1, const char *s2, unsigned int n);

// The original strcmp and strlen were also global in kernel.c
// We should replace their usage with kstrcmp and kstrlen.

} // namespace kstd

#endif // KSTD_CSTRING_H
