#include "commands.h"
#include "shell.h" // For Shell& shell_instance type, and access to console/fs
#include <kernel/console.h>
#include <kernel/filesystem/filesystem.h>
#include <kernel/filesystem/file.h> // For FS::File
#include <lib/printf/printf.h> // For Kernel::kprintf
#include <kstd/cstring.h>   // For kstrcmp, kstrcpy, kstrlen
#include <kstd/cstddef.h>   // For kstd::size_t

// For reboot/shutdown - these are platform specific.
// We might need to call some Arch specific functions.
// For now, they'll just print messages.
namespace Arch { void arch_reboot(); void arch_shutdown(); }


namespace Kernel {
namespace ShellCommands {

// --- Command Handler Implementations ---

int handle_help(const ParsedCommand& command, Shell& shell_instance) {
    (void)command; // Not using specific args for general help
    Console& con = shell_instance.get_console();
    con.println("KEKOS C++ Shell - Available Commands:");
    for (kstd::size_t i = 0; i < command_table_size; ++i) {
        if (command_table[i].name && command_table[i].help_summary) {
            Kernel::kprintf("  %-10s - %s\n", command_table[i].name, command_table[i].help_summary);
        }
    }
    con.println("Type 'help <command>' for more details on a specific command (TODO).");
    return 0;
}

int handle_ls(const ParsedCommand& command, Shell& shell_instance) {
    (void)command; // TODO: Could support path argument later
    shell_instance.get_filesystem().list_files_to_console();
    return 0;
}

int handle_create(const ParsedCommand& command, Shell& shell_instance) {
    if (command.arg_count < 2) {
        shell_instance.get_console().println("Usage: create <filename>");
        return 1;
    }
    const char* filename = command.args[1];
    FS::ErrorCode res = shell_instance.get_filesystem().create_file(filename);

    switch (res) {
        case FS::ErrorCode::OK:
            Kernel::kprintf("File '%s' created.\n", filename);
            break;
        case FS::ErrorCode::ALREADY_EXISTS:
            Kernel::kprintf("Error: File '%s' already exists.\n", filename);
            break;
        case FS::ErrorCode::FILESYSTEM_FULL:
            shell_instance.get_console().println("Error: Filesystem full (no more file slots).");
            break;
        case FS::ErrorCode::INVALID_NAME:
            shell_instance.get_console().println("Error: Invalid filename.");
            break;
        default:
            Kernel::kprintf("Error: Could not create file '%s' (code %d).\n", filename, static_cast<int>(res));
            break;
    }
    return (res == FS::ErrorCode::OK) ? 0 : 1;
}

int handle_edit(const ParsedCommand& command, Shell& shell_instance) {
    if (command.arg_count < 2) {
        shell_instance.get_console().println("Usage: edit <filename>");
        return 1;
    }
    const char* filename = command.args[1];
    // Kernel::kprintf("Shell: 'edit %s' called.\n", filename);

    Editor* editor = shell_instance.get_editor();
    if (editor) {
        // Temporarily "clear" screen for editor by printing newlines
        for(int i=0; i<5; ++i) shell_instance.get_console().put_char('\n');
        editor->open_and_run(filename); // This will run the editor's main loop
        // After editor exits, the shell continues.
        // Shell's redraw/prompt will happen naturally in its loop.
        // Might want to explicitly clear/redraw shell prompt area if editor did full screen takeover.
        // For now, assume editor prints its output and then shell continues.
        Kernel::kprintf("\nReturned to shell from editor.\n"); // Message that editor exited
    } else {
        // This case should ideally not happen if get_editor() always returns a valid instance.
        shell_instance.get_console().println("Error: Editor component could not be accessed.");
        return 1;
    }
    return 0;
}

int handle_clear(const ParsedCommand& command, Shell& shell_instance) {
    (void)command;
    // This is tricky without direct console control codes or full screen buffer.
    // A simple way is to print many newlines.
    // A better way would be a Console::clear_screen() method that uses terminal codes if supported.
    // For now, let's assume Console class will get a clear method.
    // Kernel::global_console().clear_screen(); // If available
    for (int i = 0; i < 30; ++i) shell_instance.get_console().put_char('\n'); // Simple "clear"
    Kernel::kprintf("\n--- Screen Cleared (basic) ---\n\n"); // Placeholder
    return 0;
}

int handle_reboot(const ParsedCommand& command, Shell& shell_instance) {
    (void)command;
    shell_instance.get_console().println("Attempting reboot...");
    // Arch::arch_reboot(); // Platform-specific reboot call
    // For now, simulate by halting after message.
    Kernel::kprintf("Reboot: System will halt (simulation).\n");
    Kernel::panic("Simulated Reboot requested by user."); // Halts the system
    return 0;
}

int handle_shutdown(const ParsedCommand& command, Shell& shell_instance) {
    (void)command;
    shell_instance.get_console().println("System shutting down...");
    // Arch::arch_shutdown(); // Platform-specific shutdown call
    Kernel::kprintf("Shutdown: System will halt.\n");
    Kernel::panic("Shutdown requested by user."); // Halts the system
    return 0;
}

int handle_echo(const ParsedCommand& command, Shell& shell_instance) {
    for (int i = 1; i < command.arg_count; ++i) {
        shell_instance.get_console().print(command.args[i]);
        if (i < command.arg_count - 1) {
            shell_instance.get_console().put_char(' ');
        }
    }
    shell_instance.get_console().put_char('\n');
    return 0;
}

int handle_cat(const ParsedCommand& command, Shell& shell_instance) {
    if (command.arg_count < 2) {
        shell_instance.get_console().println("Usage: cat <filename>");
        return 1;
    }
    const char* filename = command.args[1];
    FS::File* file_obj = nullptr;
    FS::ErrorCode res = shell_instance.get_filesystem().open_file(filename, FS::OpenMode::READ, file_obj);

    if (res != FS::ErrorCode::OK || !file_obj) {
        Kernel::kprintf("Error: Cannot open file '%s' (code %d).\n", filename, static_cast<int>(res));
        if(file_obj) delete file_obj; // Important: clean up if open succeeded but something else failed.
                                     // Or if open_file returns Error and valid file_obj on partial success.
                                     // Current open_file should set file_obj to nullptr on error.
        return 1;
    }

    char buffer[257]; // Read in 256 byte chunks + null terminator
    kstd::size_t bytes_read;
    Console& con = shell_instance.get_console();

    while (true) {
        kstd::kmemset(buffer, 0, sizeof(buffer));
        res = file_obj->read(buffer, 256, bytes_read);
        if (res != FS::ErrorCode::OK) {
            Kernel::kprintf("\nError reading file '%s'.\n", filename);
            break;
        }
        if (bytes_read == 0) { // EOF
            break;
        }
        buffer[bytes_read] = '\0'; // Ensure null termination for printing as string
        con.print(buffer); // Print the chunk
    }
    // con.put_char('\n'); // Ensure newline at the end if file didn't have one

    delete file_obj; // Close/release the file object
    return 0;
}

int handle_rm(const ParsedCommand& command, Shell& shell_instance) {
    if (command.arg_count < 2) {
        shell_instance.get_console().println("Usage: rm <filename>");
        return 1;
    }
    const char* filename = command.args[1];
    FS::ErrorCode res = shell_instance.get_filesystem().delete_file(filename);
    if (res == FS::ErrorCode::OK) {
        Kernel::kprintf("File '%s' removed.\n", filename);
    } else if (res == FS::ErrorCode::NOT_FOUND) {
        Kernel::kprintf("Error: File '%s' not found.\n", filename);
    } else {
        Kernel::kprintf("Error: Could not remove file '%s' (code %d).\n", filename, static_cast<int>(res));
    }
    return (res == FS::ErrorCode::OK) ? 0 : 1;
}


// --- Command Table Definition ---
// This table maps command strings to their handler functions and help text.
const CommandDefinition command_table[] = {
    {"help",     handle_help,     "Show this help message.", "Usage: help [command]"},
    {"ls",       handle_ls,       "List files in the current directory.", "Usage: ls"},
    {"create",   handle_create,   "Create an empty file.", "Usage: create <filename>"},
    {"edit",     handle_edit,     "Open a file in the text editor.", "Usage: edit <filename>"},
    {"cat",      handle_cat,      "Display file content.", "Usage: cat <filename>"},
    {"rm",       handle_rm,       "Remove (delete) a file.", "Usage: rm <filename>"},
    {"echo",     handle_echo,     "Display a line of text.", "Usage: echo [text ...]"},
    {"clear",    handle_clear,    "Clear the terminal screen.", "Usage: clear"},
    {"reboot",   handle_reboot,   "Reboot the system (simulated).", "Usage: reboot"},
    {"shutdown", handle_shutdown, "Shut down the system (simulated).", "Usage: shutdown"}
    // Add more commands here
};

const kstd::size_t command_table_size = sizeof(command_table) / sizeof(CommandDefinition);

} // namespace ShellCommands
} // namespace Kernel


// Dummy implementations for Arch::arch_reboot and Arch::arch_shutdown
// These would normally be in an arch-specific file.
namespace Arch {
    void arch_reboot() {
        Kernel::kprintf("ARCH: Platform reboot sequence would be called here.\n");
        // Typically involves writing to a system control register or using a watchdog.
    }
    void arch_shutdown() {
        Kernel::kprintf("ARCH: Platform shutdown sequence would be called here.\n");
        // E.g., for Raspberry Pi, a mailbox call to the VideoCore.
    }
}
