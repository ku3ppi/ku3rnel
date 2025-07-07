#ifndef KERNEL_FILESYSTEM_FILE_H
#define KERNEL_FILESYSTEM_FILE_H

#include "types.h"        // For ErrorCode, OpenMode, FileMetadata, MAX_FILE_SIZE_BYTES
#include <kstd/cstddef.h> // For kstd::size_t
#include <kstd/cstdint.h> // For kstd::uintXX_t

namespace Kernel {

// Forward declare Filesystem class as File needs a reference/pointer to it
// to perform actual read/write operations on the disk.
class Filesystem;

namespace FS {

class File {
public:
    // Constructor is private or protected; Files are created by Filesystem::open_file.
    // File(Filesystem& fs, FileMetadata* meta, OpenMode mode);

    // Destructor - ensures resources are released if any (not much for RAM FS file object itself)
    ~File();

    // Read data from the file
    // buffer: Pointer to the buffer where data will be stored.
    // count: Number of bytes to read.
    // bytes_read (out): Actual number of bytes read.
    // Returns ErrorCode::OK on success.
    ErrorCode read(void* buffer, kstd::size_t count, kstd::size_t& bytes_read);

    // Write data to the file
    // buffer: Pointer to the data to write.
    // count: Number of bytes to write.
    // bytes_written (out): Actual number of bytes written.
    // Returns ErrorCode::OK on success.
    ErrorCode write(const void* buffer, kstd::size_t count, kstd::size_t& bytes_written);

    // Seek to a position in the file
    // offset: Offset to seek to.
    // whence: Starting point for offset (e.g., SEEK_SET, SEEK_CUR, SEEK_END - similar to stdio)
    // For simplicity, let's only support SEEK_SET for now.
    // enum SeekWhence { SET = 0, CUR = 1, END = 2 };
    // ErrorCode seek(long offset, SeekWhence whence);
    ErrorCode seek(kstd::size_t offset); // Seek from beginning of file

    // Get current seek position
    kstd::size_t tell() const;

    // Get file size
    kstd::size_t get_size() const;

    // Get filename
    const char* get_name() const;

    // Get file type
    FileType get_type() const;

    // Check if End-Of-File has been reached
    bool eof() const;

    // Close the file (optional, as destructor might handle cleanup)
    // For this simple FS, closing might not do much beyond invalidating the File object.
    // ErrorCode close();

private:
    // Friend class Filesystem so it can construct File objects and access internals.
    friend class Kernel::Filesystem;

    File(Kernel::Filesystem& fs_instance, FileMetadata* metadata_ptr, OpenMode open_mode);

    Kernel::Filesystem& filesystem; // Reference to the parent filesystem
    FileMetadata* meta;             // Pointer to this file's metadata in the FS table
    OpenMode current_mode;
    kstd::size_t current_seek_pos;
    bool is_valid;                  // Is this File object currently valid (representing an open file)?

    // Prevent copying/assignment
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    File(File&&) = delete;
    File& operator=(File&&) = delete;
};

} // namespace FS
} // namespace Kernel

#endif // KERNEL_FILESYSTEM_FILE_H
