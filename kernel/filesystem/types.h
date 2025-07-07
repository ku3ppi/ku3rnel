#ifndef KERNEL_FILESYSTEM_TYPES_H
#define KERNEL_FILESYSTEM_TYPES_H

#include <kstd/cstddef.h> // For kstd::size_t
#include <kstd/cstdint.h> // For kstd::uintXX_t

namespace Kernel {
namespace FS {

// Configuration for our simple RAM-disk filesystem
constexpr kstd::size_t RAM_DISK_SIZE_BYTES = 1024 * 256; // 256 KB for the RAM disk
constexpr kstd::size_t BLOCK_SIZE_BYTES    = 512;        // Each block is 512 bytes
constexpr kstd::size_t MAX_BLOCKS          = RAM_DISK_SIZE_BYTES / BLOCK_SIZE_BYTES; // Total blocks (512)

// Bitmap for block allocation: 1 bit per block.
constexpr kstd::size_t BLOCK_BITMAP_SIZE_BYTES = (MAX_BLOCKS + 7) / 8; // Rounded up to nearest byte (512/8 = 64 bytes)

constexpr kstd::size_t MAX_FILENAME_LENGTH = 32; // Including null terminator
constexpr kstd::size_t MAX_FILES           = 64; // Maximum number of files we can track

// Max data a single file can hold. Using contiguous blocks for simplicity.
// This could be made more flexible with linked blocks later.
constexpr kstd::size_t MAX_BLOCKS_PER_FILE = 8; // A file can be up to 8 * 512 = 4KB
constexpr kstd::size_t MAX_FILE_SIZE_BYTES = MAX_BLOCKS_PER_FILE * BLOCK_SIZE_BYTES;

enum class FileType : kstd::uint8_t {
    FILE = 0,
    DIRECTORY = 1 // Not fully supported in this simple version, but placeholder
};

enum class ErrorCode {
    OK = 0,
    NOT_FOUND = -1,
    ALREADY_EXISTS = -2,
    FILESYSTEM_FULL = -3, // No more file slots
    DISK_FULL = -4,       // No more data blocks
    INVALID_NAME = -5,
    INVALID_OPERATION = -6,
    BUFFER_TOO_SMALL = -7,
    FILE_TOO_LARGE = -8,
    IO_ERROR = -9, // Generic I/O error
    UNKNOWN = -10
};

// Structure to hold metadata for each file/directory
struct FileMetadata {
    char name[MAX_FILENAME_LENGTH];
    FileType type;
    bool in_use;                 // True if this metadata slot is used

    kstd::uint32_t start_block;    // Index of the first data block in RAM disk
    kstd::uint32_t num_blocks;     // Number of blocks allocated
    kstd::size_t size_bytes;     // Actual size of the file content in bytes

    // Timestamps, permissions, etc. could be added here for a more complex FS.

    FileMetadata() : type(FileType::FILE), in_use(false), start_block(0), num_blocks(0), size_bytes(0) {
        name[0] = '\0';
    }
};


// Modes for opening a file (simplified)
enum class OpenMode {
    READ = 1,
    WRITE = 2, // Opens for writing, creates if not exists, truncates if exists.
    READ_WRITE = READ | WRITE
    // APPEND could be added
};

// Helper to check if a mode includes read access
inline bool has_read_access(OpenMode mode) {
    return static_cast<int>(mode) & static_cast<int>(OpenMode::READ);
}

// Helper to check if a mode includes write access
inline bool has_write_access(OpenMode mode) {
    return static_cast<int>(mode) & static_cast<int>(OpenMode::WRITE);
}


} // namespace FS
} // namespace Kernel

#endif // KERNEL_FILESYSTEM_TYPES_H
