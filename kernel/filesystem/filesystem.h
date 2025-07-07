#ifndef KERNEL_FILESYSTEM_FILESYSTEM_H
#define KERNEL_FILESYSTEM_FILESYSTEM_H

#include "types.h"        // For ErrorCode, FileMetadata, MAX_FILES, etc.
#include "file.h"         // For File class (as return type, needs forward declaration if not included)
#include <kstd/cstddef.h> // For kstd::size_t
#include <kstd/cstdint.h> // For kstd::uintXX_t
// #include <kstd/vector.h> // If using a custom vector for file_table or free block list
// #include <kstd/unique_ptr.h> // If using unique_ptr for File objects

namespace Kernel {

// Forward declare File from its namespace
namespace FS { class File; }


class Filesystem {
public:
    Filesystem();
    ~Filesystem() = default;

    // Initialize the filesystem (e.g., clear RAM disk, setup metadata structures)
    void init();

    // Create a new file
    // filename: Name of the file to create.
    // type: Type of file (FileType::FILE or FileType::DIRECTORY).
    // Returns ErrorCode::OK on success.
    FS::ErrorCode create_file(const char* filename, FS::FileType type = FS::FileType::FILE);

    // Open an existing file
    // filename: Name of the file to open.
    // mode: OpenMode (READ, WRITE, READ_WRITE).
    // file_ptr (out): On success, will point to a new File object. Caller owns the memory.
    // Returns ErrorCode::OK on success.
    // Using a raw pointer return for simplicity, could use unique_ptr with custom deleter.
    FS::ErrorCode open_file(const char* filename, FS::OpenMode mode, FS::File*& out_file_obj);

    // Delete a file
    // filename: Name of the file to delete.
    // Returns ErrorCode::OK on success.
    FS::ErrorCode delete_file(const char* filename);

    // List files (simple version, prints to console via kprintf)
    // A better version might fill a buffer or return a list structure.
    void list_files_to_console() const;

    // Check if a file exists
    bool file_exists(const char* filename) const;

    // Get file metadata
    const FS::FileMetadata* get_file_metadata(const char* filename) const;


    // Low-level disk operations, called by File objects or internally
    // These are specific to the RAM disk implementation.

    // Read data from a block on the RAM disk
    // block_index: The block number to read from.
    // offset_in_block: Offset within the block to start reading.
    // buffer: Destination buffer.
    // count: Number of bytes to read.
    // Returns actual bytes read or negative error.
    FS::ErrorCode read_from_block(kstd::uint32_t block_index, kstd::size_t offset_in_block,
                                  void* buffer, kstd::size_t count, kstd::size_t& bytes_read);

    // Write data to a block on the RAM disk
    // block_index: The block number to write to.
    // offset_in_block: Offset within the block to start writing.
    // buffer: Source data buffer.
    // count: Number of bytes to write.
    // Returns actual bytes written or negative error.
    FS::ErrorCode write_to_block(kstd::uint32_t block_index, kstd::size_t offset_in_block,
                                 const void* buffer, kstd::size_t count, kstd::size_t& bytes_written);

    // Allocate contiguous blocks for a file
    // num_blocks_needed: Number of blocks to allocate.
    // start_block_index (out): On success, the starting block index.
    // Returns ErrorCode::OK on success.
    FS::ErrorCode allocate_contiguous_blocks(kstd::size_t num_blocks_needed, kstd::uint32_t& out_start_block_index);

    // Free contiguous blocks previously allocated to a file
    // start_block_index: Starting block index.
    // num_blocks: Number of blocks to free.
    void free_contiguous_blocks(kstd::uint32_t start_block_index, kstd::size_t num_blocks);


private:
    // RAM disk data area
    static unsigned char ram_disk_data[FS::RAM_DISK_SIZE_BYTES]; // Static to be in .bss or .data

    // File metadata table
    FS::FileMetadata file_table[FS::MAX_FILES];

    // Bitmap for free block management
    // Each bit represents a block. 0 = free, 1 = used.
    unsigned char block_bitmap[FS::BLOCK_BITMAP_SIZE_BYTES];

    bool initialized;

    // Helper to find FileMetadata by name
    FS::FileMetadata* find_metadata(const char* filename);
    const FS::FileMetadata* find_metadata(const char* filename) const;


    // Block bitmap manipulation helpers
    bool is_block_free(kstd::uint32_t block_index) const;
    void mark_block_status(kstd::uint32_t block_index, bool used); // true for used, false for free

    // Find a free slot in the file_table for new file metadata
    int find_free_metadata_slot() const;

    // Prevent copying/assignment
    Filesystem(const Filesystem&) = delete;
    Filesystem& operator=(const Filesystem&) = delete;
};


// Global accessor for the main filesystem instance
Filesystem& global_filesystem();

} // namespace Kernel

#endif // KERNEL_FILESYSTEM_FILESYSTEM_H
