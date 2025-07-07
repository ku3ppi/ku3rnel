#include "buffer.h"
#include <kstd/cstring.h>   // For kmemset, kstrlen, kmemcpy, kmemmove
#include <kstd/algorithm.h> // For kstd::min

namespace Kernel {
namespace Editor {

// --- Line struct methods ---

void Line::clear() {
    text[0] = '\0';
    current_length = 0;
}

bool Line::append_char(char c) {
    if (current_length < MAX_LINE_LENGTH) {
        text[current_length++] = c;
        text[current_length] = '\0';
        return true;
    }
    return false;
}

bool Line::insert_char(kstd::size_t col, char c) {
    if (current_length >= MAX_LINE_LENGTH) return false; // Line is full
    if (col > current_length) col = current_length; // Clamp to end if col is too large

    // Shift characters to the right to make space
    // Using kmemmove because src and dest overlap
    kstd::kmemmove(&text[col + 1], &text[col], current_length - col + 1); // +1 for null terminator

    text[col] = c;
    current_length++;
    return true;
}

bool Line::delete_char(kstd::size_t col) {
    if (col >= current_length || current_length == 0) return false; // Nothing to delete

    // Shift characters to the left
    // Using kmemmove because src and dest overlap
    kstd::kmemmove(&text[col], &text[col + 1], current_length - col); // current_length - col includes null term
    current_length--;
    return true;
}

char Line::get_char(kstd::size_t col) const {
    if (col < current_length) {
        return text[col];
    }
    return '\0'; // Or some other indicator for out of bounds
}

void Line::set_char(kstd::size_t col, char c) {
    if (col < MAX_LINE_LENGTH) {
        text[col] = c;
        // Note: This does not update current_length or handle null terminator
        // if 'c' is '\0' or if col >= current_length.
        // Use with caution, primarily for direct manipulation where length is managed externally.
    }
}


// --- EditorBuffer class methods ---

EditorBuffer::EditorBuffer() : num_lines_in_use(0) {
    // Ensure all lines are initially cleared (Line constructor handles this)
    // For safety, can explicitly clear here too if Line constructor was trivial.
    // num_lines_in_use starts at 0; lines are "used" as content is added.
    // Default to one empty line for a new buffer.
    if (MAX_BUFFER_LINES > 0) {
        lines[0].clear(); // First line is present but empty
        num_lines_in_use = 1;
    }
}

void EditorBuffer::clear_all() {
    for (kstd::size_t i = 0; i < MAX_BUFFER_LINES; ++i) {
        lines[i].clear();
    }
    if (MAX_BUFFER_LINES > 0) {
        num_lines_in_use = 1; // Always have at least one (empty) line
    } else {
        num_lines_in_use = 0;
    }
}


void EditorBuffer::load_content(const char* content, kstd::size_t content_length) {
    clear_all();
    if (!content || content_length == 0) {
        return; // Buffer remains cleared with one empty line
    }

    kstd::size_t current_buffer_line = 0;
    kstd::size_t current_pos_in_content = 0;

    while (current_pos_in_content < content_length && current_buffer_line < MAX_BUFFER_LINES) {
        Line* line_ptr = &lines[current_buffer_line];
        line_ptr->clear();

        while (current_pos_in_content < content_length) {
            char c = content[current_pos_in_content++];
            if (c == '\n') {
                break; // Move to next buffer line
            }
            if (c == '\r') { // Ignore carriage returns for internal representation
                continue;
            }
            if (!line_ptr->append_char(c)) {
                // Line is full, truncate or handle error
                // For now, content for this line is truncated.
                // Need to consume rest of this logical line from 'content' until next '\n'
                while (current_pos_in_content < content_length && content[current_pos_in_content] != '\n') {
                    current_pos_in_content++;
                }
                if (current_pos_in_content < content_length && content[current_pos_in_content] == '\n') {
                     current_pos_in_content++; // Consume the newline too
                }
                break;
            }
        }
        current_buffer_line++;
    }
    num_lines_in_use = kstd::max(static_cast<kstd::size_t>(1), current_buffer_line);
    // Ensure at least one line, even if content was empty or only newlines.
    // If content ended without newline, last line is used. If it ended with newline, new empty line follows.
}


bool EditorBuffer::get_content_as_string(char* out_buffer, kstd::size_t out_buffer_size, kstd::size_t& out_length) const {
    out_length = 0;
    if (!out_buffer || out_buffer_size == 0) return false;

    kstd::size_t buffer_pos = 0;
    bool success = true;

    for (kstd::size_t i = 0; i < num_lines_in_use; ++i) {
        const Line* line_ptr = &lines[i];
        // Copy line text
        if (buffer_pos + line_ptr->current_length < out_buffer_size) {
            kstd::kmemcpy(out_buffer + buffer_pos, line_ptr->text, line_ptr->current_length);
            buffer_pos += line_ptr->current_length;
        } else {
            // Not enough space in output buffer for this line's content
            kstd::size_t can_copy = out_buffer_size - buffer_pos -1; // -1 for potential null term
            if (can_copy > 0 && line_ptr->current_length > 0) {
                 kstd::kmemcpy(out_buffer + buffer_pos, line_ptr->text, can_copy);
                 buffer_pos += can_copy;
            }
            success = false; // Content truncated
            break;
        }

        // Add newline, unless it's the very last line and it's empty (optional behavior)
        // For simplicity, always add newline if it's not the last line of actual content.
        if (i < num_lines_in_use - 1) { // Don't add newline after the very last line from buffer
            if (buffer_pos + 1 < out_buffer_size) {
                out_buffer[buffer_pos++] = '\n';
            } else {
                success = false; // Not enough space for newline
                break;
            }
        }
    }

    if (buffer_pos < out_buffer_size) {
        out_buffer[buffer_pos] = '\0';
    } else if (out_buffer_size > 0) {
        out_buffer[out_buffer_size - 1] = '\0'; // Ensure null termination if truncated exactly at end
        success = false; // No space for null terminator if buffer was filled exactly
    }

    out_length = buffer_pos;
    return success;
}


const Line* EditorBuffer::get_line(kstd::size_t line_num) const {
    if (line_num < num_lines_in_use) {
        return &lines[line_num];
    }
    return nullptr;
}

Line* EditorBuffer::get_line_mut(kstd::size_t line_num) {
    if (line_num < num_lines_in_use) {
        return &lines[line_num];
    }
    // If trying to access/create a line just beyond current num_lines_in_use (up to MAX_BUFFER_LINES)
    if (line_num < MAX_BUFFER_LINES && line_num == num_lines_in_use) {
        num_lines_in_use++; // Effectively "create" this new empty line
        lines[line_num].clear();
        return &lines[line_num];
    }
    return nullptr;
}


bool EditorBuffer::insert_new_line_at(kstd::size_t line_num) {
    if (num_lines_in_use >= MAX_BUFFER_LINES) return false; // Buffer full
    if (line_num > num_lines_in_use) line_num = num_lines_in_use; // Clamp to insert at the end

    // Shift lines down
    for (kstd::size_t i = num_lines_in_use; i > line_num; --i) {
        // This requires Line to be copyable/movable. Default member-wise copy is fine.
        lines[i] = lines[i - 1];
    }

    // Clear the new line
    lines[line_num].clear();
    num_lines_in_use++;
    return true;
}

bool EditorBuffer::delete_line_at(kstd::size_t line_num) {
    if (num_lines_in_use <= 1) return false; // Cannot delete the last line, just clear it
    if (line_num >= num_lines_in_use) return false; // Invalid line number

    // Shift lines up
    for (kstd::size_t i = line_num; i < num_lines_in_use - 1; ++i) {
        lines[i] = lines[i + 1];
    }

    // Clear the last (now unused) line
    lines[num_lines_in_use - 1].clear();
    num_lines_in_use--;
    return true;
}


} // namespace Editor
} // namespace Kernel
