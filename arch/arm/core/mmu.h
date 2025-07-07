#ifndef ARCH_ARM_CORE_MMU_H
#define ARCH_ARM_CORE_MMU_H

#include <kstd/cstdint.h>
#include <kstd/cstddef.h> // For kstd::size_t

namespace Arch {
namespace Arm {

// AArch64 Page Table Entry (Descriptor) attributes
// Reference: ARM Architecture Reference Manual ARMv8, for ARMv8-A architecture profile, section D5.3 Memory attribute fields in the VMSAv8-64 translation table format descriptors.

// --- Level 1, 2, or 3 Table/Page Descriptor (64-bit) ---
// Common bits for Block and Table descriptors (lower bits)
constexpr kstd::uint64_t PTE_VALID        = (1ULL << 0); // Valid descriptor
constexpr kstd::uint64_t PTE_TABLE_OR_PAGE= (1ULL << 1); // 0 for Block/Page, 1 for Table descriptor (points to next level)
                                                       // For L1/L2 block entries, this is 0 if it's a block.
                                                       // For L1 pointing to L2 table, this is 1.
                                                       // For L2 pointing to L3 table, this is 1.
                                                       // For L3 entries, this is 1 (Page descriptor).

// Memory Attributes (MAIR_ELx index in bits 4:2)
// These are indices into the MAIR_EL1 register.
constexpr kstd::uint64_t PTE_ATTR_INDX_MASK = (0b111ULL << 2);
constexpr kstd::uint64_t PTE_ATTR_INDX(kstd::uint64_t idx) { return (idx & 0b111) << 2; }

// MAIR_EL1 attribute encoding for AttrIndx[2:0]
// We will define these attributes in MAIR_EL1. Examples:
// Attr0: Device-nGnRnE memory (for peripherals)
// Attr1: Normal memory, Outer Write-Back Non-cacheable, Inner Write-Back Non-cacheable
// Attr2: Normal memory, Outer Write-Back Write-Allocate Read-Allocate Cacheable, Inner Write-Back Write-Allocate Read-Allocate Cacheable
// (For simplicity, non-cacheable normal memory might be easier initially)

// Access Permissions (AP bits 7:6)
// AP[2:1] -> APTable bits in TCR_EL1 determine meaning.
// Assuming APTable = 00 (default meaning):
// AP[1] (bit 6): Access permission. 0 = EL1 R/W, EL0 None. 1 = EL1 R/W, EL0 R/W. (Simplified)
// AP[2] (bit 7): Read-only. 0 = R/W. 1 = Read-only.
constexpr kstd::uint64_t PTE_AP_EL1_RW_EL0_NONE = (0b00ULL << 6); // EL1 R/W, EL0 No access
constexpr kstd::uint64_t PTE_AP_EL1_RW_EL0_RW   = (0b01ULL << 6); // EL1 R/W, EL0 R/W
constexpr kstd::uint64_t PTE_AP_EL1_RO_EL0_NONE = (0b10ULL << 6); // EL1 RO,  EL0 No access
constexpr kstd::uint64_t PTE_AP_EL1_RO_EL0_RO   = (0b11ULL << 6); // EL1 RO,  EL0 RO

// Shareability field (SH bits 9:8)
constexpr kstd::uint64_t PTE_SH_NON_SHAREABLE   = (0b00ULL << 8);
constexpr kstd::uint64_t PTE_SH_OUTER_SHAREABLE = (0b10ULL << 8); // Typically for Normal_WB memory
constexpr kstd::uint64_t PTE_SH_INNER_SHAREABLE = (0b11ULL << 8); // Typically for Device memory

// Access Flag (AF bit 10) - Set by hardware on first access
constexpr kstd::uint64_t PTE_AF = (1ULL << 10); // Must be set to 1 by software initially for block/page entries

// --- Specific to Block/Page entries ---
// (PTE_TABLE_OR_PAGE is 0 for L1/L2 block, or 1 for L3 page)
// For L1 or L2 Block descriptors:
// Bits 47:N are the output address, where N depends on block size.
// For 2MB block (Level 2): Output address bits 47:21. (N=21)
// For 1GB block (Level 1): Output address bits 47:30. (N=30) - Not used if L1 points to L2 table.

// Upper attributes for Page/Block entries
constexpr kstd::uint64_t PTE_PXN = (1ULL << 53); // Privileged Execute Never (EL1 execute never)
constexpr kstd::uint64_t PTE_UXN = (1ULL << 54); // Unprivileged Execute Never (EL0 execute never)

// --- Specific to Table descriptors ---
// (PTE_TABLE_OR_PAGE is 1)
// Bits 47:12 point to the next-level table base address (must be 4KB aligned).
// Ignored bits for Table descriptors (e.g. XN flags)


// Memory region types for MAIR_EL1 configuration
// We'll define 3 types: Device, Normal_NC (Non-Cacheable), Normal_C (Cacheable)
// MAIR_EL1 holds 8 attributes (Attr0 to Attr7), each 8 bits.
// Attr = (Outer Cacheability << 4) | (Inner Cacheability)
// Outer/Inner encodings:
//  0b0000: Device-nGnRnE (strongly ordered, non-gathering, non-reordering, early WriteAck)
//  0b0001: Device-nGnRE (non-gathering, non-reordering, early WriteAck)
//  0b0010: Device-nGRE (non-gathering, reordering, early WriteAck)
//  0b0011: Device-GRE (gathering, reordering, early WriteAck)
//  0b0100: Normal, Inner/Outer Non-Cacheable
//  0b0101 - 0b0111: Normal, Inner/Outer Write-Through Cacheable
//  0b1000 - 0b1111: Normal, Inner/Outer Write-Back Cacheable

// MAIR_EL1 attribute indices we will use:
constexpr unsigned int MAIR_IDX_DEVICE_NGNRNE = 0;
constexpr unsigned int MAIR_IDX_NORMAL_NC     = 1; // Non-cacheable
constexpr unsigned int MAIR_IDX_NORMAL_C      = 2; // Cacheable (Write-Back)

// MAIR_EL1 attribute values:
// Attr0: Device-nGnRnE (e.g., for peripherals) -> 0b00000000 = 0x00
constexpr kstd::uint8_t MAIR_ATTR_DEVICE_NGNRNE = 0x00;
// Attr1: Normal, Outer Non-Cacheable, Inner Non-Cacheable -> 0b01000100 = 0x44
constexpr kstd::uint8_t MAIR_ATTR_NORMAL_NC     = 0x44;
// Attr2: Normal, Outer Write-Back Read-Alloc Write-Alloc, Inner Write-Back Read-Alloc Write-Alloc -> 0b11111111 = 0xFF
constexpr kstd::uint8_t MAIR_ATTR_NORMAL_C      = 0xFF;


// Page table configuration constants
constexpr kstd::size_t PAGE_TABLE_ENTRIES = 512; // 512 entries per table (4KB table / 8 bytes per entry)
constexpr kstd::size_t PAGE_SIZE_4KB    = 4096;
constexpr kstd::size_t PAGE_SIZE_2MB    = 2 * 1024 * 1024;
constexpr kstd::size_t PAGE_SIZE_1GB    = 1 * 1024 * 1024 * 1024;

// TCR_EL1 (Translation Control Register) bits
// TG0: Granule size for TTBR0_EL1. 00=4KB, 01=16KB, 10=64KB. We use 4KB.
// SH0: Shareability for TTBR0_EL1 page table walks. 00=Non-shareable, 10=Outer, 11=Inner.
// ORGN0: Outer cacheability for TTBR0_EL1 page table walks. 00=NC, 01=WB WA, 10=WT, 11=WB non-WA.
// IRGN0: Inner cacheability for TTBR0_EL1 page table walks. (Same encoding as ORGN0)
// T0SZ: Size offset for TTBR0_EL1. (64 - T0SZ) is the number of address bits covered.
//       For 48-bit VA space, T0SZ = 64 - 48 = 16.
// IPS: Intermediate Physical Address Size. 000=32bit, 001=36bit, 010=40bit, 011=42bit, 100=44bit, 101=48bit.
//      RPi4 has 40-bit physical address space for RAM if using LPAE (BCM2711 has up to 8GB).
//      Let's assume 40-bit PA (IPS=010) for now.
//      Or for simplicity, 32-bit PA (IPS=000) if only mapping first 4GB.
//      Max physical address on RPi4 can be higher for peripherals.
//      Let's target 48-bit VA and 40-bit PA (IPS=010).
//      T0SZ = 16 (for 48-bit VA starting at 0)
//      TTBR0_EL1 covers the lower VA range (typically user space or low kernel space).
//      TTBR1_EL1 covers the upper VA range (typically kernel space).
//      For a simple kernel identity mapping low addresses, we use TTBR0_EL1.

// We'll use a 3-level page table structure for 2MB blocks with 4KB granule:
// L1 table -> L2 table -> 2MB Block entry
// (L0 is not used if T0SZ means 48-bit input address with 4KB granule)
// If L1 entry points to L2 table, L1 has 512 entries (1GB each)
// L2 table has 512 entries (2MB each) = 1GB per L2 table.
// So, for 2GB identity map, we need:
// One L1 table. Two L1 entries used.
// Two L2 tables. Each L2 table fully populated (512 entries).

class MMU {
public:
    MMU() = default;

    // Initialize and enable the MMU.
    // Creates identity mapping for the first 2GB of RAM.
    // Kernel image should be within this 2GB range.
    static void init_and_enable();

private:
    // Aligned page table storage. Needs to be in memory accessible before MMU is on.
    // These will be static members, allocated in .bss or .data.
    // Ensure they are page-aligned (4KB).
    static kstd::uint64_t l1_page_table[PAGE_TABLE_ENTRIES] __attribute__((aligned(PAGE_SIZE_4KB)));
    static kstd::uint64_t l2_page_table_0[PAGE_TABLE_ENTRIES] __attribute__((aligned(PAGE_SIZE_4KB))); // For VA 0x00000000 - 0x3FFFFFFF (1st GB)
    static kstd::uint64_t l2_page_table_1[PAGE_TABLE_ENTRIES] __attribute__((aligned(PAGE_SIZE_4KB))); // For VA 0x40000000 - 0x7FFFFFFF (2nd GB)
    // Add more L2 tables if mapping more than 2GB via L1.

    static void setup_page_tables();
    static void configure_translation_control();
    static void enable_mmu_and_caches();

    // Helper to get physical address of static page table arrays
    template<typename T, kstd::size_t N>
    static kstd::uintptr_t get_physical_address(T (&array)[N]) {
        // For bare metal, virtual address == physical address before MMU is on.
        // After MMU, if identity mapped, it's still the same.
        // This needs to be the actual physical address where the linker places these arrays.
        return reinterpret_cast<kstd::uintptr_t>(array);
    }
};

} // namespace Arm
} // namespace Arch

#endif // ARCH_ARM_CORE_MMU_H
