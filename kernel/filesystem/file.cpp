#include "file.h"
#include "filesystem.h" // Required for Kernel::Filesystem definition
#include <kstd/cstring.h> // For kmemcpy, kmemset
#include <kstd/algorithm.h> // For kstd::min

namespace Kernel {
namespace FS {

// Private constructor, called by Filesystem::open_file
File::File(Kernel::Filesystem& fs_instance, FileMetadata* metadata_ptr, OpenMode open_mode)
    : filesystem(fs_instance),
      meta(metadata_ptr),
      current_mode(open_mode),
      current_seek_pos(0),
      is_valid(true) {
    if (!meta || !meta->in_use) {
        is_valid = false; // Should not happen if Filesystem::open_file is correct
    }
    // If opened in write mode that truncates, handle truncation here (or in Filesystem::open_file)
    // For now, assume truncation is handled by Filesystem or write operations.
}

File::~File() {
    // In this simple RAM FS, not much to do here.
    // If File objects were dynamically allocated by Filesystem, Filesystem might track them.
    // For now, File objects are expected to be managed by the caller of Filesystem::open_file.
    is_valid = false;
}

ErrorCode File::read(void* buffer, kstd::size_t count, kstd::size_t& bytes_read) {
    bytes_read = 0;
    if (!is_valid || !meta) return ErrorCode::INVALID_OPERATION;
    if (!has_read_access(current_mode)) return ErrorCode::INVALID_OPERATION; // No read permission
    if (!buffer && count > 0) return ErrorCode::IO_ERROR; // Null buffer with non-zero count

    if (current_seek_pos >= meta->size_bytes || count == 0) {
        return ErrorCode::OK; // EOF or nothing to read
    }

    kstd::size_t remaining_in_file = meta->size_bytes - current_seek_pos;
    kstd::size_t to_read = kstd::min(count, remaining_in_file);

    if (to_read == 0) return ErrorCode::OK;

    // Helper variables for multi-block reads
    kstd::uint32_t current_block_idx_in_file = current_seek_pos / BLOCK_SIZE_BYTES;
    kstd::size_t offset_within_first_block = current_seek_pos % BLOCK_SIZE_BYTES;
    kstd::size_t total_bytes_transferred = 0;
    unsigned char* out_buffer = static_cast<unsigned char*>(buffer);

    while (total_bytes_transferred < to_read) {
        if (current_block_idx_in_file >= meta->num_blocks) {
            // Should not happen if size_bytes and num_blocks are consistent
            // and to_read was calculated correctly.
            break;
        }

        kstd::uint32_t actual_disk_block = meta->start_block + current_block_idx_in_file;
        kstd::size_t bytes_to_read_this_block = BLOCK_SIZE_BYTES - offset_within_first_block;
        bytes_to_read_this_block = kstd::min(bytes_to_read_this_block, to_read - total_bytes_transferred);

        kstd::size_t single_block_bytes_read = 0;
        ErrorCode res = filesystem.read_from_block(
            actual_disk_block,
            offset_within_first_block,
            out_buffer + total_bytes_transferred,
            bytes_to_read_this_block,
            single_block_bytes_read
        );

        if (res != ErrorCode::OK || single_block_bytes_read == 0) {
            // Error reading block or no bytes read when expected
            if (total_bytes_transferred > 0) break; // Return what we got so far
            return res != ErrorCode::OK ? res : ErrorCode::IO_ERROR;
        }

        total_bytes_transferred += single_block_bytes_read;
        offset_within_first_block = 0; // Subsequent reads are from start of block
        current_block_idx_in_file++;
    }

    bytes_read = total_bytes_transferred;
    current_seek_pos += bytes_read;
    return ErrorCode::OK;
}


ErrorCode File::write(const void* buffer, kstd::size_t count, kstd::size_t& bytes_written) {
    bytes_written = 0;
    if (!is_valid || !meta) return ErrorCode::INVALID_OPERATION;
    if (!has_write_access(current_mode)) return ErrorCode::INVALID_OPERATION;
    if (!buffer && count > 0) return ErrorCode::IO_ERROR;

    if (count == 0) return ErrorCode::OK;

    // Check if write exceeds MAX_FILE_SIZE_BYTES
    if (current_seek_pos + count > MAX_FILE_SIZE_BYTES) {
        // Option 1: Error out
        // return ErrorCode::FILE_TOO_LARGE;
        // Option 2: Truncate write to fit
        kstd::size_t available_space = MAX_FILE_SIZE_BYTES - current_seek_pos;
        if (available_space == 0) return ErrorCode::FILE_TOO_LARGE; // No space at all at current seek
        count = kstd::min(count, available_space);
        if (count == 0) return ErrorCode::FILE_TOO_LARGE; // Cannot write even 1 byte
    }

    // Check if we need to allocate more blocks
    kstd::size_t new_required_size = current_seek_pos + count;
    kstd::size_t required_blocks = (new_required_size + BLOCK_SIZE_BYTES - 1) / BLOCK_SIZE_BYTES;
    if (required_blocks == 0 && new_required_size > 0) required_blocks = 1; // Min 1 block for non-empty file

    if (required_blocks > meta->num_blocks) {
        if (required_blocks > MAX_BLOCKS_PER_FILE) {
            return ErrorCode::FILE_TOO_LARGE; // Exceeds max blocks per file limit
        }
        // Need to reallocate. This simple FS uses contiguous blocks.
        // A real FS would be more complex (extend, or allocate new, copy, free old).
        // For now, if extending beyond current contiguous allocation, it's tricky.
        // Let's assume Filesystem handles reallocation if needed, or this write implies it.
        // The current Filesystem::allocate_contiguous_blocks is for new files or full rewrites.
        // This simple File::write will assume blocks are pre-allocated by Filesystem
        // up to MAX_BLOCKS_PER_FILE if opened for write, or that write is within current allocation.
        // A more robust write would ask Filesystem to ensure_capacity(new_required_size).

        // For this version, let's assume if we write past allocated blocks but within MAX_BLOCKS_PER_FILE,
        // the Filesystem should have handled it during open or a prior write that set the size.
        // This simplified write will just write into the currently allocated blocks.
        // If current_seek_pos + count > meta->num_blocks * BLOCK_SIZE_BYTES, it's an issue.
        if (required_blocks > meta->num_blocks) {
            // This means we need more blocks than currently allocated.
            // This simple File class cannot reallocate itself. The Filesystem should manage this.
            // For now, this is a limitation. A write that extends the file might fail
            // if it requires more blocks than initially allocated by a `create` or previous `write`
            // that set the file size and block count.
            // A robust solution:
            // FS::ErrorCode e = filesystem.resize_file_blocks(meta, required_blocks); if (e != OK) return e;
            // For now, we'll cap the write to existing allocated blocks or MAX_FILE_SIZE_BYTES.
             if (current_seek_pos + count > meta->num_blocks * BLOCK_SIZE_BYTES && meta->num_blocks < MAX_BLOCKS_PER_FILE) {
                // This indicates an internal FS logic gap if we reach here with more to write than space.
                // Let's assume for now that the filesystem has pre-allocated up to MAX_BLOCKS_PER_FILE
                // if the file is writable and empty, or that write() is used to overwrite.
                // The provided C code's fs_write_file re-allocates entirely.
                // Our File class is lower level.
                // For now, let's just return an error if we try to write past allocated block space.
                 return ErrorCode::DISK_FULL; // Simplified: means "no more allocated blocks for this file"
             }
        }
    }


    // Proceed with writing data
    kstd::uint32_t current_block_idx_in_file = current_seek_pos / BLOCK_SIZE_BYTES;
    kstd::size_t offset_within_first_block = current_seek_pos % BLOCK_SIZE_BYTES;
    kstd::size_t total_bytes_transferred = 0;
    const unsigned char* in_buffer = static_cast<const unsigned char*>(buffer);

    while (total_bytes_transferred < count) {
        if (current_block_idx_in_file >= meta->num_blocks) {
            // Trying to write past allocated blocks.
            break;
        }

        kstd::uint32_t actual_disk_block = meta->start_block + current_block_idx_in_file;
        kstd::size_t bytes_to_write_this_block = BLOCK_SIZE_BYTES - offset_within_first_block;
        bytes_to_write_this_block = kstd::min(bytes_to_write_this_block, count - total_bytes_transferred);

        kstd::size_t single_block_bytes_written = 0;
        ErrorCode res = filesystem.write_to_block(
            actual_disk_block,
            offset_within_first_block,
            in_buffer + total_bytes_transferred,
            bytes_to_write_this_block,
            single_block_bytes_written
        );

        if (res != ErrorCode::OK || single_block_bytes_written == 0) {
            if (total_bytes_transferred > 0) break;
            return res != ErrorCode::OK ? res : ErrorCode::IO_ERROR;
        }

        total_bytes_transferred += single_block_bytes_written;
        offset_within_first_block = 0; // Subsequent writes are from start of block
        current_block_idx_in_file++;
    }

    bytes_written = total_bytes_transferred;
    current_seek_pos += bytes_written;

    // Update file size if we wrote past the old EOF
    if (current_seek_pos > meta->size_bytes) {
        meta->size_bytes = current_seek_pos;
    }

    return ErrorCode::OK;
}

ErrorCode File::seek(kstd::size_t offset) {
    if (!is_valid || !meta) return ErrorCode::INVALID_OPERATION;

    // Allow seeking up to file size (for writing at EOF) or MAX_FILE_SIZE_BYTES if writable.
    kstd::size_t max_seek = meta->size_bytes;
    if (has_write_access(current_mode)) {
        max_seek = MAX_FILE_SIZE_BYTES;
    }
     // Or, more restrictively, only seek within current size or allocated blocks.
     // max_seek = meta->num_blocks * BLOCK_SIZE_BYTES;
     // max_seek = kstd::min(max_seek, MAX_FILE_SIZE_BYTES);


    if (offset > max_seek) {
        // For read-only, seeking past EOF is usually an error or clamps to EOF.
        // For write, some systems allow seeking past EOF to create sparse files (not supported here).
        // Let's clamp to max_seek.
        current_seek_pos = max_seek;
    } else {
        current_seek_pos = offset;
    }
    return ErrorCode::OK;
}

kstd::size_t File::tell() const {
    if (!is_valid) return static_cast<kstd::size_t>(-1); // Error indicator
    return current_seek_pos;
}

kstd::size_t File::get_size() const {
    if (!is_valid || !meta) return 0; // Or error indicator
    return meta->size_bytes;
}

const char* File::get_name() const {
    if (!is_valid || !meta) return nullptr;
    return meta->name;
}

FileType File::get_type() const {
    if (!is_valid || !meta) return FileType::FILE; // Default, or an error type
    return meta->type;
}

bool File::eof() const {
    if (!is_valid || !meta) return true; // If file is invalid, consider it EOF
    return current_seek_pos >= meta->size_bytes;
}


} // namespace FS
} // namespace Kernel
