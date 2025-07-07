#include "editor.h"
#include <kernel/console.h>
#include <kernel/filesystem/filesystem.h>
#include <kernel/filesystem/file.h>
#include <lib/printf/printf.h> // For Kernel::kprintf
#include <kstd/cstring.h>   // For kstrncpy, kstrlen, kmemset
#include <kstd/algorithm.h> // For kstd::min, kstd::max

// Define KEY_XXX codes if we were getting them from console.get_keycode()
// For now, Console::get_char() returns plain chars. Special keys need mapping.
// Example placeholder key codes (these would depend on console input layer)
constexpr char KEY_ETX      = 0x03; // Ctrl+C (example for exit)
constexpr char KEY_ESC      = 0x1B; // Escape key
constexpr char KEY_F1       = 0x80; // Placeholder for F1 (e.g., Help)
constexpr char KEY_F2       = 0x81; // Placeholder for F2 (e.g., Save)
constexpr char KEY_F10      = 0x89; // Placeholder for F10 (e.g., Exit)
constexpr char KEY_UP_ARROW = 0x90; // Placeholder
constexpr char KEY_DOWN_ARROW=0x91; // Placeholder
constexpr char KEY_LEFT_ARROW=0x92; // Placeholder
constexpr char KEY_RIGHT_ARROW=0x93;// Placeholder
constexpr char KEY_BACKSPACE = '\b';// Or 0x08 / 0x7F depending on terminal
constexpr char KEY_DELETE    = 0x7F;// Often DEL key code (or map another char)
constexpr char KEY_ENTER     = '\n';// Or '\r'
constexpr char KEY_TAB       = '\t';


namespace Kernel {

Editor::Editor(Console& console, Filesystem& fs)
    : term_console(console),
      filesystem_instance(fs),
      // text_buffer is default constructed
      is_dirty(false),
      cursor_line(0),
      cursor_col(0),
      top_visible_line(0),
      left_visible_col(0) {
    current_filename[0] = '\0';
}

void Editor::open_and_run(const char* filename) {
    if (!filename || filename[0] == '\0') {
        kstd::kstrncpy(current_filename, "untitled.txt", FS::MAX_FILENAME_LENGTH -1);
    } else {
        kstd::kstrncpy(current_filename, filename, FS::MAX_FILENAME_LENGTH -1);
    }
    current_filename[FS::MAX_FILENAME_LENGTH -1] = '\0';

    is_dirty = false;
    cursor_line = 0;
    cursor_col = 0;
    top_visible_line = 0;
    left_visible_col = 0;
    text_buffer.clear_all(); // Start with a clean buffer

    if (filesystem_instance.file_exists(current_filename)) {
        if (!load_file(current_filename)) {
            // Error loading, console message already printed by load_file
            // Start with an empty buffer for this filename.
            term_console.println("Warning: Could not load file. Starting with empty buffer.");
            // text_buffer is already cleared, ensure one empty line
            if (text_buffer.get_num_lines() == 0) text_buffer.insert_new_line_at(0);
        }
    } else {
        // File doesn't exist, buffer is already empty with one line.
        term_console.print("New file: "); term_console.println(current_filename);
        is_dirty = true; // New file is considered "dirty" until first save
    }

    // term_console.set_raw_mode(true); // If console supports raw input mode
    editor_main_loop();
    // term_console.set_raw_mode(false); // Restore console mode
    term_console.println(""); // Newline after editor exits
}


void Editor::editor_main_loop() {
    bool running = true;
    redraw_screen();

    while (running) {
        update_cursor_on_console();
        char key = term_console.get_char(); // Blocking read

        // Simple key processing for now.
        // A more advanced system would use key codes for non-ASCII keys.
        switch (key) {
            // --- Control/Function Keys (Placeholders) ---
            // These depend on how Console::get_char() handles special keys.
            // Assuming for now it might return special codes or escape sequences.
            // For this basic version, we'll map some common Ctrl chars or use F-key placeholders.

            case KEY_ETX: // Ctrl+C as an example for "force exit" or "exit without save"
            case KEY_F10: // F10 for Exit
                if (is_dirty) {
                    // TODO: Prompt "Save changes? (Y/N/Cancel)"
                    term_console.println("\nWarning: Unsaved changes. Exit anyway? (y/N - currently exits)");
                    // For now, just exit. A real editor would prompt.
                    // char confirm = term_console.get_char();
                    // if (confirm == 'y' || confirm == 'Y') running = false; else redraw_screen();
                    running = false; // Simplified: exit directly
                } else {
                    running = false;
                }
                break;

            // case KEY_F1: // Help (TODO)
            //    show_help_screen(); redraw_screen(); break;
            case KEY_F2: // Save
                save_file();
                redraw_screen(); // Redraw status bar for save message
                break;

            // --- Arrow Key Placeholders ---
            // These would need Console to map arrow key escape sequences to these defines.
            // For now, these cases won't be hit with simple term_console.get_char().
            case KEY_UP_ARROW:    move_cursor_up();    break;
            case KEY_DOWN_ARROW:  move_cursor_down();  break;
            case KEY_LEFT_ARROW:  move_cursor_left();  break;
            case KEY_RIGHT_ARROW: move_cursor_right(); break;

            // --- Standard Editing Keys ---
            case KEY_BACKSPACE: // Typically '\b' or 0x08
            case 0x7F:          // Often DEL key or another backspace variant
                handle_backspace();
                break;

            // case KEY_DELETE: // If we have a separate delete key
            //    handle_delete();
            //    break;

            case KEY_ENTER:     // '\n' or '\r'
            case '\r':          // Handle CR explicitly if get_char might return it
                handle_enter();
                break;

            case KEY_TAB:
                handle_tab();
                break;

            default:
                if (key >= ' ' && key <= '~') { // Printable ASCII characters
                    handle_char_insert(key);
                } else {
                    // Kernel::kprintf("Unhandled key: 0x%x\n", (unsigned int)key); // Debug
                }
                break;
        }
        if (running) { // Don't redraw if we are exiting
             scroll_if_needed(); // Adjust viewport based on cursor
             redraw_screen();    // Redraw after any change
        }
    }
}


void Editor::process_key_press(char key) {
    // This function would be more elaborate if handling complex key codes / states.
    // For now, the switch in editor_main_loop handles it.
    (void)key;
}


// --- Drawing and UI ---
void Editor::redraw_screen() {
    // For a simple non-full-screen console editor:
    // 1. Clear the part of the console used by the editor (e.g., print newlines or use clear codes)
    // 2. Draw status bar
    // 3. Draw text area
    // This is a simplified version. A real terminal editor uses cursor addressing (ANSI escape codes).
    // Our Console class doesn't have clear_screen or cursor_goto yet.
    // So, we'll just redraw everything by reprinting.

    // Simulate clear by printing many newlines (very basic)
    for(int i=0; i < 5; ++i) term_console.put_char('\n'); // Push old content up
    Kernel::kprintf("--- KEKOS Editor --- File: %s %s---\n", current_filename, is_dirty ? "[Modified]" : "");

    draw_text_area();
    draw_status_bar();
    // update_cursor_on_console() will be called by main loop after this.
}

void Editor::draw_status_bar() {
    // Prints a status line at the bottom of the editor area
    // Example: Filename | Line X, Col Y | Dirty status | Help keys
    Kernel::kprintf("--------------------------------------------------------------------------------\n");
    Kernel::kprintf("L%u, C%u %s | F2:Save F10:Exit (Ctrl+C also exits for now)\n",
        (unsigned int)cursor_line + 1,
        (unsigned int)cursor_col + 1,
        is_dirty ? "*" : " "
    );
    Kernel::kprintf("--------------------------------------------------------------------------------\n");
}

void Editor::draw_text_area() {
    for (kstd::size_t screen_line = 0; screen_line < EDITOR_VIEW_LINES; ++screen_line) {
        kstd::size_t buffer_line_idx = top_visible_line + screen_line;
        const Line* line = text_buffer.get_line(buffer_line_idx);

        if (line) {
            // Print line content, respecting left_visible_col (basic horizontal scroll)
            kstd::size_t len_to_print = line->current_length;
            const char* text_to_print = line->text;

            if (left_visible_col < len_to_print) {
                text_to_print += left_visible_col;
                len_to_print -= left_visible_col;
            } else {
                len_to_print = 0; // Scrolled past all text on this line
            }

            len_to_print = kstd::min(len_to_print, EDITOR_VIEW_COLS);

            char display_buf[EDITOR_VIEW_COLS + 1];
            kstd::kstrncpy(display_buf, text_to_print, len_to_print);
            display_buf[len_to_print] = '\0';
            term_console.println(display_buf); // println adds its own newline
        } else {
            // Line does not exist in buffer (e.g., past EOF)
            term_console.println("~"); // Tilde like vi for empty lines past EOF
        }
    }
}

void Editor::update_cursor_on_console() {
    // This is where we would use Console::set_cursor_pos(x, y)
    // Since our Console is simple, we can't directly position the cursor.
    // The prompt-based input of Console::read_line handles cursor itself.
    // For a full-screen editor, this function is critical.
    // For now, it does nothing, relying on the fact that redraw_screen prints everything
    // and the user types at the "bottom" after the last print.
    // This is a major simplification due to current Console limitations.
    // A real editor would clear screen, then print text, then position cursor.
}


// --- File Operations ---
bool Editor::load_file(const char* filename_to_load) {
    term_console.print("Loading file: "); term_console.println(filename_to_load);
    FS::File* file_obj = nullptr;
    FS::ErrorCode res = filesystem_instance.open_file(filename_to_load, FS::OpenMode::READ, file_obj);

    if (res != FS::ErrorCode::OK || !file_obj) {
        Kernel::kprintf("Error: Cannot open '%s' for reading (code %d).\n", filename_to_load, static_cast<int>(res));
        if(file_obj) delete file_obj;
        return false;
    }

    kstd::size_t file_size = file_obj->get_size();
    if (file_size == 0) {
        text_buffer.clear_all(); // Ensure one empty line for empty file
        if (text_buffer.get_num_lines() == 0) text_buffer.insert_new_line_at(0);
        delete file_obj;
        term_console.println("File is empty or new.");
        return true;
    }

    // Max file size check for editor buffer (rough estimate, could be more precise)
    // Content size can be larger than buffer line capacity due to many short lines.
    if (file_size > EditorBuffer::get_max_lines() * EditorBuffer::get_max_line_length()) {
         Kernel::kprintf("Error: File '%s' is too large for editor buffer.\n", filename_to_load);
         delete file_obj;
         return false;
    }

    // Allocate a temporary buffer to read the whole file content
    // This is not ideal for very large files but okay for our MAX_FILE_SIZE_BYTES limit.
    char* temp_content_buffer = new char[file_size + 1]; // +1 for null terminator
    if (!temp_content_buffer) {
        Kernel::kprintf("Error: Not enough memory to load file '%s'.\n", filename_to_load);
        delete file_obj;
        return false;
    }

    kstd::size_t bytes_read_total = 0;
    res = file_obj->read(temp_content_buffer, file_size, bytes_read_total);
    if (res != FS::ErrorCode::OK || bytes_read_total != file_size) {
        Kernel::kprintf("Error reading content of '%s'. Expected %u, got %u bytes.\n",
            filename_to_load, (unsigned int)file_size, (unsigned int)bytes_read_total);
        delete[] temp_content_buffer;
        delete file_obj;
        return false;
    }
    temp_content_buffer[bytes_read_total] = '\0';

    text_buffer.load_content(temp_content_buffer, bytes_read_total);

    delete[] temp_content_buffer;
    delete file_obj;
    is_dirty = false; // Freshly loaded
    term_console.println("File loaded successfully.");
    return true;
}

bool Editor::save_file() {
    term_console.print("Saving file: "); term_console.println(current_filename);

    kstd::size_t content_len;
    // Estimate required buffer size: num_lines * (avg_line_len + 1 for newline)
    // Max possible: MAX_BUFFER_LINES * (MAX_LINE_LENGTH + 1)
    constexpr kstd::size_t max_serial_size = EditorBuffer::get_max_lines() * (EditorBuffer::get_max_line_length() + 1) + 1;
    char* temp_content_buffer = new char[max_serial_size];
    if (!temp_content_buffer) {
        Kernel::kprintf("Error: Not enough memory to serialize file content for saving.\n");
        return false;
    }

    if (!text_buffer.get_content_as_string(temp_content_buffer, max_serial_size, content_len)) {
        // This might mean content was truncated, but get_content_as_string should try its best.
        Kernel::kprintf("Warning: Content might have been truncated during serialization for save.\n");
    }

    // Now write this temp_content_buffer to the filesystem.
    // Filesystem::open_file with WRITE mode handles truncation of existing file.
    FS::File* file_obj = nullptr;
    FS::ErrorCode res = filesystem_instance.open_file(current_filename, FS::OpenMode::WRITE, file_obj);
    if (res != FS::ErrorCode::OK || !file_obj) {
        Kernel::kprintf("Error: Cannot open '%s' for writing (code %d).\n", current_filename, static_cast<int>(res));
        delete[] temp_content_buffer;
        if (file_obj) delete file_obj;
        return false;
    }

    kstd::size_t bytes_written = 0;
    res = file_obj->write(temp_content_buffer, content_len, bytes_written);
    delete[] temp_content_buffer; // Free temp buffer regardless of write outcome

    if (res != FS::ErrorCode::OK || bytes_written != content_len) {
        Kernel::kprintf("Error writing content to '%s'. Expected to write %u, wrote %u.\n",
            current_filename, (unsigned int)content_len, (unsigned int)bytes_written);
        delete file_obj;
        return false;
    }

    delete file_obj; // Closes the file
    is_dirty = false;
    term_console.println("File saved successfully.");
    return true;
}

// --- Cursor and Viewport Management (Simplified stubs) ---
void Editor::scroll_if_needed() {
    // Scroll vertically
    if (cursor_line < top_visible_line) {
        top_visible_line = cursor_line;
    } else if (cursor_line >= top_visible_line + EDITOR_VIEW_LINES) {
        top_visible_line = cursor_line - EDITOR_VIEW_LINES + 1;
    }
    // Scroll horizontally (very basic)
    if (cursor_col < left_visible_col) {
        left_visible_col = cursor_col;
    } else if (cursor_col >= left_visible_col + EDITOR_VIEW_COLS) {
        left_visible_col = cursor_col - EDITOR_VIEW_COLS + 1;
    }
}

Editor::Line* Editor::get_current_buffer_line_mut() {
    return text_buffer.get_line_mut(cursor_line);
}
const Editor::Line* Editor::get_current_buffer_line() const {
    return text_buffer.get_line(cursor_line);
}


void Editor::move_cursor_up() {
    if (cursor_line > 0) {
        cursor_line--;
        // Adjust cursor_col to be within the new line's length or preferred column
        const Line* current_line = get_current_buffer_line();
        if (current_line && cursor_col > current_line->current_length) {
            cursor_col = current_line->current_length;
        }
    }
}
void Editor::move_cursor_down() {
    if (cursor_line < text_buffer.get_num_lines() - 1) {
        cursor_line++;
        const Line* current_line = get_current_buffer_line();
        if (current_line && cursor_col > current_line->current_length) {
            cursor_col = current_line->current_length;
        }
    }
}
void Editor::move_cursor_left() {
    if (cursor_col > 0) {
        cursor_col--;
    } else if (cursor_line > 0) { // Move to end of previous line
        cursor_line--;
        const Line* prev_line = get_current_buffer_line();
        cursor_col = prev_line ? prev_line->current_length : 0;
    }
}
void Editor::move_cursor_right() {
    const Line* current_line = get_current_buffer_line();
    if (current_line && cursor_col < current_line->current_length) {
        cursor_col++;
    } else if (current_line && cursor_col == current_line->current_length &&
               cursor_line < text_buffer.get_num_lines() - 1) { // Move to start of next line
        cursor_line++;
        cursor_col = 0;
    }
}

// --- Editing Handlers (Simplified) ---
void Editor::handle_char_insert(char c) {
    Line* line = get_current_buffer_line_mut();
    if (line) {
        if (line->insert_char(cursor_col, c)) {
            cursor_col++;
            is_dirty = true;
        } else {
            // Line full, beep or status message (TODO)
        }
    }
}
void Editor::handle_backspace() {
    if (cursor_col > 0) {
        Line* line = get_current_buffer_line_mut();
        if (line && line->delete_char(cursor_col - 1)) {
            cursor_col--;
            is_dirty = true;
        }
    } else if (cursor_line > 0) { // At start of line, merge with previous
        Line* prev_line = text_buffer.get_line_mut(cursor_line - 1);
        Line* current_line = get_current_buffer_line_mut();
        if (prev_line && current_line) {
            cursor_col = prev_line->current_length; // Move cursor to end of prev line
            // Append current line's text to previous line
            if (prev_line->current_length + current_line->current_length <= EditorBuffer::get_max_line_length()) {
                kstd::kstrncpy(prev_line->text + prev_line->current_length, current_line->text, current_line->current_length);
                prev_line->current_length += current_line->current_length;
                prev_line->text[prev_line->current_length] = '\0'; // Ensure null term

                text_buffer.delete_line_at(cursor_line); // Delete the now-empty current line
                cursor_line--; // Cursor is now on the merged line (previous line)
                is_dirty = true;
            } else {
                // Cannot merge, lines too long (TODO: beep/status)
            }
        }
    }
}
void Editor::handle_delete() { // Deletes char at cursor_col
    Line* line = get_current_buffer_line_mut();
    if (line && cursor_col < line->current_length) {
        if (line->delete_char(cursor_col)) {
            is_dirty = true;
        }
    } else if (line && cursor_col == line->current_length && cursor_line < text_buffer.get_num_lines() -1) {
        // At end of line, merge with next line (similar to backspace at start of line)
        Line* next_line = text_buffer.get_line_mut(cursor_line + 1);
        if (next_line) {
             if (line->current_length + next_line->current_length <= EditorBuffer::get_max_line_length()) {
                kstd::kstrncpy(line->text + line->current_length, next_line->text, next_line->current_length);
                line->current_length += next_line->current_length;
                line->text[line->current_length] = '\0';
                text_buffer.delete_line_at(cursor_line + 1);
                is_dirty = true;
            }
        }
    }
}

void Editor::handle_enter() {
    Line* current_line_ptr = get_current_buffer_line_mut();
    if (!current_line_ptr) return;

    if (text_buffer.insert_new_line_at(cursor_line + 1)) {
        Line* new_line_ptr = text_buffer.get_line_mut(cursor_line + 1);
        if (new_line_ptr) {
            // If cursor was not at the end of the line, move rest of the line to new line
            if (cursor_col < current_line_ptr->current_length) {
                kstd::size_t tail_len = current_line_ptr->current_length - cursor_col;
                kstd::kstrncpy(new_line_ptr->text, current_line_ptr->text + cursor_col, tail_len);
                new_line_ptr->current_length = tail_len;
                new_line_ptr->text[tail_len] = '\0';

                current_line_ptr->text[cursor_col] = '\0';
                current_line_ptr->current_length = cursor_col;
            }
        }
        cursor_line++;
        cursor_col = 0;
        is_dirty = true;
    } else {
        // Buffer full (TODO: beep/status)
    }
}

void Editor::handle_tab() {
    // Insert multiple spaces for a tab (e.g., 4 spaces)
    // Or, could implement proper tab stops.
    constexpr int TAB_WIDTH = 4;
    int spaces_to_insert = TAB_WIDTH - (cursor_col % TAB_WIDTH);
    for (int i = 0; i < spaces_to_insert; ++i) {
        handle_char_insert(' ');
    }
}


// The Makefile needs to be updated to include editor.cpp and buffer.cpp
// $(KERNEL_EDIT_DIR)/editor.cpp \
// $(KERNEL_EDIT_DIR)/buffer.cpp
// These are already in the Makefile from Step 1.

// The Shell's 'edit' command needs to be updated to instantiate and run this Editor.
// This will involve modifying `kernel/shell/commands.cpp`'s `handle_edit` and
// `kernel/shell/shell.cpp`'s `get_editor()` and potentially `Shell::run` if editor takes over loop.
// This will be done in the next step.

} // namespace Kernel
