//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements the LogoIO I/O state manager.
//

#include "devices/io.h"

#include <string.h>
#include <stdlib.h>

//
// Lifecycle
//

void logo_io_init(LogoIO *io, LogoConsole *console)
{
    if (!io)
    {
        return;
    }

    io->console = console;
    
    // Default reader is keyboard, writer is screen
    io->reader = console ? &console->input : NULL;
    io->writer = console ? &console->output : NULL;
    
    io->dribble = NULL;
    io->open_count = 0;
    io->prefix[0] = '\0';
    io->file_opener = NULL;

    for (int i = 0; i < LOGO_MAX_OPEN_FILES; i++)
    {
        io->open_streams[i] = NULL;
    }
}

void logo_io_set_file_opener(LogoIO *io, LogoFileOpener opener)
{
    if (io)
    {
        io->file_opener = opener;
    }
}

void logo_io_cleanup(LogoIO *io)
{
    if (!io)
    {
        return;
    }

    // Close all open files
    logo_io_close_all(io);

    // Stop dribbling
    logo_io_stop_dribble(io);

    // Reset to defaults
    io->reader = io->console ? &io->console->input : NULL;
    io->writer = io->console ? &io->console->output : NULL;
}

//
// File prefix management
//

void logo_io_set_prefix(LogoIO *io, const char *prefix)
{
    if (!io)
    {
        return;
    }

    if (!prefix || prefix[0] == '\0')
    {
        io->prefix[0] = '\0';
    }
    else
    {
        strncpy(io->prefix, prefix, LOGO_PREFIX_MAX - 1);
        io->prefix[LOGO_PREFIX_MAX - 1] = '\0';
    }
}

const char *logo_io_get_prefix(const LogoIO *io)
{
    if (!io)
    {
        return "";
    }
    return io->prefix;
}

char *logo_io_resolve_path(const LogoIO *io, const char *pathname,
                           char *buffer, size_t buffer_size)
{
    if (!io || !pathname || !buffer || buffer_size == 0)
    {
        return NULL;
    }

    // If pathname is absolute or prefix is empty, use pathname as-is
    if (pathname[0] == '/' || pathname[0] == '\\' || io->prefix[0] == '\0')
    {
        size_t len = strlen(pathname);
        if (len >= buffer_size)
        {
            return NULL;
        }
        strcpy(buffer, pathname);
        return buffer;
    }

    // Combine prefix and pathname
    size_t prefix_len = strlen(io->prefix);
    size_t path_len = strlen(pathname);
    
    // Check if we need a separator
    bool need_sep = (io->prefix[prefix_len - 1] != '/' && 
                     io->prefix[prefix_len - 1] != '\\');
    
    size_t total = prefix_len + (need_sep ? 1 : 0) + path_len;
    if (total >= buffer_size)
    {
        return NULL;
    }

    strcpy(buffer, io->prefix);
    if (need_sep)
    {
        buffer[prefix_len] = '/';
        strcpy(buffer + prefix_len + 1, pathname);
    }
    else
    {
        strcpy(buffer + prefix_len, pathname);
    }

    return buffer;
}

//
// File/device management
//

LogoStream *logo_io_open(LogoIO *io, const char *pathname)
{
    if (!io || !pathname)
    {
        return NULL;
    }

    // Resolve the pathname with prefix
    char resolved[LOGO_STREAM_NAME_MAX];
    char *full_path = logo_io_resolve_path(io, pathname, resolved, sizeof(resolved));
    if (!full_path)
    {
        return NULL;
    }

    // Check if already open
    LogoStream *existing = logo_io_find_open(io, full_path);
    if (existing)
    {
        return existing;
    }

    // Check if we have room
    if (io->open_count >= LOGO_MAX_OPEN_FILES)
    {
        return NULL;
    }

    // Check if we have a file opener
    if (!io->file_opener)
    {
        return NULL;
    }

    // Try to open the file - if it doesn't exist, create it
    LogoStream *stream = io->file_opener(full_path, LOGO_FILE_UPDATE);
    if (!stream)
    {
        // File doesn't exist, try to create it
        stream = io->file_opener(full_path, LOGO_FILE_WRITE);
    }

    if (!stream)
    {
        return NULL;
    }

    // Find an empty slot
    for (int i = 0; i < LOGO_MAX_OPEN_FILES; i++)
    {
        if (io->open_streams[i] == NULL)
        {
            io->open_streams[i] = stream;
            io->open_count++;
            return stream;
        }
    }

    // No slot found (shouldn't happen since we checked count above)
    logo_stream_close(stream);
    free(stream);
    return NULL;
}

// Helper function to open a file with a specific mode
static LogoStream *logo_io_open_with_mode(LogoIO *io, const char *pathname, LogoFileMode mode)
{
    if (!io || !pathname)
    {
        return NULL;
    }

    // Resolve the pathname with prefix
    char resolved[LOGO_STREAM_NAME_MAX];
    char *full_path = logo_io_resolve_path(io, pathname, resolved, sizeof(resolved));
    if (!full_path)
    {
        return NULL;
    }

    // Check if already open
    LogoStream *existing = logo_io_find_open(io, full_path);
    if (existing)
    {
        return existing;
    }

    // Check if we have room
    if (io->open_count >= LOGO_MAX_OPEN_FILES)
    {
        return NULL;
    }

    // Check if we have a file opener
    if (!io->file_opener)
    {
        return NULL;
    }

    // Open the file
    LogoStream *stream = io->file_opener(full_path, mode);
    if (!stream)
    {
        return NULL;
    }

    // Find an empty slot
    for (int i = 0; i < LOGO_MAX_OPEN_FILES; i++)
    {
        if (io->open_streams[i] == NULL)
        {
            io->open_streams[i] = stream;
            io->open_count++;
            return stream;
        }
    }

    // No slot found
    logo_stream_close(stream);
    free(stream);
    return NULL;
}

LogoStream *logo_io_open_read(LogoIO *io, const char *pathname)
{
    return logo_io_open_with_mode(io, pathname, LOGO_FILE_READ);
}

LogoStream *logo_io_open_write(LogoIO *io, const char *pathname)
{
    return logo_io_open_with_mode(io, pathname, LOGO_FILE_WRITE);
}

LogoStream *logo_io_open_append(LogoIO *io, const char *pathname)
{
    return logo_io_open_with_mode(io, pathname, LOGO_FILE_APPEND);
}

LogoStream *logo_io_open_update(LogoIO *io, const char *pathname)
{
    return logo_io_open_with_mode(io, pathname, LOGO_FILE_UPDATE);
}

void logo_io_close(LogoIO *io, const char *pathname)
{
    if (!io || !pathname)
    {
        return;
    }

    for (int i = 0; i < LOGO_MAX_OPEN_FILES; i++)
    {
        LogoStream *stream = io->open_streams[i];
        if (stream && strcmp(stream->name, pathname) == 0)
        {
            // If this stream is the current reader or writer, reset to console
            if (io->reader == stream)
            {
                io->reader = &io->console->input;
            }
            if (io->writer == stream)
            {
                io->writer = &io->console->output;
            }

            logo_stream_close(stream);
            free(stream);
            io->open_streams[i] = NULL;
            io->open_count--;
            return;
        }
    }
}

void logo_io_close_all(LogoIO *io)
{
    if (!io)
    {
        return;
    }

    for (int i = 0; i < LOGO_MAX_OPEN_FILES; i++)
    {
        if (io->open_streams[i])
        {
            logo_stream_close(io->open_streams[i]);
            free(io->open_streams[i]);
            io->open_streams[i] = NULL;
        }
    }
    io->open_count = 0;

    // Reset reader/writer to console
    if (io->console)
    {
        io->reader = &io->console->input;
        io->writer = &io->console->output;
    }
}

LogoStream *logo_io_find_open(LogoIO *io, const char *pathname)
{
    if (!io || !pathname)
    {
        return NULL;
    }

    for (int i = 0; i < LOGO_MAX_OPEN_FILES; i++)
    {
        LogoStream *stream = io->open_streams[i];
        if (stream && strcmp(stream->name, pathname) == 0)
        {
            return stream;
        }
    }

    return NULL;
}

bool logo_io_is_open(const LogoIO *io, const char *pathname)
{
    if (!io || !pathname)
    {
        return false;
    }

    for (int i = 0; i < LOGO_MAX_OPEN_FILES; i++)
    {
        LogoStream *stream = io->open_streams[i];
        if (stream && strcmp(stream->name, pathname) == 0)
        {
            return true;
        }
    }

    return false;
}

int logo_io_open_count(const LogoIO *io)
{
    return io ? io->open_count : 0;
}

LogoStream *logo_io_get_open(const LogoIO *io, int index)
{
    if (!io || index < 0)
    {
        return NULL;
    }

    // Return the nth non-NULL open stream
    int count = 0;
    for (int i = 0; i < LOGO_MAX_OPEN_FILES; i++)
    {
        if (io->open_streams[i])
        {
            if (count == index)
            {
                return io->open_streams[i];
            }
            count++;
        }
    }

    return NULL;
}

//
// Reader/writer control
//

void logo_io_set_reader(LogoIO *io, LogoStream *stream)
{
    if (!io)
    {
        return;
    }

    if (stream == NULL && io->console)
    {
        io->reader = &io->console->input;
    }
    else
    {
        io->reader = stream;
    }
}

void logo_io_set_writer(LogoIO *io, LogoStream *stream)
{
    if (!io)
    {
        return;
    }

    if (stream == NULL && io->console)
    {
        io->writer = &io->console->output;
    }
    else
    {
        io->writer = stream;
    }
}

const char *logo_io_get_reader_name(const LogoIO *io)
{
    if (!io || !io->reader)
    {
        return "";
    }

    // Return empty string for keyboard (Logo convention)
    if (io->console && io->reader == &io->console->input)
    {
        return "";
    }

    return io->reader->name;
}

const char *logo_io_get_writer_name(const LogoIO *io)
{
    if (!io || !io->writer)
    {
        return "";
    }

    // Return empty string for screen (Logo convention)
    if (io->console && io->writer == &io->console->output)
    {
        return "";
    }

    return io->writer->name;
}

bool logo_io_reader_is_keyboard(const LogoIO *io)
{
    if (!io || !io->console)
    {
        return false;
    }
    return io->reader == &io->console->input;
}

bool logo_io_writer_is_screen(const LogoIO *io)
{
    if (!io || !io->console)
    {
        return false;
    }
    return io->writer == &io->console->output;
}

//
// Dribble control
//

bool logo_io_start_dribble(LogoIO *io, const char *pathname)
{
    if (!io || !pathname)
    {
        return false;
    }

    // Stop any existing dribble first
    logo_io_stop_dribble(io);

    // Check if we have a file opener
    if (!io->file_opener)
    {
        return false;
    }

    // Resolve the pathname with prefix
    char resolved[LOGO_STREAM_NAME_MAX];
    char *full_path = logo_io_resolve_path(io, pathname, resolved, sizeof(resolved));
    if (!full_path)
    {
        return false;
    }

    // Open file for writing (append mode for dribble)
    LogoStream *stream = io->file_opener(full_path, LOGO_FILE_APPEND);
    if (!stream)
    {
        return false;
    }

    io->dribble = stream;
    return true;
}

void logo_io_stop_dribble(LogoIO *io)
{
    if (!io || !io->dribble)
    {
        return;
    }

    logo_stream_close(io->dribble);
    free(io->dribble);
    io->dribble = NULL;
}

bool logo_io_is_dribbling(const LogoIO *io)
{
    return io && io->dribble != NULL;
}

//
// High-level I/O operations
//

int logo_io_read_char(LogoIO *io)
{
    if (!io || !io->reader)
    {
        return -1;
    }

    return logo_stream_read_char(io->reader);
}

int logo_io_read_chars(LogoIO *io, char *buffer, int count)
{
    if (!io || !io->reader)
    {
        return -1;
    }

    return logo_stream_read_chars(io->reader, buffer, count);
}

int logo_io_read_line(LogoIO *io, char *buffer, size_t size)
{
    if (!io || !io->reader)
    {
        return -1;
    }

    return logo_stream_read_line(io->reader, buffer, size);
}

bool logo_io_key_available(LogoIO *io)
{
    if (!io || !io->reader)
    {
        return false;
    }

    return logo_stream_can_read(io->reader);
}

void logo_io_write(LogoIO *io, const char *text)
{
    if (!io || !text)
    {
        return;
    }

    // Write to current writer
    if (io->writer)
    {
        logo_stream_write(io->writer, text);
    }

    // Also write to dribble if active
    if (io->dribble)
    {
        logo_stream_write(io->dribble, text);
    }
}

void logo_io_write_line(LogoIO *io, const char *text)
{
    if (!io)
    {
        return;
    }

    if (text)
    {
        logo_io_write(io, text);
    }
    logo_io_write(io, "\n");
}

void logo_io_flush(LogoIO *io)
{
    if (!io)
    {
        return;
    }

    if (io->writer)
    {
        logo_stream_flush(io->writer);
    }

    if (io->dribble)
    {
        logo_stream_flush(io->dribble);
    }
}

//
// Direct console access
//

void logo_io_console_write(LogoIO *io, const char *text)
{
    if (!io || !io->console || !text)
    {
        return;
    }

    logo_stream_write(&io->console->output, text);

    // Dribble even for direct console writes
    if (io->dribble)
    {
        logo_stream_write(io->dribble, text);
    }
}

void logo_io_console_write_line(LogoIO *io, const char *text)
{
    if (!io || !io->console)
    {
        return;
    }

    if (text)
    {
        logo_io_console_write(io, text);
    }
    logo_io_console_write(io, "\n");
}
