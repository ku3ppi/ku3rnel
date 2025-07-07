#ifndef KERNEL_EDITOR_EDITOR_H
#define KERNEL_EDITOR_EDITOR_H

#include "buffer.h"       // For EditorBuffer
#include <kernel/filesystem/filesystem.h> // For Filesystem reference
#include <kernel/filesystem/file.h>       // For FS::File (to read/write)
#include <kernel/console.h> // For Console reference
#include <kstd/cstddef.h>   // For kstd::size_t
#include <kstd/cstdint.h>   // For kstd::uintXX_t

namespace Kernel {

class Editor {
public:
    Editor(Console& console, Filesystem& fs);
    ~Editor() = default;

    // Opens a file (or creates if not exists) and runs the editor's main loop.
    // Returns when the editor is exited.
    void open_and_run(const char* filename);

private:
    Console& term_console;
    Filesystem& filesystem_instance;
    EditorBuffer text_buffer;

    // Editor state
    char current_filename[FS::MAX_FILENAME_LENGTH];
    bool is_dirty; // True if buffer has been modified since last save

    // Cursor position within the buffer (logical line and column)
    kstd::size_t cursor_line; // 0-indexed line in text_buffer
    kstd::size_t cursor_col;  // 0-indexed column in current_line.text

    // Viewport state (what part of the buffer is visible on screen)
    kstd::size_t top_visible_line;    // First line of the buffer shown at the top of the screen
    kstd::size_t left_visible_col;    // First column of the buffer shown at the left (for horizontal scroll - basic for now)

    // Screen dimensions (e.g., from console, or fixed)
    // For simplicity, let's assume a fixed screen size for the editor drawing area.
    // The console itself might be 80x25, editor uses a portion.
    static constexpr kstd::size_t EDITOR_VIEW_LINES = 20; // Number of lines for text editing area
    static constexpr kstd::size_t EDITOR_VIEW_COLS  = 78; // Number of columns for text editing area (leave space for borders/status)


    // Core editor loop and input handling
    void editor_main_loop();
    void process_key_press(char key); // Later, this might take key codes for arrows, F-keys etc.

    // Drawing and UI
    void redraw_screen();
    void draw_status_bar();
    void draw_text_area();
    void update_cursor_on_console(); // Positions the console's actual cursor

    // File operations
    bool load_file(const char* filename);
    bool save_file();

    // Cursor and viewport management
    void scroll_if_needed();
    void move_cursor_up();
    void move_cursor_down();
    void move_cursor_left();
    void move_cursor_right();
    void handle_char_insert(char c);
    void handle_backspace();
    void handle_delete(); // If we support a DEL key
    void handle_enter();
    void handle_tab();

    // Helper to get current line from buffer, handling potential null
    Editor::Line* get_current_buffer_line_mut();
    const Editor::Line* get_current_buffer_line() const;


    // Prevent copying
    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;
};

} // namespace Kernel

#endif // KERNEL_EDITOR_EDITOR_H
