//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Defines the LogoStream interface for abstract I/O sources and sinks.
//  Streams can represent files, serial ports, or console I/O.
//

#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Maximum length of a stream name (pathname or device name)
    #define LOGO_STREAM_NAME_MAX 64

    // Special return values for read operations
    #define LOGO_STREAM_EOF        (-1)   // End of file or error
    #define LOGO_STREAM_INTERRUPTED (-2)  // User pressed BRK key

    typedef enum LogoStreamType
    {
        LOGO_STREAM_CONSOLE_INPUT,  // Keyboard or serial input
        LOGO_STREAM_CONSOLE_OUTPUT, // Screen or serial output
        LOGO_STREAM_FILE,           // Disk file (seekable)
        LOGO_STREAM_SERIAL,         // Serial port (future)
    } LogoStreamType;

    typedef struct LogoStream LogoStream;

    typedef struct LogoStreamOps
    {
        // Reading operations (NULL if write-only)
        // Returns character read, or -1 on EOF/error
        int (*read_char)(LogoStream *stream);
        // Returns count of characters read, or -1 on error
        int (*read_chars)(LogoStream *stream, char *buffer, int count);
        // Returns length of line read (excluding newline), or -1 on EOF/error
        int (*read_line)(LogoStream *stream, char *buffer, size_t size);
        // Returns true if data is available without blocking (keyp)
        bool (*can_read)(LogoStream *stream);

        // Writing operations (NULL if read-only)
        void (*write)(LogoStream *stream, const char *text);
        void (*flush)(LogoStream *stream);

        // Position operations (NULL if not seekable, e.g., keyboard/screen/serial)
        long (*get_read_pos)(LogoStream *stream);
        bool (*set_read_pos)(LogoStream *stream, long pos);
        long (*get_write_pos)(LogoStream *stream);
        bool (*set_write_pos)(LogoStream *stream, long pos);
        long (*get_length)(LogoStream *stream);

        // Lifecycle
        void (*close)(LogoStream *stream);
    } LogoStreamOps;

    struct LogoStream
    {
        LogoStreamType type;
        const LogoStreamOps *ops;
        void *context;                      // Implementation-specific data
        char name[LOGO_STREAM_NAME_MAX];    // Pathname for files, device name for console
        bool is_open;
        bool write_error;                   // Set if a write operation failed (partial write)
    };

    //
    // Stream operations - delegate to the ops vtable
    // These check for NULL ops and return appropriate defaults
    //

    // Reading operations
    int logo_stream_read_char(LogoStream *stream);
    int logo_stream_read_chars(LogoStream *stream, char *buffer, int count);
    int logo_stream_read_line(LogoStream *stream, char *buffer, size_t size);
    bool logo_stream_can_read(LogoStream *stream);

    // Writing operations
    void logo_stream_write(LogoStream *stream, const char *text);
    void logo_stream_write_line(LogoStream *stream, const char *text);
    void logo_stream_flush(LogoStream *stream);

    // Write error checking
    bool logo_stream_has_write_error(LogoStream *stream);
    void logo_stream_clear_write_error(LogoStream *stream);

    // Position operations (return -1 or false if not seekable)
    long logo_stream_get_read_pos(LogoStream *stream);
    bool logo_stream_set_read_pos(LogoStream *stream, long pos);
    long logo_stream_get_write_pos(LogoStream *stream);
    bool logo_stream_set_write_pos(LogoStream *stream, long pos);
    long logo_stream_get_length(LogoStream *stream);

    // Lifecycle
    void logo_stream_close(LogoStream *stream);

    //
    // Utility functions for stream implementations
    //

    // Initialize a stream with the given type, ops, and name
    void logo_stream_init(LogoStream *stream, LogoStreamType type,
                          const LogoStreamOps *ops, void *context, const char *name);

#ifdef __cplusplus
}
#endif
