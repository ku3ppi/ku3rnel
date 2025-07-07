#include "mmu.h"
#include <kernel/console.h> // For kprintf
#include <kstd/cstring.h>   // For kmemset
#include <arch/arm/peripherals/gpio.h> // For GPIO_BASE (example peripheral region)
#include <arch/arm/core/gic.h> // For GICD_BASE, GICC_BASE (example peripheral region)

// Define physical memory map constants for RPi4
// These are approximate and for example purposes.
// A more robust solution would get memory map from Device Tree Blob (DTB) if available.
// RPi4 has RAM starting at 0x0.
// Peripherals are typically in the 0xFE000000 - 0xFEFFFFFF range (BCM2711 ARM physical addresses)
// Or more broadly, 0xFC000000 to 0xFFFFFFFF might be considered peripheral space.
// For simplicity, we'll define a peripheral window.
constexpr kstd::uintptr_t RPI4_PERIPHERAL_BASE_PHYS = 0xFE000000;
constexpr kstd::uintptr_t RPI4_PERIPHERAL_END_PHYS  = 0xFFFFFFFF; // End of 32-bit mapped area for common peripherals

// Kernel region (example, assumes kernel is loaded low, e.g., at 0x80000)
// Linker script defines KERNEL_START and KERNEL_END.
extern "C" char KERNEL_START[];
extern "C" char KERNEL_END[];


namespace Arch {
namespace Arm {

// Static page table allocations
kstd::uint64_t MMU::l1_page_table[PAGE_TABLE_ENTRIES] __attribute__((aligned(PAGE_SIZE_4KB)));
kstd::uint64_t MMU::l2_page_table_0[PAGE_TABLE_ENTRIES] __attribute__((aligned(PAGE_SIZE_4KB)));
kstd::uint64_t MMU::l2_page_table_1[PAGE_TABLE_ENTRIES] __attribute__((aligned(PAGE_SIZE_4KB)));


void MMU::setup_page_tables() {
    Kernel::kprintf("MMU: Setting up page tables...\n");

    // Clear page tables initially
    kstd::kmemset(l1_page_table, 0, sizeof(l1_page_table));
    kstd::kmemset(l2_page_table_0, 0, sizeof(l2_page_table_0));
    kstd::kmemset(l2_page_table_1, 0, sizeof(l2_page_table_1));

    // --- Populate L1 Table ---
    // L1 entry for the first 1GB (VA 0x00000000 - 0x3FFFFFFF), pointing to L2_TABLE_0
    kstd::uintptr_t l2_pt0_phys = get_physical_address(l2_page_table_0);
    l1_page_table[0] = l2_pt0_phys | PTE_VALID | PTE_TABLE_OR_PAGE; // Table descriptor

    // L1 entry for the second 1GB (VA 0x40000000 - 0x7FFFFFFF), pointing to L2_TABLE_1
    kstd::uintptr_t l2_pt1_phys = get_physical_address(l2_page_table_1);
    l1_page_table[1] = l2_pt1_phys | PTE_VALID | PTE_TABLE_OR_PAGE; // Table descriptor

    // Other L1 entries (2 to 511) remain invalid, covering up to 512GB if populated.

    Kernel::kprintf("MMU: L1 table populated (0: -> L2_0@0x%llx, 1: -> L2_1@0x%llx)\n", l2_pt0_phys, l2_pt1_phys);


    // --- Populate L2 Tables for 2MB Blocks ---
    // Each L2 entry covers 2MB. 512 entries cover 1GB.

    // L2_TABLE_0: Identity map for VA 0x00000000 - 0x3FFFFFFF (first 1GB)
    for (unsigned int i = 0; i < PAGE_TABLE_ENTRIES; ++i) {
        kstd::uintptr_t current_pa = static_cast<kstd::uintptr_t>(i) * PAGE_SIZE_2MB; // Physical address = Virtual address
        kstd::uint64_t flags = PTE_VALID | PTE_AF | PTE_SH_INNER_SHAREABLE; // Valid, Access Flag, Inner Shareable
                                                                        // PTE_TABLE_OR_PAGE is 0 for Block entry (default)

        // Determine memory type and permissions
        if (current_pa >= RPI4_PERIPHERAL_BASE_PHYS && (current_pa + PAGE_SIZE_2MB -1) <= RPI4_PERIPHERAL_END_PHYS) {
            // This 2MB block is within the main peripheral region
            flags |= PTE_ATTR_INDX(MAIR_IDX_DEVICE_NGNRNE);
            flags |= PTE_AP_EL1_RW_EL0_NONE; // EL1 R/W, EL0 No access
            flags |= PTE_PXN | PTE_UXN;      // Execute Never for peripherals
        } else if ( (current_pa >= (kstd::uintptr_t)KERNEL_START && current_pa < (kstd::uintptr_t)KERNEL_END) ||
                    ((current_pa + PAGE_SIZE_2MB -1) >= (kstd::uintptr_t)KERNEL_START && (current_pa + PAGE_SIZE_2MB -1) < (kstd::uintptr_t)KERNEL_END) ) {
            // This 2MB block overlaps with kernel image region
            flags |= PTE_ATTR_INDX(MAIR_IDX_NORMAL_C); // Normal Cacheable for Kernel
            flags |= PTE_AP_EL1_RW_EL0_NONE;          // EL1 R/W, EL0 No access
            // Allow kernel execution (PXN=0, UXN=1)
            flags &= ~PTE_PXN; // Privileged Execute
            flags |= PTE_UXN;  // Unprivileged Execute Never
        } else {
            // General RAM
            flags |= PTE_ATTR_INDX(MAIR_IDX_NORMAL_C); // Normal Cacheable for RAM
            flags |= PTE_AP_EL1_RW_EL0_NONE;          // EL1 R/W for now, can be EL0_RW if user space planned
            flags |= PTE_PXN | PTE_UXN;               // Execute Never for general RAM
        }
        l2_page_table_0[i] = current_pa | flags;
    }
    Kernel::kprintf("MMU: L2_TABLE_0 (0GB-1GB) populated.\n");

    // L2_TABLE_1: Identity map for VA 0x40000000 - 0x7FFFFFFF (second 1GB)
    for (unsigned int i = 0; i < PAGE_TABLE_ENTRIES; ++i) {
        kstd::uintptr_t va_base_for_this_table = PAGE_SIZE_1GB; // Start of 2nd GB
        kstd::uintptr_t current_pa = va_base_for_this_table + (static_cast<kstd::uintptr_t>(i) * PAGE_SIZE_2MB);
        kstd::uint64_t flags = PTE_VALID | PTE_AF | PTE_SH_INNER_SHAREABLE;

        if (current_pa >= RPI4_PERIPHERAL_BASE_PHYS && (current_pa + PAGE_SIZE_2MB -1) <= RPI4_PERIPHERAL_END_PHYS) {
            flags |= PTE_ATTR_INDX(MAIR_IDX_DEVICE_NGNRNE);
            flags |= PTE_AP_EL1_RW_EL0_NONE;
            flags |= PTE_PXN | PTE_UXN;
        } else {
            // General RAM in the second GB
            flags |= PTE_ATTR_INDX(MAIR_IDX_NORMAL_C);
            flags |= PTE_AP_EL1_RW_EL0_NONE;
            flags |= PTE_PXN | PTE_UXN;
        }
        l2_page_table_1[i] = current_pa | flags;
    }
    Kernel::kprintf("MMU: L2_TABLE_1 (1GB-2GB) populated.\n");

    // Special handling for specific peripheral base addresses if they fall on 2MB boundaries
    // and need different attributes than the general sweep.
    // Example: Ensure GICD_BASE and GPIO_BASE are device memory.
    // The current loop should handle them if RPI4_PERIPHERAL_BASE_PHYS is set correctly.
    // For instance, GICD_BASE (0xFF841000) is within 0xFE000000 - 0xFFFFFFFF.
    // The 2MB block containing 0xFF800000 to 0xFF9FFFFF would be marked as device.
}


void MMU::configure_translation_control() {
    Kernel::kprintf("MMU: Configuring TCR_EL1 and MAIR_EL1...\n");

    // --- Configure MAIR_EL1 (Memory Attribute Indirection Register) ---
    // Attr0: Device-nGnRnE (MAIR_IDX_DEVICE_NGNRNE = 0)
    // Attr1: Normal, Non-Cacheable (MAIR_IDX_NORMAL_NC = 1)
    // Attr2: Normal, Write-Back Cacheable (MAIR_IDX_NORMAL_C = 2)
    kstd::uint64_t mair_val = 0;
    mair_val |= (static_cast<kstd::uint64_t>(MAIR_ATTR_DEVICE_NGNRNE) << (MAIR_IDX_DEVICE_NGNRNE * 8));
    mair_val |= (static_cast<kstd::uint64_t>(MAIR_ATTR_NORMAL_NC)     << (MAIR_IDX_NORMAL_NC * 8));
    mair_val |= (static_cast<kstd::uint64_t>(MAIR_ATTR_NORMAL_C)      << (MAIR_IDX_NORMAL_C * 8));
    asm volatile("msr mair_el1, %0" : : "r"(mair_val));
    Kernel::kprintf("MMU: MAIR_EL1 set to 0x%llx\n", mair_val);

    // --- Configure TCR_EL1 (Translation Control Register) ---
    // Using TTBR0_EL1 for the first 2GB (identity map).
    // Assuming 4KB granule (TG0=00), 48-bit VA (T0SZ=16).
    // Physical Address Size (IPS). RPi4 supports up to 40-bit physical addresses for RAM.
    // For peripherals like GIC, addresses are higher (e.g., 0xFF841000).
    // Let's assume 40-bit PA space (IPS=010) to be safe for peripherals up to 1TB.
    // If strictly limiting to 2GB, 32-bit (IPS=000) would map up to 4GB.
    // Let's use IPS=010 (40-bit PA / 1TB).
    // Shareability (SH0) and Cacheability (IRGN0, ORGN0) for page table walks:
    // Typically Inner Shareable, Write-Back Read-Allocate Write-Allocate Cacheable.
    kstd::uint64_t tcr_val = 0;
    // T0SZ (bits 5:0): Region size for TTBR0_EL1 is 2^(64-T0SZ). T0SZ = 16 for 48-bit VA space.
    tcr_val |= (16ULL << 0);  // T0SZ = 16
    // TG0 (bits 15:14): Granule size for TTBR0. 00 = 4KB.
    tcr_val |= (0b00ULL << 14); // TG0 = 4KB granule
    // SH0 (bits 13:12): Shareability for TTBR0 walks. 0b11 = Inner Shareable.
    tcr_val |= (0b11ULL << 12); // SH0 = Inner Shareable
    // ORGN0 (bits 11:10): Outer cacheability for TTBR0 walks. 0b01 = Normal WB Read-Alloc Write-Alloc.
    tcr_val |= (0b01ULL << 10); // ORGN0 = Normal WB RAWA Cacheable
    // IRGN0 (bits 9:8): Inner cacheability for TTBR0 walks. 0b01 = Normal WB Read-Alloc Write-Alloc.
    tcr_val |= (0b01ULL << 8);  // IRGN0 = Normal WB RAWA Cacheable
    // EPD0 (bit 7): Disable page table walk for TTBR0 if set. Must be 0 to enable.
    // EPD1 (bit 23): Disable page table walk for TTBR1 if set. Set to 1 if TTBR1 not used.
    tcr_val |= (1ULL << 23); // Disable TTBR1 walks for now.
    // IPS (bits 34:32): Intermediate Physical Address Size. 010 = 40 bits.
    tcr_val |= (0b010ULL << 32); // IPS = 40-bit PA

    // TBI0 (bit 37): Top Byte Ignore for TTBR0_EL1. Set to 0 if using full VA range.
    // TBI1 (bit 38): Top Byte Ignore for TTBR1_EL1.

    asm volatile("msr tcr_el1, %0" : : "r"(tcr_val));
    Kernel::kprintf("MMU: TCR_EL1 set to 0x%llx\n", tcr_val);

    // --- Set TTBR0_EL1 (Translation Table Base Register 0) ---
    kstd::uintptr_t l1_pt_phys = get_physical_address(l1_page_table);
    asm volatile("msr ttbr0_el1, %0" : : "r"(l1_pt_phys));
    Kernel::kprintf("MMU: TTBR0_EL1 set to 0x%llx (L1 Table Physical Address)\n", l1_pt_phys);
}

void MMU::enable_mmu_and_caches() {
    Kernel::kprintf("MMU: Enabling MMU and caches...\n");

    // Ensure all previous writes to page tables and control registers are complete.
    asm volatile("dsb ish"); // Data Synchronization Barrier, inner shareable
    asm volatile("isb");    // Instruction Synchronization Barrier

    // Invalidate entire TLB (Translation Lookaside Buffer)
    // This is for EL1 and EL0.
    asm volatile("tlbi vmalle1is"); // Invalidate entire unified TLB, inner shareable, for EL1&0 stage 1

    asm volatile("dsb ish"); // Ensure TLB invalidation is complete
    asm volatile("isb");

    // --- Configure SCTLR_EL1 (System Control Register) ---
    kstd::uint64_t sctlr_val;
    asm volatile("mrs %0, sctlr_el1" : "=r"(sctlr_val));

    // Bits to set:
    // M (bit 0): MMU enable for EL1 and EL0 stage 1 address translation.
    // C (bit 2): Data Cache enable.
    // I (bit 12): Instruction Cache enable.
    // SA (bit 3): Stack Alignment Check enable for EL1.
    // SA0 (bit 4): Stack Alignment Check enable for EL0.
    // Other bits like EE (Endianness), WXN (Write Execute Never), etc., might be relevant.
    // Default RPi bootloader might already set some of these.
    // We want M=1, C=1, I=1.
    // For simplicity, let's preserve most existing SCTLR bits and just set M, C, I.
    // Important: Alignment checks (SCTLR_EL1.A, SA, SA0) should be enabled.
    // Endianness (SCTLR_EL1.EE) should be set correctly (usually Little Endian for RPi).

    sctlr_val |= (1ULL << 0);  // M - Enable MMU
    sctlr_val |= (1ULL << 2);  // C - Enable Data Cache
    sctlr_val |= (1ULL << 12); // I - Enable Instruction Cache
    // sctlr_val |= (1ULL << 3);  // SA - Stack Alignment Check EL1
    // sctlr_val |= (1ULL << 4);  // SA0 - Stack Alignment Check EL0
    // sctlr_val &= ~(1ULL << 1); // A - Alignment Check Disable (ensure it's 0 to enable checks)
                               // Default reset value for A is 0 (checks enabled).

    Kernel::kprintf("MMU: Writing 0x%llx to SCTLR_EL1 (current: read 0x%llx before modification)\n", sctlr_val, ({kstd::uint64_t r; asm volatile("mrs %0, sctlr_el1" : "=r"(r)); r;}) );

    asm volatile("msr sctlr_el1, %0" : : "r"(sctlr_val));
    asm volatile("isb"); // Synchronize context on this PE

    Kernel::kprintf("MMU: MMU and Caches Enabled (SCTLR_EL1 written).\n");
    kstd::uint64_t final_sctlr;
    asm volatile("mrs %0, sctlr_el1" : "=r"(final_sctlr));
    Kernel::kprintf("MMU: SCTLR_EL1 after enable: 0x%llx\n", final_sctlr);

    if (!(final_sctlr & 1)) {
        Kernel::panic("MMU FAILED TO ENABLE!");
    }
}


void MMU::init_and_enable() {
    // Check if MMU is already enabled. If so, perhaps skip or panic.
    kstd::uint64_t sctlr_val;
    asm volatile("mrs %0, sctlr_el1" : "=r"(sctlr_val));
    if (sctlr_val & 1) {
        Kernel::kprintf("MMU: Warning - MMU already enabled (SCTLR_EL1.M = 1).\n");
        // For now, we'll proceed, assuming it might be okay or for re-configuration.
        // A robust system might panic or handle this state carefully.
        // return; // Or proceed to reconfigure if that's the intent.
    }

    setup_page_tables();
    configure_translation_control();
    enable_mmu_and_caches();

    Kernel::kprintf("MMU Initialization Complete.\n");
}


// This file needs to be added to CPP_SOURCES in Makefile:
// $(ARCH_CORE_DIR)/mmu.cpp
// It is already listed in the Makefile from Step 1.

// In kernel_main.cpp, call Arch::Arm::MMU::init_and_enable()
// This should be done VERY early, before most other initializations that might
// rely on memory attributes or caching (like GIC, Timer if their registers
// are mapped as device memory). Console UART is usually simple enough.
// The call should be after basic console is up for kprintf, but before complex drivers.
//
// Order in kernel_main:
// 1. Allocator init
// 2. Console init (for kprintf during MMU setup)
// 3. MMU init and enable
// 4. Exception init (VBAR_EL1 - can be set before or after MMU, but vector table itself needs correct mapping)
//    VBAR_EL1 should point to the *virtual* address of the vectors if MMU is on.
//    If identity mapping, virtual == physical.
//    Let's ensure exceptions.S .vectors section is mapped as Normal_C, R-X for kernel.
//    The current page table setup maps KERNEL_START to KERNEL_END as Normal_C, R/W (kernel), X (kernel).
//    This should cover the .vectors section if it's linked within kernel image.
// 5. GIC init
// 6. Timer init
// The `init_exceptions()` which sets VBAR_EL1 should ideally be called *after* MMU is enabled if the
// vector table address `_exception_vectors` is a virtual address that requires MMU translation.
// If `_exception_vectors` is a physical address and VBAR_EL1 takes physical, it can be before.
// For simplicity with identity mapping, physical==virtual for kernel space, so order might not be critical.
// However, best practice is often MMU first, then VBAR with virtual address.
// The current `init_exceptions()` in exceptions.cpp uses the physical address of `_exception_vectors`.
// This is fine for identity mapping.

} // namespace Arm
} // namespace Arch
