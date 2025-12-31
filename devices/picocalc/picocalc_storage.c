//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements the LogoStorage interface for PicoCalc device.
//

#include "../storage.h"
#include "picocalc_storage.h"
#include "fat32.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

//
// File stream context - wraps a fat32_file_t* with separate read/write positions
//
typedef struct FileContext
{
    fat32_file_t *file;
    long read_pos;   // Separate read position
    long write_pos;  // Separate write position (starts at end of file)
} FileContext;

//
// Stream operation implementations
//

static int picocalc_file_read_char(LogoStream *stream)
{
    char buffer[2];
    if (!stream || !stream->context)
    {
        return -1;
    }

    FileContext *ctx = (FileContext *)stream->context;
    if (!ctx->file)
    {
        return -1;
    }

    // Seek to read position
    fat32_seek(ctx->file, (uint32_t)ctx->read_pos);
    
    size_t read;
    if (fat32_read(ctx->file, buffer, 1, &read) == FAT32_OK && read == 1)
    {
        ctx->read_pos++;
        return (unsigned char)buffer[0];
    }
    return -1;
}

static int picocalc_file_read_chars(LogoStream *stream, char *buffer, int count)
{
    if (!stream || !stream->context || !buffer || count <= 0)
    {
        return -1;
    }

    FileContext *ctx = (FileContext *)stream->context;
    if (!ctx->file)
    {
        return -1;
    }

    // Seek to read position
    fat32_seek(ctx->file, (uint32_t)ctx->read_pos);
    
    size_t read;
    if (fat32_read(ctx->file, buffer, (size_t)count, &read) == FAT32_OK)
    {
        ctx->read_pos += (long)read;
        return (int)read;
    }

    return -1;
}

static int picocalc_file_read_line(LogoStream *stream, char *buffer, size_t size)
{
    if (!stream || !stream->context || !buffer || size == 0)
    {
        return -1;
    }

    FileContext *ctx = (FileContext *)stream->context;
    if (!ctx->file)
    {
        return -1;
    }

    // Seek to read position
    fat32_seek(ctx->file, (uint32_t)ctx->read_pos);
    
    size_t total_read = 0;
    bool line_ended = false;
    while (total_read < size - 1) // Leave space for null terminator
    {
        size_t read;
        if (fat32_read(ctx->file, &buffer[total_read], 1, &read) != FAT32_OK || read == 0)
        {
            break; // Read error or EOF
        }
        ctx->read_pos++;
        if (buffer[total_read] == '\n' || buffer[total_read] == '\r')
        {
            line_ended = true;
            break; // End of line
        }
        total_read++;
    }
    buffer[total_read] = '\0';
    // Return -1 only for EOF/error at start of line, not for empty lines
    return (total_read > 0 || line_ended) ? (int)total_read : -1;
}

static bool picocalc_file_can_read(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return false;
    }

    FileContext *ctx = (FileContext *)stream->context;
    if (!ctx->file)
    {
        return false;
    }

    // Check if read position is at or past end of file
    return ctx->read_pos < (long)fat32_size(ctx->file);
}

static void picocalc_file_write(LogoStream *stream, const char *text)
{
    if (!stream || !stream->context || !text)
    {
        return;
    }

    FileContext *ctx = (FileContext *)stream->context;
    if (!ctx->file)
    {
        return;
    }

    // Seek to write position
    fat32_seek(ctx->file, (uint32_t)ctx->write_pos);
    
    size_t len = strlen(text);
    size_t written;
    fat32_write(ctx->file, text, len, &written);
    ctx->write_pos += (long)written;
    
    // Check for partial write (disk full or other error)
    if (written < len)
    {
        stream->write_error = true;
    }
}

static void picocalc_file_flush(LogoStream *stream)
{
    // No-op for FAT32 files
    (void)stream;
}

static long picocalc_file_get_read_pos(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return -1;
    }

    FileContext *ctx = (FileContext *)stream->context;
    if (!ctx->file)
    {
        return -1;
    }

    return ctx->read_pos;
}

static bool picocalc_file_set_read_pos(LogoStream *stream, long pos)
{
    if (!stream || !stream->context)
    {
        return false;
    }

    FileContext *ctx = (FileContext *)stream->context;
    if (!ctx->file)
    {
        return false;
    }

    if (pos < 0)
    {
        return false;
    }

    // Get file size to ensure the requested position is within bounds
    uint32_t file_size = fat32_size(ctx->file);

    if ((uint32_t)pos > file_size)
    {
        return false;
    }

    ctx->read_pos = pos;
    return true;
}

static long picocalc_file_get_write_pos(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return -1;
    }

    FileContext *ctx = (FileContext *)stream->context;
    if (!ctx->file)
    {
        return -1;
    }

    return ctx->write_pos;
}

static bool picocalc_file_set_write_pos(LogoStream *stream, long pos)
{
    if (!stream || !stream->context)
    {
        return false;
    }

    FileContext *ctx = (FileContext *)stream->context;
    if (!ctx->file)
    {
        return false;
    }

    // Validate lower bound
    if (pos < 0)
    {
        return false;
    }

    // Validate upper bound against current file length
    {
        uint32_t file_size = fat32_size(ctx->file);
        long max_pos = (long)file_size;

        // Allow positioning at most at end-of-file (for appending),
        // but not beyond the current file length.
        if (pos > max_pos)
        {
            return false;
        }
    }
    ctx->write_pos = pos;
    return true;
}

static long picocalc_file_get_length(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return -1;
    }

    FileContext *ctx = (FileContext *)stream->context;
    if (!ctx->file)
    {
        return -1;
    }

    return fat32_size(ctx->file);
}

static void picocalc_file_close(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return;
    }

    FileContext *ctx = (FileContext *)stream->context;
    if (ctx->file)
    {
        fat32_close(ctx->file);
        free(ctx->file);
        ctx->file = NULL;
    }

    // Free the context (stream itself is freed by caller)
    free(ctx);
    stream->context = NULL;
    stream->is_open = false;
}

//
// Stream operations table
//
static const LogoStreamOps picocalc_stream_ops = {
    .read_char = picocalc_file_read_char,
    .read_chars = picocalc_file_read_chars,
    .read_line = picocalc_file_read_line,
    .can_read = picocalc_file_can_read,
    .write = picocalc_file_write,
    .flush = picocalc_file_flush,
    .get_read_pos = picocalc_file_get_read_pos,
    .set_read_pos = picocalc_file_set_read_pos,
    .get_write_pos = picocalc_file_get_write_pos,
    .set_write_pos = picocalc_file_set_write_pos,
    .get_length = picocalc_file_get_length,
    .close = picocalc_file_close,
};

//
// File Manager API
//

static LogoStream *logo_picocalc_file_open(const char *pathname)
{
    if (!pathname)
    {
        return NULL;
    }

    // Open the file
    fat32_file_t *file = (fat32_file_t *)malloc(sizeof(fat32_file_t));
    if (!file)
    {
        return NULL;
    }
    fat32_error_t result = fat32_open(file, pathname);
    if (result == FAT32_ERROR_FILE_NOT_FOUND)
    {
        // Try to create the file
        result = fat32_create(file, pathname);
    }
    if (result != FAT32_OK)
    {

        free(file);
        return NULL;
    }

    if (file->attributes & FAT32_ATTR_DIRECTORY)
    {
        // Cannot open directories as files
        fat32_close(file);
        free(file);
        return NULL;
    }

    // Allocate context
    FileContext *ctx = (FileContext *)malloc(sizeof(FileContext));
    if (!ctx)
    {
        fat32_close(file);
        free(file);
        return NULL;
    }
    ctx->file = file;
    ctx->read_pos = 0;  // Read position starts at beginning of file
    ctx->write_pos = (long)fat32_size(file);  // Write position starts at end of file

    // Allocate stream
    LogoStream *stream = (LogoStream *)malloc(sizeof(LogoStream));
    if (!stream)
    {
        fat32_close(file);
        free(file);
        free(ctx);
        return NULL;
    }

    // Initialize stream
    stream->type = LOGO_STREAM_FILE;
    stream->ops = &picocalc_stream_ops;
    stream->context = ctx;
    stream->is_open = true;

    // Copy pathname to stream name
    strncpy(stream->name, pathname, LOGO_STREAM_NAME_MAX - 1);
    stream->name[LOGO_STREAM_NAME_MAX - 1] = '\0';

    return stream;
}

static bool logo_picocalc_file_exists(const char *pathname)
{
    if (!pathname)
    {
        return false;
    }

    fat32_file_t file;
    if (fat32_open(&file, pathname) == FAT32_OK)
    {
        bool is_file = (file.attributes & FAT32_ATTR_DIRECTORY) == 0;
        fat32_close(&file);
        return is_file;
    }
    return false;
}

static bool logo_picocalc_dir_exists(const char *pathname)
{
    if (!pathname)
    {
        return false;
    }

    fat32_file_t dir;
    if (fat32_open(&dir, pathname) == FAT32_OK)
    {
        bool is_dir = (dir.attributes & FAT32_ATTR_DIRECTORY) != 0;
        fat32_close(&dir);
        return is_dir;
    }
    return false;
}

static bool logo_picocalc_file_delete(const char *pathname)
{
    if (!pathname)
    {
        return false;
    }

    fat32_file_t file;
    if (fat32_open(&file, pathname) == FAT32_OK)
    {
        if (file.attributes & FAT32_ATTR_DIRECTORY)
        {
            fat32_close(&file);
            return false; // Not a file
        }
        fat32_close(&file);
        return fat32_delete(pathname) == FAT32_OK;
    }
    return false;
}

static bool logo_picocalc_dir_create(const char *pathname)
{
    if (!pathname)
    {
        return false;
    }

    fat32_file_t dir;
    if (fat32_dir_create(&dir, pathname) == FAT32_OK)
    {
        fat32_close(&dir);
        return true;
    }
    return false;
}

static bool logo_picocalc_dir_delete(const char *pathname)
{
    if (!pathname)
    {
        return false;
    }

    fat32_file_t dir;
    if (fat32_open(&dir, pathname) == FAT32_OK)
    {
        if (!(dir.attributes & FAT32_ATTR_DIRECTORY))
        {
            fat32_close(&dir);
            return false; // Not a directory
        }
        // Check if directory is empty
        fat32_entry_t entry;
        while (fat32_dir_read(&dir, &entry) == FAT32_OK && entry.filename[0])
        {
            if (strcmp(entry.filename, ".") != 0 && strcmp(entry.filename, "..") != 0)
            {   
                fat32_close(&dir);
                return false; // Directory not empty
            }
        }

        fat32_close(&dir);
        return fat32_delete(pathname) == FAT32_OK;
    }
    return false;
}

static bool logo_picocalc_rename(const char *old_path, const char *new_path)
{
    if (!old_path || !new_path)
    {
        return false;
    }

    return fat32_rename(old_path, new_path) == FAT32_OK;
}

static long logo_picocalc_file_size(const char *pathname)
{
    if (!pathname)
    {
        return -1;
    }

    fat32_file_t file;
    if (fat32_open(&file, pathname) == FAT32_OK)
    {
        if (file.attributes & FAT32_ATTR_DIRECTORY)
        {
            fat32_close(&file);
            return -1; // Not a file
        }
        long size = (long)fat32_size(&file);
        fat32_close(&file);
        return size;
    }
    return -1;
}

bool logo_picocalc_list_directory(const char *pathname, LogoDirCallback callback,
                                  void *user_data, const char *filter)
{
    if (!pathname || !callback)
    {
        return false;
    }

    fat32_file_t dir;
    if (fat32_open(&dir, pathname) != FAT32_OK || !(dir.attributes & FAT32_ATTR_DIRECTORY))
    {
        return false;
    }

    fat32_entry_t entry;

    while (fat32_dir_read(&dir, &entry) == FAT32_OK && entry.filename[0])
    {
        // Skip . and ..
        if (entry.filename[0] == '.'
            || ((entry.attr & (FAT32_ATTR_VOLUME_ID|FAT32_ATTR_HIDDEN|FAT32_ATTR_SYSTEM)) != 0))
        {
            continue;
        }

        // Determine entry type
        LogoEntryType type;

        if (entry.attr & FAT32_ATTR_DIRECTORY)
        {
            type = LOGO_ENTRY_DIRECTORY;
        }
        else if ((entry.attr & (FAT32_ATTR_VOLUME_ID|FAT32_ATTR_HIDDEN|FAT32_ATTR_SYSTEM)) == 0)
        {
            type = LOGO_ENTRY_FILE;
            
            // Apply extension filter for files
            if (filter && strcmp(filter, "*") != 0)
            {
                // Find the extension in the filename
                const char *dot = strrchr(entry.filename, '.');
                if (!dot || dot == entry.filename)
                {
                    continue; // No extension, skip
                }
                // Compare extension (case-insensitive)
                if (strcasecmp(dot + 1, filter) != 0)
                {
                    continue; // Extension doesn't match
                }
            }
        }
        else
        {
            continue; // Not a file or directory, skip
        }

        // Call callback
        if (!callback(entry.filename, type, user_data))
        {
            break; // Callback requested stop
        }
    }

    fat32_close(&dir);
    return true;
}

static const LogoStorageOps picocalc_storage_ops = {
    .open = logo_picocalc_file_open,
    .file_exists = logo_picocalc_file_exists,
    .dir_exists = logo_picocalc_dir_exists,
    .file_delete = logo_picocalc_file_delete,
    .dir_create = logo_picocalc_dir_create,
    .dir_delete = logo_picocalc_dir_delete,
    .rename = logo_picocalc_rename,
    .file_size = logo_picocalc_file_size,
    .list_directory = logo_picocalc_list_directory,
};


//
// LogoStorage lifecycle functions
//

LogoStorage *logo_picocalc_storage_create(void)
{
    LogoStorage *storage = (LogoStorage *)malloc(sizeof(LogoStorage));

    if (!storage)
    {
        return NULL;
    }

    logo_storage_init(storage, &picocalc_storage_ops);
    

    return storage;
}

void logo_picocalc_storage_destroy(LogoStorage *storage)
{
    if (!storage)
    {
        return;
    }

    free(storage);
}
