#include "filesystem.h"
#include <kstd/cstring.h>   // For kmemset, kstrncpy, kstrcmp
#include <kstd/algorithm.h> // For kstd::min
#include <kernel/console.h> // For kprintf (used in list_files_to_console)
#include <lib/printf/printf.h> // For Kernel::kprintf specifically

namespace Kernel {

// Define static members
unsigned char Filesystem::ram_disk_data[FS::RAM_DISK_SIZE_BYTES];
// FileMetadata Filesystem::file_table[FS::MAX_FILES]; // Already a member
// unsigned char Filesystem::block_bitmap[FS::BLOCK_BITMAP_SIZE_BYTES]; // Already a member


// Global filesystem instance
static Filesystem g_filesystem_instance;

Filesystem& global_filesystem() {
    return g_filesystem_instance;
}


Filesystem::Filesystem() : initialized(false) {
    // Initialize arrays to zero. This is important as they are static.
    // kstd::kmemset might not be usable if kstd itself relies on FS or other things not yet up.
    // However, for static/global objects, they should be zero-initialized by C++ runtime (.bss).
    // Explicitly zeroing here is safer if that's not guaranteed in our freestanding env.
    // For now, assume .bss is cleared by startup code. If issues arise, explicitly zero here.
}

void Filesystem::init() {
    if (initialized) return;

    Kernel::kprintf("Initializing In-Memory Filesystem...\n");
    // Clear RAM disk data
    kstd::kmemset(ram_disk_data, 0, FS::RAM_DISK_SIZE_BYTES);

    // Clear file metadata table (FileMetadata constructor handles individual clearing)
    for (int i = 0; i < FS::MAX_FILES; ++i) {
        file_table[i] = FS::FileMetadata(); // Re-initialize
    }

    // Clear block bitmap (all blocks free)
    kstd::kmemset(block_bitmap, 0, FS::BLOCK_BITMAP_SIZE_BYTES);

    initialized = true;
    Kernel::kprintf("Filesystem initialized: %u KB RAM Disk, %u blocks of %u bytes.\n",
           FS::RAM_DISK_SIZE_BYTES / 1024, FS::MAX_BLOCKS, FS::BLOCK_SIZE_BYTES);
}

FS::FileMetadata* Filesystem::find_metadata(const char* filename) {
    if (!filename || filename[0] == '\0') return nullptr;
    for (int i = 0; i < FS::MAX_FILES; ++i) {
        if (file_table[i].in_use && kstd::kstrcmp(file_table[i].name, filename) == 0) {
            return &file_table[i];
        }
    }
    return nullptr;
}
const FS::FileMetadata* Filesystem::find_metadata(const char* filename) const {
    // Call the non-const version and cast. Safe if used appropriately.
    return const_cast<Filesystem*>(this)->find_metadata(filename);
}


int Filesystem::find_free_metadata_slot() const {
    for (int i = 0; i < FS::MAX_FILES; ++i) {
        if (!file_table[i].in_use) {
            return i;
        }
    }
    return -1; // No free slots
}

bool Filesystem::is_block_free(kstd::uint32_t block_index) const {
    if (block_index >= FS::MAX_BLOCKS) return false; // Invalid block
    kstd::uint32_t byte_idx = block_index / 8;
    kstd::uint8_t bit_idx  = block_index % 8;
    return !(block_bitmap[byte_idx] & (1 << bit_idx));
}

void Filesystem::mark_block_status(kstd::uint32_t block_index, bool used) {
    if (block_index >= FS::MAX_BLOCKS) return;
    kstd::uint32_t byte_idx = block_index / 8;
    kstd::uint8_t bit_idx  = block_index % 8;
    if (used) {
        block_bitmap[byte_idx] |= (1 << bit_idx);
    } else {
        block_bitmap[byte_idx] &= ~(1 << bit_idx);
    }
}


FS::ErrorCode Filesystem::create_file(const char* filename, FS::FileType type) {
    if (!initialized) init(); // Ensure initialized
    if (!filename || filename[0] == '\0' || kstd::kstrlen(filename) >= FS::MAX_FILENAME_LENGTH) {
        return FS::ErrorCode::INVALID_NAME;
    }
    if (find_metadata(filename) != nullptr) {
        return FS::ErrorCode::ALREADY_EXISTS;
    }

    int slot_idx = find_free_metadata_slot();
    if (slot_idx == -1) {
        return FS::ErrorCode::FILESYSTEM_FULL; // No more file metadata slots
    }

    FS::FileMetadata& new_meta = file_table[slot_idx];
    new_meta = FS::FileMetadata(); // Reset to default values

    kstd::kstrncpy(new_meta.name, filename, FS::MAX_FILENAME_LENGTH -1);
    new_meta.name[FS::MAX_FILENAME_LENGTH -1] = '\0'; // Ensure null termination
    new_meta.type = type;
    new_meta.in_use = true;
    new_meta.size_bytes = 0;
    new_meta.start_block = 0; // No blocks allocated yet (or an invalid marker like ~0U)
    new_meta.num_blocks = 0;

    // For this simple FS, creating a file doesn't pre-allocate blocks.
    // Blocks are allocated on first write that needs them.
    // Or, open_file in write mode could allocate initial blocks.
    // The old fs_create_file also didn't allocate blocks.

    Kernel::kprintf("FS: Created file '%s'\n", filename);
    return FS::ErrorCode::OK;
}


FS::ErrorCode Filesystem::open_file(const char* filename, FS::OpenMode mode, FS::File*& out_file_obj) {
    out_file_obj = nullptr;
    if (!initialized) init();
    if (!filename) return FS::ErrorCode::INVALID_NAME;

    FS::FileMetadata* meta = find_metadata(filename);

    if (!meta) {
        if (FS::has_write_access(mode)) { // Create if doesn't exist and mode allows writing
            FS::ErrorCode create_res = create_file(filename);
            if (create_res != FS::ErrorCode::OK) {
                return create_res;
            }
            meta = find_metadata(filename); // Get the newly created metadata
            if (!meta) return FS::ErrorCode::UNKNOWN; // Should not happen
        } else {
            return FS::ErrorCode::NOT_FOUND;
        }
    }

    // At this point, meta should be valid.
    // If opening for write and file exists, handle truncation or initial block allocation.
    // The current C code's fs_write_file (which is like a combined open-truncate-write-close)
    // always frees old blocks and allocates new ones.
    // For our open_file, if mode is WRITE and file exists:
    // We could truncate it (set size to 0, free blocks).
    // For simplicity now, let's say write operations on the File object will handle this.
    // If it's a new file (just created), size is 0, num_blocks is 0.
    // If it's an existing file, opening for pure WRITE might imply truncation.
    // Let's make Filesystem::open with WRITE mode truncate the file.
    if (FS::has_write_access(mode) && meta->size_bytes > 0) {
        // Truncate: free blocks and reset size.
        if (meta->num_blocks > 0) {
            free_contiguous_blocks(meta->start_block, meta->num_blocks);
        }
        meta->size_bytes = 0;
        meta->num_blocks = 0;
        meta->start_block = 0; // Or invalid marker
        Kernel::kprintf("FS: File '%s' truncated due to write mode.\n", filename);
    }

    // Create and return File object.
    // The caller of Filesystem::open_file is responsible for deleting this File object.
    // Consider using a unique_ptr if custom allocators/deleters are set up.
    // For now, raw new.
    out_file_obj = new FS::File(*this, meta, mode);
    if (!out_file_obj) {
        return FS::ErrorCode::UNKNOWN; // Allocation failed
    }

    Kernel::kprintf("FS: Opened file '%s'\n", filename);
    return FS::ErrorCode::OK;
}


FS::ErrorCode Filesystem::delete_file(const char* filename) {
    if (!initialized) return FS::ErrorCode::INVALID_OPERATION;
    FS::FileMetadata* meta = find_metadata(filename);
    if (!meta) {
        return FS::ErrorCode::NOT_FOUND;
    }

    // Free allocated blocks
    if (meta->num_blocks > 0) {
        free_contiguous_blocks(meta->start_block, meta->num_blocks);
    }

    // Mark metadata slot as free
    meta->in_use = false;
    meta->name[0] = '\0'; // Clear name
    // Other fields reset by FileMetadata constructor if slot is reused.

    Kernel::kprintf("FS: Deleted file '%s'\n", filename);
    return FS::ErrorCode::OK;
}

void Filesystem::list_files_to_console() const {
    if (!initialized) {
        Kernel::kprintf("Filesystem not initialized.\n");
        return;
    }
    Kernel::kprintf("--- Filesystem Contents ---\n");
    Kernel::kprintf("Name                             Size (Bytes) Blocks StartBlk\n");
    Kernel::kprintf("-------------------------------- ------------ ------ --------\n");
    bool found_any = false;
    for (int i = 0; i < FS::MAX_FILES; ++i) {
        if (file_table[i].in_use) {
            found_any = true;
            // Kernel::kprintf is used here.
            Kernel::kprintf("%-32s %12u %6u %8u\n",
                   file_table[i].name,
                   static_cast<unsigned int>(file_table[i].size_bytes), // kprintf might not handle size_t directly
                   file_table[i].num_blocks,
                   file_table[i].start_block);
        }
    }
    if (!found_any) {
        Kernel::kprintf("(empty)\n");
    }
    Kernel::kprintf("------------------------------------------------------------\n");
}

bool Filesystem::file_exists(const char* filename) const {
    if (!initialized) return false;
    return find_metadata(filename) != nullptr;
}

const FS::FileMetadata* Filesystem::get_file_metadata(const char* filename) const {
    if (!initialized) return nullptr;
    return find_metadata(filename);
}


FS::ErrorCode Filesystem::read_from_block(kstd::uint32_t block_index, kstd::size_t offset_in_block,
                                       void* buffer, kstd::size_t count, kstd::size_t& bytes_read) {
    bytes_read = 0;
    if (block_index >= FS::MAX_BLOCKS || offset_in_block >= FS::BLOCK_SIZE_BYTES) {
        return FS::ErrorCode::IO_ERROR; // Invalid args
    }

    kstd::size_t to_read = kstd::min(count, FS::BLOCK_SIZE_BYTES - offset_in_block);
    if (to_read == 0) return FS::ErrorCode::OK;

    kstd::uintptr_t src_addr = reinterpret_cast<kstd::uintptr_t>(ram_disk_data) +
                               (block_index * FS::BLOCK_SIZE_BYTES) +
                               offset_in_block;

    kstd::kmemcpy(buffer, reinterpret_cast<const void*>(src_addr), to_read);
    bytes_read = to_read;
    return FS::ErrorCode::OK;
}

FS::ErrorCode Filesystem::write_to_block(kstd::uint32_t block_index, kstd::size_t offset_in_block,
                                      const void* buffer, kstd::size_t count, kstd::size_t& bytes_written) {
    bytes_written = 0;
    if (block_index >= FS::MAX_BLOCKS || offset_in_block >= FS::BLOCK_SIZE_BYTES) {
        return FS::ErrorCode::IO_ERROR; // Invalid args
    }

    kstd::size_t to_write = kstd::min(count, FS::BLOCK_SIZE_BYTES - offset_in_block);
    if (to_write == 0) return FS::ErrorCode::OK;

    kstd::uintptr_t dest_addr = reinterpret_cast<kstd::uintptr_t>(ram_disk_data) +
                                (block_index * FS::BLOCK_SIZE_BYTES) +
                                offset_in_block;

    kstd::kmemcpy(reinterpret_cast<void*>(dest_addr), buffer, to_write);
    bytes_written = to_write;
    return FS::ErrorCode::OK;
}


FS::ErrorCode Filesystem::allocate_contiguous_blocks(kstd::size_t num_blocks_needed, kstd::uint32_t& out_start_block_index) {
    out_start_block_index = ~0U; // Invalid marker
    if (num_blocks_needed == 0) return FS::ErrorCode::OK; // Or error, depending on desired behavior
    if (num_blocks_needed > FS::MAX_BLOCKS_PER_FILE) return FS::ErrorCode::FILE_TOO_LARGE; // Exceeds design limit

    if (FS::MAX_BLOCKS < num_blocks_needed) return FS::ErrorCode::DISK_FULL; // Not enough total blocks possible

    for (kstd::uint32_t i = 0; i <= FS::MAX_BLOCKS - num_blocks_needed; ++i) {
        bool found_space = true;
        for (kstd::size_t j = 0; j < num_blocks_needed; ++j) {
            if (!is_block_free(i + j)) {
                found_space = false;
                i += j; // Skip already checked non-free blocks
                break;
            }
        }
        if (found_space) {
            for (kstd::size_t j = 0; j < num_blocks_needed; ++j) {
                mark_block_status(i + j, true /* used */);
            }
            out_start_block_index = i;
            return FS::ErrorCode::OK;
        }
    }
    return FS::ErrorCode::DISK_FULL; // No contiguous space found
}

void Filesystem::free_contiguous_blocks(kstd::uint32_t start_block_index, kstd::size_t num_blocks) {
    if (start_block_index == static_cast<kstd::uint32_t>(~0U)) return; // Invalid start block
    for (kstd::size_t i = 0; i < num_blocks; ++i) {
        if ((start_block_index + i) < FS::MAX_BLOCKS) {
            mark_block_status(start_block_index + i, false /* free */);
        }
    }
}


// Filesystem instance and global accessor are defined at the top of this file.
// Ensure kernel_main.cpp calls global_filesystem().init();

} // namespace Kernel
