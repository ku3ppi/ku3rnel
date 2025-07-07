#ifndef KERNEL_SHELL_SHELL_H
#define KERNEL_SHELL_SHELL_H

#include "commands.h"     // For ParsedCommand, CommandHandlerFunc
#include <kernel/filesystem/filesystem.h> // For Filesystem reference
#include <kernel/console.h> // For Console reference
#include <kstd/cstddef.h>   // For kstd::size_t

namespace Kernel {

// Forward declaration for Editor class for 'edit' command
class Editor;

class Shell {
public:
    Shell(Filesystem& fs, Console& console);
    ~Shell() = default;

    // Initialize the shell (if needed)
    void init();

    // Run the main shell loop
    void run();

    // Exit the shell (might not be used if shell runs indefinitely)
    void exit();

    // Getters for internal components if needed by commands directly
    Console& get_console() { return term_console; }
    Filesystem& get_filesystem() { return filesystem_instance; }
    Editor* get_editor(); // Will create if not exists, or return existing


private:
    Filesystem& filesystem_instance;
    Console& term_console;
    // Editor* text_editor; // Pointer to the editor instance, created on demand
    Editor editor_instance; // Make it a direct member instance

    bool running;

    // Command input buffer
    static constexpr kstd::size_t MAX_CMD_BUFFER_LEN = 256;
    char command_buffer[MAX_CMD_BUFFER_LEN];

    // Prompt string
    static constexpr const char* PROMPT_STRING = "KekOS C++ > ";

    // Helper methods
    void display_prompt();
    void read_command(); // Reads into command_buffer

    // Parses command_buffer into ParsedCommand structure.
    // Returns true on success, false if input is empty or too complex.
    bool parse_command(const char* buffer, ShellCommands::ParsedCommand& parsed_cmd) const;

    void execute_command(const ShellCommands::ParsedCommand& command);

    // Prevent copying
    Shell(const Shell&) = delete;
    Shell& operator=(const Shell&) = delete;
};

// Global shell runner function, to be called from kernel_main perhaps after other inits
void start_kernel_shell();


} // namespace Kernel

#endif // KERNEL_SHELL_SHELL_H
