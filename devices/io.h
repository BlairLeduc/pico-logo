//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Manages I/O state: current reader, writer, open files, and dribble.
//  This provides the Logo-level abstractions like SETREAD, SETWRITE, etc.
//

#pragma once

#include "devices/stream.h"
#include "devices/console.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Maximum number of simultaneously open files
    #define LOGO_MAX_OPEN_FILES 6

    // File prefix maximum length
    #define LOGO_PREFIX_MAX 64

    //
    // File opening modes (used by file opener callback)
    //
    typedef enum LogoFileMode
    {
        LOGO_FILE_READ,     // Open for reading (file must exist)
        LOGO_FILE_WRITE,    // Open for writing (creates/truncates)
        LOGO_FILE_APPEND,   // Open for appending (creates if needed)
        LOGO_FILE_UPDATE,   // Open for reading and writing (file must exist)
    } LogoFileMode;

    //
    // File opener callback type
    // Platform-specific implementation should be provided to logo_io_init()
    //
    typedef LogoStream *(*LogoFileOpener)(const char *pathname, LogoFileMode mode);

    //
    // LogoIO manages the I/O state for the Logo interpreter
    //
    typedef struct LogoIO
    {
        // The physical console (always available, may be serial-only)
        LogoConsole *console;

        // Current input source (defaults to &console->input)
        LogoStream *reader;

        // Current output destination (defaults to &console->output)
        LogoStream *writer;

        // Dribble stream (NULL if not dribbling)
        // When active, output goes to both writer AND dribble
        LogoStream *dribble;

        // Open file/device streams
        LogoStream *open_streams[LOGO_MAX_OPEN_FILES];
        int open_count;

        // Current file prefix (for relative pathnames)
        char prefix[LOGO_PREFIX_MAX];

        // Platform-specific file opener (NULL if file I/O not supported)
        LogoFileOpener file_opener;
    } LogoIO;

    //
    // Lifecycle
    //

    // Initialize I/O with a console (keyboard/screen or serial-only)
    // file_opener can be NULL if file I/O is not needed
    void logo_io_init(LogoIO *io, LogoConsole *console);

    // Set the file opener callback (for platforms that support file I/O)
    void logo_io_set_file_opener(LogoIO *io, LogoFileOpener opener);

    // Clean up all open files and reset state
    void logo_io_cleanup(LogoIO *io);

    //
    // File prefix management
    //

    // Set the file prefix (used for relative pathnames)
    void logo_io_set_prefix(LogoIO *io, const char *prefix);

    // Get the current prefix (returns "" if none)
    const char *logo_io_get_prefix(const LogoIO *io);

    // Resolve a pathname with the current prefix
    // Writes full path to buffer, returns buffer on success or NULL if too long
    char *logo_io_resolve_path(const LogoIO *io, const char *pathname,
                               char *buffer, size_t buffer_size);

    //
    // File/device management
    //

    // Open a file for read/write (Logo "open" primitive)
    // Creates file if it doesn't exist
    LogoStream *logo_io_open(LogoIO *io, const char *pathname);

    // Open a file for reading only (Logo "openread" primitive)
    LogoStream *logo_io_open_read(LogoIO *io, const char *pathname);

    // Open a file for writing (Logo "openwrite" primitive)
    // Creates/truncates the file
    LogoStream *logo_io_open_write(LogoIO *io, const char *pathname);

    // Open a file for appending (Logo "openappend" primitive)
    LogoStream *logo_io_open_append(LogoIO *io, const char *pathname);

    // Open a file for update/read+write (Logo "openupdate" primitive)
    LogoStream *logo_io_open_update(LogoIO *io, const char *pathname);

    // Close a specific file by name
    void logo_io_close(LogoIO *io, const char *pathname);

    // Close all open files (not dribble)
    void logo_io_close_all(LogoIO *io);

    // Find an open stream by name, returns NULL if not found
    LogoStream *logo_io_find_open(LogoIO *io, const char *pathname);

    // Check if a file is currently open
    bool logo_io_is_open(const LogoIO *io, const char *pathname);

    // Get count of open files
    int logo_io_open_count(const LogoIO *io);

    // Get open file by index (for allopen primitive)
    LogoStream *logo_io_get_open(const LogoIO *io, int index);

    //
    // Reader/writer control
    //

    // Set the current reader (NULL or console input = keyboard)
    void logo_io_set_reader(LogoIO *io, LogoStream *stream);

    // Set the current writer (NULL or console output = screen)
    void logo_io_set_writer(LogoIO *io, LogoStream *stream);

    // Get reader name (returns empty string for keyboard)
    const char *logo_io_get_reader_name(const LogoIO *io);

    // Get writer name (returns empty string for screen)
    const char *logo_io_get_writer_name(const LogoIO *io);

    // Check if reader is the keyboard
    bool logo_io_reader_is_keyboard(const LogoIO *io);

    // Check if writer is the screen
    bool logo_io_writer_is_screen(const LogoIO *io);

    //
    // Dribble control
    //

    // Start dribbling to a file (opens the file)
    bool logo_io_start_dribble(LogoIO *io, const char *pathname);

    // Stop dribbling (closes the dribble file)
    void logo_io_stop_dribble(LogoIO *io);

    // Check if dribbling is active
    bool logo_io_is_dribbling(const LogoIO *io);

    //
    // High-level I/O operations (use current reader/writer)
    // These handle dribbling automatically for output operations.
    //

    // Read a single character from current reader
    // Returns character or -1 on EOF/error
    int logo_io_read_char(LogoIO *io);

    // Read multiple characters from current reader
    // Returns count read or -1 on error
    int logo_io_read_chars(LogoIO *io, char *buffer, int count);

    // Read a line from current reader (for readlist/readword)
    // Returns length of line or -1 on EOF/error
    int logo_io_read_line(LogoIO *io, char *buffer, size_t size);

    // Check if input is available without blocking (keyp)
    bool logo_io_key_available(LogoIO *io);

    // Write text to current writer (and dribble if active)
    void logo_io_write(LogoIO *io, const char *text);

    // Write text followed by newline
    void logo_io_write_line(LogoIO *io, const char *text);

    // Flush output
    void logo_io_flush(LogoIO *io);

    //
    // Direct console access (for primitives that always use screen/keyboard)
    // Use these for po, pots, etc. which always print to screen
    //

    // Write directly to console output (ignores setwrite)
    void logo_io_console_write(LogoIO *io, const char *text);
    void logo_io_console_write_line(LogoIO *io, const char *text);

#ifdef __cplusplus
}
#endif
