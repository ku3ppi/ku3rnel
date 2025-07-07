# Toolchain configuration
PREFIX      := aarch64-elf-
CC          := $(PREFIX)gcc
CXX         := $(PREFIX)g++
LD          := $(PREFIX)ld
OBJCOPY     := $(PREFIX)objcopy
OBJDUMP     := $(PREFIX)objdump
GDB         := $(PREFIX)gdb

# Source directories
ARCH_DIR        := arch/arm
ARCH_BOOT_DIR   := $(ARCH_DIR)/boot
ARCH_CORE_DIR   := $(ARCH_DIR)/core
ARCH_PERI_DIR   := $(ARCH_DIR)/peripherals
KERNEL_DIR      := kernel
KERNEL_FS_DIR   := $(KERNEL_DIR)/filesystem
KERNEL_SHELL_DIR:= $(KERNEL_DIR)/shell
KERNEL_EDIT_DIR := $(KERNEL_DIR)/editor
LIB_DIR         := lib
LIB_KSTD_DIR    := $(LIB_DIR)/kstd
LIB_PRINTF_DIR  := $(LIB_DIR)/printf
INCLUDE_DIR     := include
LIBCXX_DIR      := $(INCLUDE_DIR)/libcxx_support

# Target
TARGET_NAME := kernel8
BUILD_DIR   := build
TARGET_IMG  := $(BUILD_DIR)/$(TARGET_NAME).img
TARGET_ELF  := $(BUILD_DIR)/$(TARGET_NAME).elf
TARGET_LST  := $(BUILD_DIR)/$(TARGET_NAME).list

# Compiler and Linker Flags
# For Raspberry Pi 4 (Cortex-A72)
CPUFLAGS    := -mcpu=cortex-a72 -mtune=cortex-a72
# Common flags for C and C++
COMMONFLAGS := $(CPUFLAGS) -Wall -Wextra -O2 -ffreestanding -nostdlib -fno-builtin -fno-exceptions -fno-rtti -g
CFLAGS      := $(COMMONFLAGS)
CXXFLAGS    := $(COMMONFLAGS) -std=c++17 -fno-use-cxa-atexit

# Linker script
LINKER_SCRIPT := toolchain/rpi.ld
LDFLAGS       := -T $(LINKER_SCRIPT) -nostdlib --gc-sections

# Source files
S_SOURCES := \
    $(ARCH_BOOT_DIR)/boot.S \
    $(ARCH_CORE_DIR)/start.S \
    $(ARCH_CORE_DIR)/exceptions.S

CPP_SOURCES := \
    $(KERNEL_DIR)/main.cpp \
    $(KERNEL_DIR)/panic.cpp \
    $(KERNEL_DIR)/console.cpp \
    $(ARCH_PERI_DIR)/gpio.cpp \
    $(ARCH_PERI_DIR)/uart.cpp \
    $(ARCH_PERI_DIR)/timer.cpp \
    $(ARCH_CORE_DIR)/exceptions.cpp \
    $(ARCH_CORE_DIR)/gic.cpp \
    $(ARCH_CORE_DIR)/mmu.cpp \
    $(ARCH_DIR)/common/arm_common.cpp \
    $(LIBCXX_DIR)/cxx_support.cpp \
    $(LIB_KSTD_DIR)/cstring.cpp \
    $(LIB_PRINTF_DIR)/printf.cpp \
    $(KERNEL_FS_DIR)/file.cpp \
    $(KERNEL_FS_DIR)/filesystem.cpp \
    $(KERNEL_SHELL_DIR)/commands.cpp \
    $(KERNEL_SHELL_DIR)/shell.cpp \
    $(KERNEL_EDIT_DIR)/editor.cpp \
    $(KERNEL_EDIT_DIR)/buffer.cpp

# Object files
S_OBJECTS   := $(patsubst %.S, $(BUILD_DIR)/%.o, $(filter %.S, $(S_SOURCES)))
CPP_OBJECTS := $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(filter %.cpp, $(CPP_SOURCES)))
OBJECTS     := $(S_OBJECTS) $(CPP_OBJECTS)

# Include paths
INCLUDES := \
    -I$(INCLUDE_DIR) \
    -I$(ARCH_DIR)/include \
    -I$(LIB_DIR) \
    -I$(KERNEL_DIR)

.PHONY: all clean qemu debug

all: $(TARGET_IMG)

$(TARGET_IMG): $(TARGET_ELF)
	@echo "  OBJCOPY $(TARGET_ELF) -> $(TARGET_IMG)"
	@$(OBJCOPY) -O binary $(TARGET_ELF) $(TARGET_IMG)

$(TARGET_ELF): $(OBJECTS) $(LINKER_SCRIPT)
	@echo "  LD $(OBJECTS) -> $(TARGET_ELF)"
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJECTS) -o $(TARGET_ELF) $(foreach lib,$(LIBS),-l$(lib))
	@$(OBJDUMP) -D $(TARGET_ELF) > $(TARGET_LST)

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@echo "  AS $< -> $@"
	@$(CXX) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "  CXX $< -> $@"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@echo "  CLEAN"
	@rm -rf $(BUILD_DIR)

# QEMU for Raspberry Pi 4 (requires a suitable aarch64 BIOS/firmware like qemu_efi.fd)
# Adjust QEMU path and options as needed.
QEMU_SYSTEM   := qemu-system-aarch64
QEMU_MACHINE  := raspi4b # This is an alias for 'raspi3b' in older QEMU, newer might have 'virt' or other options
QEMU_CPU      := cortex-a72
QEMU_MEM      := 1G
QEMU_ARGS     := -M $(QEMU_MACHINE) -cpu $(QEMU_CPU) -m $(QEMU_MEM) -serial stdio -kernel $(TARGET_IMG)
# For debugging with GDB: -s -S (starts QEMU paused, listening on port 1234)
QEMU_DEBUG_ARGS := -s -S

qemu: $(TARGET_IMG)
	@echo "  QEMU $(TARGET_IMG)"
	@$(QEMU_SYSTEM) $(QEMU_ARGS)

qemu-debug: $(TARGET_IMG)
	@echo "  QEMU (Debug) $(TARGET_IMG)"
	@echo "  Run GDB with: $(GDB) -ex \"target remote localhost:1234\" -ex \"symbol-file $(TARGET_ELF)\""
	@$(QEMU_SYSTEM) $(QEMU_ARGS) $(QEMU_DEBUG_ARGS)

# Example: make GDBINIT="-ex 'b kernel_main'" debug
debug: $(TARGET_ELF)
	$(GDB) -ex "target remote localhost:1234" -ex "symbol-file $(TARGET_ELF)" $(GDBINIT)

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Ensure subdirectories for objects are created
$(patsubst %,$(BUILD_DIR)/%.d,$(S_SOURCES) $(CPP_SOURCES)): | $(BUILD_DIR)
	mkdir -p $(dir $@)

# Note: Dependency generation could be added for more robust builds.
# For now, this Makefile is simplified.
# All object files depend on Makefile changes.
$(OBJECTS): Makefile
