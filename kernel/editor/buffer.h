#ifndef KERNEL_EDITOR_BUFFER_H
#define KERNEL_EDITOR_BUFFER_H

#include <kstd/cstddef.h>   // For kstd::size_t
#include <kstd/cstdint.h>   // For kstd::uintXX_t
// #include <kstd/vector.h> // If we had a kstd::vector<kstd::string>-like structure
// #include <kstd/string.h> // If we had a kstd::string

namespace Kernel {
namespace Editor {

// Configuration for the editor buffer
// Similar to EDITOR_BUFFER_LINES and EDITOR_BUFFER_COLS from kernel.c
constexpr kstd::size_t MAX_BUFFER_LINES = 64; // Max lines the buffer can hold internally
constexpr kstd::size_t MAX_LINE_LENGTH  = 80;  // Max characters per line (excluding null terminator)

// Represents a single line in the text buffer
struct Line {
    char text[MAX_LINE_LENGTH + 1]; // +1 for null terminator
    kstd::size_t current_length;    // Actual length of text in this line

    Line() : current_length(0) {
        text[0] = '\0';
    }

    // Clears the line
    void clear();
    // Appends a character if space allows
    bool append_char(char c);
    // Inserts a character at a given column
    bool insert_char(kstd::size_t col, char c);
    // Deletes a character at a given column
    bool delete_char(kstd::size_t col);
    // Gets character at column
    char get_char(kstd::size_t col) const;
    // Sets character at column (use with care, doesn't update length automatically)
    void set_char(kstd::size_t col, char c);
};


class EditorBuffer {
public:
    EditorBuffer();
    ~EditorBuffer() = default;

    // Load content into the buffer (e.g., from a file string)
    // Content is assumed to be a single string with newline characters.
    void load_content(const char* content, kstd::size_t content_length);

    // Serialize buffer content into a single string (with newlines)
    // buffer: Destination buffer to write to.
    // buffer_size: Size of the destination buffer.
    // out_length (out): Actual length written to buffer (excluding null terminator).
    // Returns true on success, false if content is too large for provided buffer.
    bool get_content_as_string(char* out_buffer, kstd::size_t out_buffer_size, kstd::size_t& out_length) const;

    // Get a specific line (const access)
    const Line* get_line(kstd::size_t line_num) const;
    // Get a specific line (non-const access)
    Line* get_line_mut(kstd::size_t line_num);

    kstd::size_t get_num_lines() const { return num_lines_in_use; }
    kstd::size_t get_max_lines() const { return MAX_BUFFER_LINES; }
    static kstd::size_t get_max_line_length() { return MAX_LINE_LENGTH; }

    // Insert a new empty line at a given line number, shifting subsequent lines down.
    // Returns true on success, false if buffer is full.
    bool insert_new_line_at(kstd::size_t line_num);

    // Delete a line at a given line number, shifting subsequent lines up.
    // Returns true on success, false if line_num is invalid.
    bool delete_line_at(kstd::size_t line_num);

    // Clears the entire buffer
    void clear_all();

private:
    Line lines[MAX_BUFFER_LINES];
    kstd::size_t num_lines_in_use; // Number of lines currently holding content

    // Prevent copying
    EditorBuffer(const EditorBuffer&) = delete;
    EditorBuffer& operator=(const EditorBuffer&) = delete;
};

} // namespace Editor
} // namespace Kernel

#endif // KERNEL_EDITOR_BUFFER_H
