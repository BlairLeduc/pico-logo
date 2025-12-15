//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Manages I/O state: current reader, writer, open files, and dribble.
//  This provides the Logo-level abstractions like SETREAD, SETWRITE, etc.
//

#pragma once

#include "devices/hardware.h"
#include "devices/storage.h"
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
    // LogoIO manages the I/O state for the Logo interpreter
    //
    typedef struct LogoIO
    {
        // The physical console (always available, may be serial-only)
        LogoConsole *console;

        // The physical storage (may be NULL if no file I/O support)
        LogoStorage *storage;

        // The physical hardware (always available)
        LogoHardware *hardware;

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
    } LogoIO;

    //
    // Lifecycle
    //

    // Initialize I/O with a console (keyboard/screen or serial-only)
    // file_opener can be NULL if file I/O is not needed
    void logo_io_init(LogoIO *io, LogoConsole *console, LogoStorage *storage, LogoHardware *hardware);

    // Clean up all open files and reset state
    void logo_io_cleanup(LogoIO *io);

    //
    // Device specific functions
    //

    // Sleep for specified milliseconds
    void logo_io_usleep(LogoIO *io, int milliseconds);
    
    // Get a random 32-bit number from the device
    uint32_t logo_io_random(LogoIO *io);

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

    // Check if a file exists
    bool logo_io_file_exists(const LogoIO *io, const char *pathname);

    // Check if a directory exists
    bool logo_io_dir_exists(const LogoIO *io, const char *pathname);
    
    // Delete a file
    bool logo_io_file_delete(const LogoIO *io, const char *pathname);
    
    // Create a new empty directory
    bool logo_io_dir_create(const LogoIO *io, const char *pathname);

    // Delete a directory
    bool logo_io_dir_delete(const LogoIO *io, const char *pathname);
    
    // Rename a file or directory
    bool logo_io_rename(const LogoIO *io, const char *old_path, const char *new_path);
    
    // Get file size, returns -1 on error
    long logo_io_file_size(const LogoIO *io, const char *pathname);
    
    bool logo_io_list_directory(const LogoIO *io, const char *pathname,
                                LogoDirCallback callback, void *user_data,
                                const char *filter);
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
