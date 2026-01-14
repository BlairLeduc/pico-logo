//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements the LogoIO I/O state manager.
//

#include "devices/io.h"
#include "devices/storage.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

//
// Lifecycle
//

void logo_io_init(LogoIO *io, LogoConsole *console, LogoStorage *storage, LogoHardware *hardware)
{
    if (!io)
    {
        return;
    }

    io->console = console;
    io->storage = storage;
    io->hardware = hardware;
    
    // Default reader is keyboard, writer is screen
    io->reader = console ? &console->input : NULL;
    io->writer = console ? &console->output : NULL;
    
    io->dribble = NULL;
    io->open_count = 0;
    io->prefix[0] = '\0';
    io->network_timeout = LOGO_DEFAULT_NETWORK_TIMEOUT;

    for (int i = 0; i < LOGO_MAX_OPEN_FILES; i++)
    {
        io->open_streams[i] = NULL;
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
// Device-specific operations
//

void logo_io_sleep(LogoIO *io, int milliseconds)
{
    if (!io)
    {
        return;
    }

    io->hardware->ops->sleep(milliseconds);
}

uint32_t logo_io_random(LogoIO *io)
{
    if (!io)
    {
        return 0;
    }

    return io->hardware->ops->random();
}

void logo_io_get_battery_level(LogoIO *io, int *level, bool *charging)
{
    if (!level || !charging)
    {
        return;
    }

    // Default values
    *level = -1;
    *charging = false;

    if (!io || !io->hardware || !io->hardware->ops || !io->hardware->ops->get_battery_level)
    {
        return;
    }
    io->hardware->ops->get_battery_level(level, charging);
}

bool logo_io_check_user_interrupt(LogoIO *io)
{
    if (!io || !io->hardware || !io->hardware->ops || !io->hardware->ops->check_user_interrupt)
    {
        return false;
    }

    if (io->hardware->ops->check_user_interrupt())
    {
        // Clear the interrupt flag and return true
        if (io->hardware->ops->clear_user_interrupt)
        {
            io->hardware->ops->clear_user_interrupt();
        }
        return true;
    }
    return false;
}

bool logo_io_check_pause_request(LogoIO *io)
{
    if (!io || !io->hardware || !io->hardware->ops || !io->hardware->ops->check_pause_request)
    {
        return false;
    }

    // Don't clear the flag here - let the caller decide when to clear
    // This allows the pause to be deferred until inside a procedure
    return io->hardware->ops->check_pause_request();
}

void logo_io_clear_pause_request(LogoIO *io)
{
    if (!io || !io->hardware || !io->hardware->ops || !io->hardware->ops->clear_pause_request)
    {
        return;
    }
    io->hardware->ops->clear_pause_request();
}

bool logo_io_check_freeze_request(LogoIO *io)
{
    if (!io || !io->hardware || !io->hardware->ops || !io->hardware->ops->check_freeze_request)
    {
        return false;
    }

    if (io->hardware->ops->check_freeze_request())
    {
        // Clear the freeze flag and return true
        if (io->hardware->ops->clear_freeze_request)
        {
            io->hardware->ops->clear_freeze_request();
        }
        return true;
    }
    return false;
}

void logo_io_clear_freeze_request(LogoIO *io)
{
    if (!io || !io->hardware || !io->hardware->ops || !io->hardware->ops->clear_freeze_request)
    {
        return;
    }
    io->hardware->ops->clear_freeze_request();
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

//
// Network address parsing
//

bool logo_io_parse_network_address(const char *target, char *host_out, size_t host_size, uint16_t *port_out)
{
    if (!target || !host_out || host_size == 0 || !port_out)
    {
        return false;
    }

    // Find the last colon in the string
    const char *last_colon = strrchr(target, ':');
    if (!last_colon || last_colon == target)
    {
        // No colon found or colon is at the start (empty hostname)
        return false;
    }

    // Check that there's something after the colon
    const char *port_str = last_colon + 1;
    if (*port_str == '\0')
    {
        return false;
    }

    // Parse the port number - must be all digits
    char *end;
    long port = strtol(port_str, &end, 10);
    
    // Check that we consumed the entire port string and it's a valid port
    if (*end != '\0' || port < 1 || port > 65535)
    {
        return false;
    }

    // Calculate hostname length
    size_t host_len = last_colon - target;
    if (host_len == 0 || host_len >= host_size)
    {
        return false;
    }

    // Copy hostname
    memcpy(host_out, target, host_len);
    host_out[host_len] = '\0';
    *port_out = (uint16_t)port;

    return true;
}

bool logo_io_is_network_address(const char *target)
{
    char host[LOGO_STREAM_NAME_MAX];
    uint16_t port;
    return logo_io_parse_network_address(target, host, sizeof(host), &port);
}

//
// Network timeout management
//

void logo_io_set_timeout(LogoIO *io, int timeout_tenths)
{
    if (!io)
    {
        return;
    }
    io->network_timeout = timeout_tenths >= 0 ? timeout_tenths : 0;
}

int logo_io_get_timeout(const LogoIO *io)
{
    if (!io)
    {
        return 0;
    }
    return io->network_timeout;
}

//
// Network stream helper
//

bool logo_io_is_network_stream(const LogoStream *stream)
{
    return stream && stream->type == LOGO_STREAM_NETWORK;
}

// Normalize a path in-place, resolving "." and ".." segments
// Returns the buffer on success, NULL if the path tries to go above root
static char *normalize_path(char *buffer)
{
    if (!buffer || buffer[0] == '\0')
    {
        return buffer;
    }

    bool is_absolute = (buffer[0] == '/');
    
    // We'll use an array of pointers to track the start of each path component
    // and a write pointer to build the normalized path
    char *read = buffer;
    char *write = buffer;
    
    // Preserve leading slash for absolute paths
    if (is_absolute)
    {
        read++;
        write++;
    }
    
    while (*read != '\0')
    {
        // Skip consecutive slashes
        while (*read == '/')
        {
            read++;
        }
        
        if (*read == '\0')
        {
            break;
        }
        
        // Find the end of this component
        char *component_start = read;
        while (*read != '\0' && *read != '/')
        {
            read++;
        }
        size_t component_len = read - component_start;
        
        // Check for "." (current directory) - skip it
        if (component_len == 1 && component_start[0] == '.')
        {
            continue;
        }
        
        // Check for ".." (parent directory)
        if (component_len == 2 && component_start[0] == '.' && component_start[1] == '.')
        {
            // Go back to previous component
            if (write > buffer + (is_absolute ? 1 : 0))
            {
                // Remove trailing slash if present
                if (write > buffer && *(write - 1) == '/')
                {
                    write--;
                }
                // Back up to the previous slash or start
                while (write > buffer + (is_absolute ? 1 : 0) && *(write - 1) != '/')
                {
                    write--;
                }
                // Remove the trailing slash we stopped at (unless we're at root)
                if (write > buffer + (is_absolute ? 1 : 0) && *(write - 1) == '/')
                {
                    write--;
                }
            }
            else if (!is_absolute)
            {
                // For relative paths, we can't go above the starting point
                // So we need to keep the ".." in the result
                if (write > buffer && *(write - 1) != '/')
                {
                    *write++ = '/';
                }
                *write++ = '.';
                *write++ = '.';
            }
            // For absolute paths at root, just ignore the ".."
            continue;
        }
        
        // Regular component - add it
        if (write > buffer + (is_absolute ? 1 : 0))
        {
            // Add separator before component (unless we're at the start)
            if (*(write - 1) != '/')
            {
                *write++ = '/';
            }
        }
        
        // Copy the component
        memmove(write, component_start, component_len);
        write += component_len;
    }
    
    // Handle edge cases
    if (is_absolute && write == buffer + 1)
    {
        // Result is just "/" for absolute path
        // write is already pointing past the /
    }
    else if (!is_absolute && write == buffer)
    {
        // Result is empty for relative path - use "."
        *write++ = '.';
    }
    
    *write = '\0';
    return buffer;
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
        return normalize_path(buffer);
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

    return normalize_path(buffer);
}

//
// File/device/network management
//

// Forward declaration for network stream creation
static LogoStream *create_network_stream(LogoIO *io, const char *host, uint16_t port, const char *name);

LogoStream *logo_io_open(LogoIO *io, const char *target)
{
    if (!io || !target)
    {
        return NULL;
    }

    // Check if we have room
    if (io->open_count >= LOGO_MAX_OPEN_FILES)
    {
        return NULL;
    }

    // Check if this is a network address (host:port format)
    char host[LOGO_STREAM_NAME_MAX];
    uint16_t port;
    if (logo_io_parse_network_address(target, host, sizeof(host), &port))
    {
        // It's a network address - check if already open using the original target as name
        LogoStream *existing = logo_io_find_open(io, target);
        if (existing)
        {
            return existing;
        }

        // Open network connection
        return logo_io_open_network(io, host, port);
    }

    // It's a file - resolve the pathname with prefix
    char resolved[LOGO_STREAM_NAME_MAX];
    char *full_path = logo_io_resolve_path(io, target, resolved, sizeof(resolved));
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

    // Check if we have a file opener
    if (!io->storage || !io->storage->ops || !io->storage->ops->open)
    {
        return NULL;
    }

    // Try to open the file - if it doesn't exist, create it
    LogoStream *stream = io->storage->ops->open(full_path);
    if (!stream)
    {
        // File doesn't exist, try to create it
        stream = io->storage->ops->open(full_path);
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

LogoStream *logo_io_open_network(LogoIO *io, const char *host, uint16_t port)
{
    if (!io || !host)
    {
        return NULL;
    }

    // Check if we have room
    if (io->open_count >= LOGO_MAX_OPEN_FILES)
    {
        return NULL;
    }

    // Format the connection name as host:port (using original host, not resolved IP)
    char name[LOGO_STREAM_NAME_MAX];
    snprintf(name, sizeof(name), "%s:%u", host, port);

    // Check if already open
    LogoStream *existing = logo_io_find_open(io, name);
    if (existing)
    {
        return existing;
    }

    // Resolve hostname to IP if needed
    char ip_address[16];
    if (io->hardware && io->hardware->ops && io->hardware->ops->network_resolve)
    {
        if (!io->hardware->ops->network_resolve(host, ip_address, sizeof(ip_address)))
        {
            // Resolution failed
            return NULL;
        }
    }
    else
    {
        // No resolver available - assume host is already an IP address
        strncpy(ip_address, host, sizeof(ip_address) - 1);
        ip_address[sizeof(ip_address) - 1] = '\0';
    }

    // Create the network stream
    LogoStream *stream = create_network_stream(io, ip_address, port, name);
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

void logo_io_close(LogoIO *io, const char *name)
{
    if (!io || !name)
    {
        return;
    }

    // Determine if this is a network address or file path
    const char *lookup_name;
    char resolved[LOGO_STREAM_NAME_MAX];
    
    if (logo_io_is_network_address(name))
    {
        // Network addresses are stored with their original name
        lookup_name = name;
    }
    else
    {
        // File paths need to be resolved with prefix
        char *full_path = logo_io_resolve_path(io, name, resolved, sizeof(resolved));
        if (!full_path)
        {
            return;
        }
        lookup_name = full_path;
    }

    for (int i = 0; i < LOGO_MAX_OPEN_FILES; i++)
    {
        LogoStream *stream = io->open_streams[i];
        if (stream && strcmp(stream->name, lookup_name) == 0)
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

LogoStream *logo_io_find_open(LogoIO *io, const char *name)
{
    if (!io || !name)
    {
        return NULL;
    }

    // Determine if this is a network address or file path
    const char *lookup_name;
    char resolved[LOGO_STREAM_NAME_MAX];
    
    if (logo_io_is_network_address(name))
    {
        // Network addresses are stored with their original name
        lookup_name = name;
    }
    else
    {
        // File paths need to be resolved with prefix
        char *full_path = logo_io_resolve_path(io, name, resolved, sizeof(resolved));
        if (!full_path)
        {
            return NULL;
        }
        lookup_name = full_path;
    }

    for (int i = 0; i < LOGO_MAX_OPEN_FILES; i++)
    {
        LogoStream *stream = io->open_streams[i];
        if (stream && strcmp(stream->name, lookup_name) == 0)
        {
            return stream;
        }
    }

    return NULL;
}

bool logo_io_is_open(const LogoIO *io, const char *name)
{
    if (!io || !name)
    {
        return false;
    }

    // Determine if this is a network address or file path
    const char *lookup_name;
    char resolved[LOGO_STREAM_NAME_MAX];
    
    if (logo_io_is_network_address(name))
    {
        // Network addresses are stored with their original name
        lookup_name = name;
    }
    else
    {
        // File paths need to be resolved with prefix
        char *full_path = logo_io_resolve_path(io, name, resolved, sizeof(resolved));
        if (!full_path)
        {
            return false;
        }
        lookup_name = full_path;
    }

    for (int i = 0; i < LOGO_MAX_OPEN_FILES; i++)
    {
        LogoStream *stream = io->open_streams[i];
        if (stream && strcmp(stream->name, lookup_name) == 0)
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

bool logo_io_file_exists(const LogoIO *io, const char *pathname)
{
    if (!io || !pathname)
    {
        return false;
    }

    // Resolve the pathname with prefix
    char resolved[LOGO_STREAM_NAME_MAX];
    char *full_path = logo_io_resolve_path(io, pathname, resolved, sizeof(resolved));
    if (!full_path)
    {
        return NULL;
    }

    return io->storage->ops->file_exists(full_path);
}   

bool logo_io_dir_exists(const LogoIO *io, const char *pathname)
{
    if (!io || !io->storage || !pathname)
    {
        return false;
    }

    // Resolve the pathname with prefix
    char resolved[LOGO_STREAM_NAME_MAX];
    char *full_path = logo_io_resolve_path(io, pathname, resolved, sizeof(resolved));
    if (!full_path)
    {
        return NULL;
    }

    return io->storage->ops->dir_exists(full_path);
}

bool logo_io_file_delete(const LogoIO *io, const char *pathname)
{
    if (!io || !io->storage || !pathname)
    {
        return false;
    }

    // Resolve the pathname with prefix
    char resolved[LOGO_STREAM_NAME_MAX];
    char *full_path = logo_io_resolve_path(io, pathname, resolved, sizeof(resolved));
    if (!full_path)
    {
        return NULL;
    }

    return io->storage->ops->file_delete(full_path);
}

bool logo_io_dir_create(const LogoIO *io, const char *pathname)
{
    if (!io || !io->storage || !pathname)
    {
        return false;
    }

    // Resolve the pathname with prefix
    char resolved[LOGO_STREAM_NAME_MAX];
    char *full_path = logo_io_resolve_path(io, pathname, resolved, sizeof(resolved));
    if (!full_path)
    {
        return NULL;
    }

    return io->storage->ops->dir_create(full_path);
}

bool logo_io_dir_delete(const LogoIO *io, const char *pathname)
{
    if (!io || !io->storage || !pathname)
    {
        return false;
    }

    // Resolve the pathname with prefix
    char resolved[LOGO_STREAM_NAME_MAX];
    char *full_path = logo_io_resolve_path(io, pathname, resolved, sizeof(resolved));
    if (!full_path)
    {
        return NULL;
    }

    return io->storage->ops->dir_delete(full_path);
}

bool logo_io_rename(const LogoIO *io, const char *old_path, const char *new_path)
{
    if (!io || !io->storage || !old_path || !new_path)
    {
        return false;
    }

    // Resolve the pathname with prefix
    char resolved_old[LOGO_STREAM_NAME_MAX];
    char *full_old_path = logo_io_resolve_path(io, old_path, resolved_old, sizeof(resolved_old));
    if (!full_old_path)
    {
        return NULL;
    }
    char resolved_new[LOGO_STREAM_NAME_MAX];
    char *full_new_path = logo_io_resolve_path(io, new_path, resolved_new, sizeof(resolved_new));
    if (!full_new_path)
    {
        return NULL;
    }

    return io->storage->ops->rename(full_old_path, full_new_path);
}

long logo_io_file_size(const LogoIO *io, const char *pathname)
{
    if (!io || !io->storage || !pathname)
    {
        return -1;
    }

    // Resolve the pathname with prefix
    char resolved[LOGO_STREAM_NAME_MAX];
    char *full_path = logo_io_resolve_path(io, pathname, resolved, sizeof(resolved));
    if (!full_path)
    {
        return -1;
    }

    return io->storage->ops->file_size(full_path);
}

bool logo_io_list_directory(const LogoIO *io, const char *pathname,
                             LogoDirCallback callback, void *user_data,
                             const char *filter)
{
    if (!io || !io->storage || !pathname || !callback)
    {
        return false;
    }

    return io->storage->ops->list_directory(pathname, callback, user_data, filter);
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

    // Caller should check for already dribbling before calling
    if (io->dribble)
    {
        return false;
    }

    // Check if we have a file opener
    if (!io->storage || !io->storage->ops->open)
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
    LogoStream *stream = io->storage->ops->open(full_path);
    if (!stream)
    {
        return false;
    }
    stream->ops->set_write_pos(stream, stream->ops->get_length(stream));

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

void logo_io_dribble_input(LogoIO *io, const char *text)
{
    if (!io || !io->dribble || !text)
    {
        return;
    }

    logo_stream_write(io->dribble, text);
    logo_stream_write(io->dribble, "\n");
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

bool logo_io_check_write_error(LogoIO *io)
{
    if (!io)
    {
        return false;
    }

    bool error = false;

    // Check writer for errors
    if (io->writer && logo_stream_has_write_error(io->writer))
    {
        logo_stream_clear_write_error(io->writer);
        error = true;
    }

    // Check dribble for errors
    if (io->dribble && logo_stream_has_write_error(io->dribble))
    {
        logo_stream_clear_write_error(io->dribble);
        error = true;
    }

    return error;
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

//
// Network stream implementation
//

typedef struct NetworkStreamContext
{
    LogoIO *io;           // Back-reference to LogoIO for timeout and hardware access
    void *connection;     // Opaque connection handle from hardware layer
} NetworkStreamContext;

static int network_stream_read_char(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return LOGO_STREAM_EOF;
    }

    NetworkStreamContext *ctx = (NetworkStreamContext *)stream->context;
    if (!ctx->io || !ctx->io->hardware || !ctx->io->hardware->ops || !ctx->io->hardware->ops->network_tcp_read)
    {
        return LOGO_STREAM_EOF;
    }

    // Convert timeout from tenths of a second to milliseconds
    int timeout_ms = ctx->io->network_timeout * 100;
    
    char c;
    int result = ctx->io->hardware->ops->network_tcp_read(ctx->connection, &c, 1, timeout_ms);
    
    if (result > 0)
    {
        return (unsigned char)c;
    }
    else if (result == 0)
    {
        return LOGO_STREAM_TIMEOUT;
    }
    else
    {
        return LOGO_STREAM_EOF;
    }
}

static int network_stream_read_chars(LogoStream *stream, char *buffer, int count)
{
    if (!stream || !stream->context || !buffer || count <= 0)
    {
        return -1;
    }

    NetworkStreamContext *ctx = (NetworkStreamContext *)stream->context;
    if (!ctx->io || !ctx->io->hardware || !ctx->io->hardware->ops || !ctx->io->hardware->ops->network_tcp_read)
    {
        return -1;
    }

    // Convert timeout from tenths of a second to milliseconds
    int timeout_ms = ctx->io->network_timeout * 100;
    
    return ctx->io->hardware->ops->network_tcp_read(ctx->connection, buffer, count, timeout_ms);
}

static int network_stream_read_line(LogoStream *stream, char *buffer, size_t size)
{
    if (!stream || !stream->context || !buffer || size == 0)
    {
        return -1;
    }

    NetworkStreamContext *ctx = (NetworkStreamContext *)stream->context;
    if (!ctx->io || !ctx->io->hardware || !ctx->io->hardware->ops || !ctx->io->hardware->ops->network_tcp_read)
    {
        return -1;
    }

    // Convert timeout from tenths of a second to milliseconds
    int timeout_ms = ctx->io->network_timeout * 100;
    
    // Read one character at a time until newline or timeout
    size_t pos = 0;
    while (pos < size - 1)
    {
        char c;
        int result = ctx->io->hardware->ops->network_tcp_read(ctx->connection, &c, 1, timeout_ms);
        
        if (result > 0)
        {
            if (c == '\n')
            {
                break;
            }
            if (c != '\r')  // Skip carriage return
            {
                buffer[pos++] = c;
            }
        }
        else if (result == 0)
        {
            // Timeout - return what we have (may be partial line)
            break;
        }
        else
        {
            // Error or connection closed
            if (pos == 0)
            {
                return -1;
            }
            break;
        }
    }

    buffer[pos] = '\0';
    return (int)pos;
}

static bool network_stream_can_read(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return false;
    }

    NetworkStreamContext *ctx = (NetworkStreamContext *)stream->context;
    if (!ctx->io || !ctx->io->hardware || !ctx->io->hardware->ops || !ctx->io->hardware->ops->network_tcp_can_read)
    {
        return false;
    }

    return ctx->io->hardware->ops->network_tcp_can_read(ctx->connection);
}

static void network_stream_write(LogoStream *stream, const char *text)
{
    if (!stream || !stream->context || !text)
    {
        return;
    }

    NetworkStreamContext *ctx = (NetworkStreamContext *)stream->context;
    if (!ctx->io || !ctx->io->hardware || !ctx->io->hardware->ops || !ctx->io->hardware->ops->network_tcp_write)
    {
        stream->write_error = true;
        return;
    }

    int len = (int)strlen(text);
    int result = ctx->io->hardware->ops->network_tcp_write(ctx->connection, text, len);
    if (result < 0 || result != len)
    {
        stream->write_error = true;
    }
}

static void network_stream_flush(LogoStream *stream)
{
    // TCP doesn't need explicit flushing - writes are sent immediately
    (void)stream;
}

static void network_stream_close(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return;
    }

    NetworkStreamContext *ctx = (NetworkStreamContext *)stream->context;
    if (ctx->io && ctx->io->hardware && ctx->io->hardware->ops && ctx->io->hardware->ops->network_tcp_close)
    {
        ctx->io->hardware->ops->network_tcp_close(ctx->connection);
    }

    free(ctx);
    stream->context = NULL;
    stream->is_open = false;
}

static const LogoStreamOps network_stream_ops = {
    .read_char = network_stream_read_char,
    .read_chars = network_stream_read_chars,
    .read_line = network_stream_read_line,
    .can_read = network_stream_can_read,
    .write = network_stream_write,
    .flush = network_stream_flush,
    .get_read_pos = NULL,   // Network streams are not seekable
    .set_read_pos = NULL,
    .get_write_pos = NULL,
    .set_write_pos = NULL,
    .get_length = NULL,
    .close = network_stream_close,
};

static LogoStream *create_network_stream(LogoIO *io, const char *ip_address, uint16_t port, const char *name)
{
    if (!io || !ip_address || !name)
    {
        return NULL;
    }

    // Check if hardware supports TCP connections
    if (!io->hardware || !io->hardware->ops || !io->hardware->ops->network_tcp_connect)
    {
        return NULL;
    }

    // Convert timeout from tenths of a second to milliseconds
    int timeout_ms = io->network_timeout * 100;

    // Establish connection
    void *connection = io->hardware->ops->network_tcp_connect(ip_address, port, timeout_ms);
    if (!connection)
    {
        return NULL;
    }

    // Allocate stream
    LogoStream *stream = (LogoStream *)malloc(sizeof(LogoStream));
    if (!stream)
    {
        io->hardware->ops->network_tcp_close(connection);
        return NULL;
    }

    // Allocate context
    NetworkStreamContext *ctx = (NetworkStreamContext *)malloc(sizeof(NetworkStreamContext));
    if (!ctx)
    {
        io->hardware->ops->network_tcp_close(connection);
        free(stream);
        return NULL;
    }

    ctx->io = io;
    ctx->connection = connection;

    // Initialize the stream
    logo_stream_init(stream, LOGO_STREAM_NETWORK, &network_stream_ops, ctx, name);

    return stream;
}
