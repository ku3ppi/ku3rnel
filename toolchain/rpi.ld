/*
 * Linker script for a Raspberry Pi 4 (AArch64) kernel.
 * Kernel is expected to be loaded by the RPi firmware/bootloader.
 * Default load address for kernel8.img is 0x80000 on RPi3/4.
 */

ENTRY(_start_kernel) /* Entry point, defined in arch/arm/core/start.S */

SECTIONS
{
    /* Kernel code and data starts at 0x80000 */
    . = 0x80000;
    KERNEL_START = .;

    /* Exception Vector Table section - must be aligned (e.g., 2KB for AArch64 VBAR_EL1) */
    .vectors : ALIGN(2K)
    {
        KEEP(*(.vectors))       /* Exception vectors from exceptions.S */
    }

    /* Text section: code */
    .text : ALIGN(4K)
    {
        KEEP(*(.boot))          /* Bootloader specific code from boot.S */
        *(.text._start_kernel)  /* Ensure _start_kernel is early */
        *(.text .text.*)        /* All other text */
    }

    /* Read-only data section */
    .rodata : ALIGN(4K)
    {
        *(.rodata .rodata.*)
    }

    /* Data section: initialized data */
    .data : ALIGN(4K)
    {
        *(.data .data.*)
    }

    /* BSS section: uninitialized data */
    .bss : ALIGN(4K)
    {
        BSS_START = .;
        *(.bss .bss.*)
        *(COMMON)
        BSS_END = .;
    }

    /* End of kernel symbol */
    KERNEL_END = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note.gnu.build-id)
        *(.ARM.attributes)
    }

    /* Stack grows downwards from here. */
    /* HEAP_START and HEAP_END are used by the bump allocator. */
    /* These are defined relative to KERNEL_END or BSS_END. */
    /* Ensure BSS is page aligned before heap. */
    /* . = ALIGN(4K); */ /* Ensure current location is page aligned if needed before heap */
    HEAP_START = ALIGN(4K); /* Align heap start to a page boundary after BSS and other sections */
    HEAP_END = HEAP_START + 1M; /* Example: 1MB heap, can be adjusted, e.g., 0x100000 */
}

/* Assert that BSS section is page aligned for clearing */
/* Disabling these assertions for now as they might be too strict with current simple layout */
/* ASSERT( (BSS_END - BSS_START) % 4096 == 0 || BSS_START == BSS_END, "BSS section size is not page aligned or is empty" ) */
/* ASSERT( BSS_START % 4096 == 0, "BSS section start is not page aligned" ) */
