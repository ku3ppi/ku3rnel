# KEKOS C++ Edition - Raspberry Pi 4 Bare-Metal Kernel

This project is a bare-metal kernel for the Raspberry Pi 4 (ARM AArch64), written in modern C++. It is inspired by an older x86 C-based kernel but re-implements functionality from the ground up using C++ idioms and targeting the ARM architecture.

## Features

*   **Target Architecture:** ARMv8-A AArch64 (specifically for Raspberry Pi 4 - Cortex-A72).
*   **Language:** Modern C++ (C++17) in a freestanding environment.
    *   No Standard C++ Library (e.g., `std::vector`, `iostream`).
    *   No exceptions (`-fno-exceptions`).
    *   No RTTI (`-fno-rtti`).
*   **Boot Process:** Produces a `kernel8.img` file compatible with the Raspberry Pi 4 bootloader.
*   **Console I/O:** Uses the PL011 UART (serial console) for all text input and output.
*   **Interrupt Handling:** Implements ARM GICv2 (GIC-400) for interrupt management and ARMv8-A exception handling.
*   **System Timer:** Utilizes the ARM Generic Timer (CNTP EL1) for periodic interrupts.
*   **Memory Management:**
    *   Basic C++ runtime support (`new`/`delete` via a simple bump allocator).
    *   MMU setup with identity mapping for the first 2GB of physical memory (2MB blocks), with distinct attributes for kernel code/data, RAM, and device memory.
*   **In-Memory Filesystem:** A simple block-based filesystem running in RAM.
    *   Supports file creation, deletion, reading, and writing.
    *   Flat directory structure.
*   **Interactive Shell:**
    *   Provides commands like `ls`, `create <file>`, `cat <file>`, `rm <file>`, `edit <file>`, `echo`, `clear`, `help`.
    *   Placeholders for `reboot` and `shutdown`.
*   **Text Editor:**
    *   Basic full-screen text editor (conceptually, UI is simple due to console limitations).
    *   Supports loading, saving, and simple text manipulation (character typing, backspace, enter, tab).

## Project Structure

```
.
├── arch/arm/                 # ARM-specific code (boot, core, peripherals)
├── include/                  # Global include directory (libcxx_support, kernel interfaces)
├── kernel/                   # Core kernel components (main, console, fs, shell, editor)
├── lib/                      # Utility libraries (kstd - minimal custom lib, printf)
├── toolchain/                # Toolchain files (linker script)
├── Makefile                  # Build system configuration
└── README.md                 # This file
```

## Prerequisites

*   An AArch64 cross-compiler toolchain (e.g., `aarch64-elf-gcc`, `aarch64-elf-g++`, `aarch64-elf-ld`).
    Ensure these are in your system's PATH or adjust the `PREFIX` variable in the `Makefile`.
*   `make` utility.
*   QEMU (for AArch64, e.g., `qemu-system-aarch64`) for emulation (optional but recommended for testing).
*   A Raspberry Pi 4 and a way to load `kernel8.img` (e.g., via SD card with appropriate bootloader configuration, or TFTP).

## Building the Kernel

1.  **Clone the repository (if applicable).**
2.  **Navigate to the project root directory.**
3.  **Build the kernel:**
    ```bash
    make
    ```
    This will compile the source files and link them to produce:
    *   `build/kernel8.elf`: The kernel in ELF format (useful for debugging).
    *   `build/kernel8.img`: The raw binary image suitable for booting on a Raspberry Pi 4.
    *   `build/kernel8.list`: A disassembly listing.

4.  **Clean build files:**
    ```bash
    make clean
    ```
    This will remove the `build` directory and all compiled files.

## Running with QEMU

QEMU can be used to emulate a Raspberry Pi 4 and run the kernel image. Note that full RPi4 peripheral emulation in QEMU can be tricky or incomplete for some features, but it's excellent for testing CPU execution, basic UART, and GIC/timer functionality.

1.  **Build the kernel** as described above to get `build/kernel8.img`.

2.  **Run QEMU:**
    The `Makefile` includes a convenience target:
    ```bash
    make qemu
    ```
    This typically executes a command similar to:
    ```bash
    qemu-system-aarch64 -M raspi4b -cpu cortex-a72 -m 1G -serial stdio -kernel build/kernel8.img
    ```
    *   `-M raspi4b`: Specifies the Raspberry Pi 4B machine model. (Note: QEMU's `raspi4b` support might alias to `raspi3b` in older versions or have limitations. The `virt` machine with a device tree might be another option for generic ARMv8 testing if RPi4-specific peripherals are not strictly needed for a given test.)
    *   `-cpu cortex-a72`: Specifies the CPU model.
    *   `-m 1G`: Allocates 1GB of RAM to the VM.
    *   `-serial stdio`: Redirects the guest's first serial port (UART0) to the host's standard I/O. This is how you'll interact with the kernel console.
    *   `-kernel build/kernel8.img`: Specifies the kernel image to load directly.

3.  **Debugging with QEMU and GDB:**
    To debug the kernel with GDB:
    *   Start QEMU in a paused state, listening for a GDB connection:
        ```bash
        make qemu-debug
        ```
        This adds `-s -S` to the QEMU command line (`-s` is a shorthand for `-gdb tcp::1234`, `-S` freezes CPU at startup).
    *   In another terminal, start GDB for AArch64:
        ```bash
        aarch64-elf-gdb
        ```
        Or, if your GDB is multi-arch:
        ```bash
        gdb
        ```
    *   Inside GDB, connect to QEMU and load symbols:
        ```gdb
        target remote localhost:1234
        symbol-file build/kernel8.elf
        # Set breakpoints, e.g., b kernel_main
        # Then continue execution: c
        ```
        The `Makefile` also provides a `make debug` target that can simplify launching GDB.

## Deployment to Raspberry Pi 4

1.  **Prepare an SD card:**
    *   Ensure it has the latest Raspberry Pi bootloader firmware (`bootcode.bin`, `start4.elf`, `fixup4.dat`).
    *   A `config.txt` file might be needed. A minimal one could be:
        ```txt
        arm_64bit=1
        enable_uart=1
        kernel=kernel8.img # This line tells the firmware to load your kernel image.
        ```
        Ensure no other `kernel=` line specifies a different kernel. Some boot configurations might automatically look for `kernel8.img`.

2.  **Copy `build/kernel8.img`** to the root directory of the SD card's boot partition.

3.  **Connect Serial Console:**
    Connect a USB-to-TTL serial adapter to the Raspberry Pi 4's GPIO pins for UART0:
    *   GPIO14 (TXD0) -> Adapter RX
    *   GPIO15 (RXD0) -> Adapter TX
    *   Ground -> Adapter Ground
    Use a terminal emulator (minicom, PuTTY, screen) on your host machine, configured for the serial port (e.g., `/dev/ttyUSB0` on Linux) with settings: `115200 baud, 8 data bits, no parity, 1 stop bit` (8N1).

4.  **Power on the Raspberry Pi 4.** You should see kernel boot messages on your serial console and eventually the shell prompt.

## Future Development / Areas for Improvement

*   More robust memory management (e.g., buddy allocator, slab allocator).
*   Proper user mode and process management.
*   Advanced console features (ANSI escape codes for cursor positioning, colors, screen clearing).
*   More complete device drivers (e.g., SD card, USB, network).
*   Thread-safe C++ runtime components (e.g., for `__cxa_guard_acquire`).
*   Detailed Device Tree Blob (DTB) parsing for hardware configuration.
*   Refined editor UI and features.
```
