#include "shell.h"
#include "commands.h" // For command_table, ParsedCommand
#include <kernel/console.h>
#include <kernel/filesystem/filesystem.h>
#include <kernel/editor/editor.h> // For Kernel::Editor, will be implemented in next step
#include <lib/printf/printf.h>  // For Kernel::kprintf
#include <kstd/cstring.h>    // For kstrlen, kstrcmp, kstrncpy

namespace Kernel {

// Global shell runner function
static Shell* g_global_shell_instance = nullptr;

void start_kernel_shell() {
    // This assumes global_console() and global_filesystem() are already initialized
    if (!g_global_shell_instance) {
        // Create the shell instance dynamically or have it as a global static.
        // If dynamic, ensure it's cleaned up if the shell can exit.
        // For a kernel shell, it often runs forever.
        static Shell kernel_shell(global_filesystem(), global_console());
        g_global_shell_instance = &kernel_shell;
        g_global_shell_instance->init(); // Initialize if needed
    }

    if (g_global_shell_instance) {
        Kernel::kprintf("\nStarting KEKOS C++ Shell...\n");
        g_global_shell_instance->run();
        Kernel::kprintf("Kernel shell exited (should not happen for main kernel shell).\n");
    } else {
        Kernel::kprintf("Error: Could not start kernel shell.\n");
    }
}


Shell::Shell(Filesystem& fs, Console& console)
    : filesystem_instance(fs),
      term_console(console),
      editor_instance(console, fs), // Initialize editor_instance
      running(false) {
    command_buffer[0] = '\0';
}

void Shell::init() {
    // Any one-time shell initialization can go here.
    term_console.println("Shell initialized. Type 'help' for commands.");
    // Editor is already constructed via member initializer list.
    // If editor_instance needed an init() call: editor_instance.init();
}

Editor* Shell::get_editor() {
    // Return a pointer to the member instance
    return &editor_instance;
}


void Shell::display_prompt() {
    term_console.print(PROMPT_STRING);
}

void Shell::read_command() {
    // Using Console::read_line to get input
    kstd::kmemset(command_buffer, 0, MAX_CMD_BUFFER_LEN);
    term_console.read_line(command_buffer, MAX_CMD_BUFFER_LEN);
    // read_line handles basic echoing and backspace.
}

bool Shell::parse_command(const char* buffer, ShellCommands::ParsedCommand& parsed_cmd) const {
    parsed_cmd.arg_count = 0;
    const char* current_char = buffer;
    bool in_token = false;
    int arg_idx = 0;
    int char_idx_in_arg = 0;

    while (*current_char != '\0' && arg_idx < ShellCommands::MAX_COMMAND_ARGS) {
        // Skip leading whitespace
        while (*current_char == ' ' || *current_char == '\t') {
            current_char++;
        }
        if (*current_char == '\0') break; // End of buffer

        // Start of a token
        in_token = true;
        char_idx_in_arg = 0;
        parsed_cmd.args[arg_idx][0] = '\0'; // Clear current arg buffer

        while (*current_char != '\0' && *current_char != ' ' && *current_char != '\t') {
            if (char_idx_in_arg < ShellCommands::MAX_ARG_LENGTH - 1) {
                parsed_cmd.args[arg_idx][char_idx_in_arg++] = *current_char;
            }
            current_char++;
        }
        parsed_cmd.args[arg_idx][char_idx_in_arg] = '\0'; // Null-terminate argument

        if (in_token && char_idx_in_arg > 0) { // If we actually read an argument
            arg_idx++;
        }
        in_token = false;
    }
    parsed_cmd.arg_count = arg_idx;
    return (parsed_cmd.arg_count > 0);
}


void Shell::execute_command(const ShellCommands::ParsedCommand& command) {
    if (command.arg_count == 0 || command.name() == nullptr) {
        return; // No command entered
    }

    const char* cmd_name = command.name();
    bool found_cmd = false;

    for (kstd::size_t i = 0; i < ShellCommands::command_table_size; ++i) {
        if (ShellCommands::command_table[i].name &&
            kstd::kstrcmp(cmd_name, ShellCommands::command_table[i].name) == 0) {
            if (ShellCommands::command_table[i].handler) {
                // Pass 'this' shell instance to the handler
                ShellCommands::command_table[i].handler(command, *this);
                found_cmd = true;
                break;
            }
        }
    }

    if (!found_cmd) {
        Kernel::kprintf("Unknown command: '%s'. Type 'help'.\n", cmd_name);
    }
}

void Shell::run() {
    running = true;
    if (!filesystem_instance.file_exists("test.txt")) { // Example: ensure FS is usable
        // filesystem_instance.create_file("welcome.txt");
    }

    while (running) {
        display_prompt();
        read_command();

        ShellCommands::ParsedCommand parsed_cmd;
        if (parse_command(command_buffer, parsed_cmd)) {
            if (kstd::kstrcmp(parsed_cmd.name(), "exit_shell_completely_for_debug") == 0) { // Hidden command to stop shell
                running = false;
                term_console.println("Exiting shell (debug command)...");
                break;
            }
            execute_command(parsed_cmd);
        }
    }
}

void Shell::exit() {
    running = false;
    term_console.println("Shell exiting.");
}


// In kernel_main.cpp, after all essential initializations (console, fs, interrupts, timer, etc.),
// call Kernel::start_kernel_shell();
// This will be done in the next step.

} // namespace Kernel
