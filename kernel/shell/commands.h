#ifndef KERNEL_SHELL_COMMANDS_H
#define KERNEL_SHELL_COMMANDS_H

#include <kstd/cstddef.h> // For kstd::size_t

namespace Kernel {

class Shell; // Forward declaration for Shell reference if needed by command functions

namespace ShellCommands {

// Maximum number of arguments a command can take (including command name itself)
constexpr kstd::size_t MAX_COMMAND_ARGS = 8;
// Maximum length of a single argument
constexpr kstd::size_t MAX_ARG_LENGTH = 64;


// Structure to hold a parsed command
struct ParsedCommand {
    char args[MAX_COMMAND_ARGS][MAX_ARG_LENGTH];
    int arg_count;

    ParsedCommand() : arg_count(0) {
        for (kstd::size_t i = 0; i < MAX_COMMAND_ARGS; ++i) {
            args[i][0] = '\0';
        }
    }

    const char* name() const {
        return (arg_count > 0) ? args[0] : nullptr;
    }
};


// Type definition for a shell command handler function
// Takes the parsed command (argv, argc style) and a reference to the shell instance (for context like FS)
// Returns an integer status code (e.g., 0 for success, non-zero for error)
using CommandHandlerFunc = int (*)(const ParsedCommand& command, Shell& shell_instance);

// Structure to map command strings to their handlers
struct CommandDefinition {
    const char* name;
    CommandHandlerFunc handler;
    const char* help_summary;
    const char* help_details; // More detailed usage
};


// Command handler function declarations
// These will be implemented in commands.cpp
int handle_help(const ParsedCommand& command, Shell& shell_instance);
int handle_ls(const ParsedCommand& command, Shell& shell_instance);
int handle_create(const ParsedCommand& command, Shell& shell_instance);
int handle_edit(const ParsedCommand& command, Shell& shell_instance);
int handle_clear(const ParsedCommand& command, Shell& shell_instance);
int handle_reboot(const ParsedCommand& command, Shell& shell_instance);
int handle_shutdown(const ParsedCommand& command, Shell& shell_instance);
int handle_echo(const ParsedCommand& command, Shell& shell_instance); // Example new command
int handle_cat(const ParsedCommand& command, Shell& shell_instance); // Example new command: print file content
int handle_rm(const ParsedCommand& command, Shell& shell_instance); // Example new command: remove file


// Array of command definitions
// This will be defined in commands.cpp
extern const CommandDefinition command_table[];
extern const kstd::size_t command_table_size;

} // namespace ShellCommands
} // namespace Kernel

#endif // KERNEL_SHELL_COMMANDS_H
