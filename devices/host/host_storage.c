//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements the LogoStorage interface for host (desktop) device.
//

#include "host_storage.h"
#include "../storage.h"
#include "../stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <strings.h>  // for strcasecmp

//
// File stream context - wraps a FILE*
//
typedef struct FileContext
{
    FILE *file;
} FileContext;

//
// Stream operation implementations
//

static int host_file_read_char(LogoStream *stream)
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

    int ch = fgetc(ctx->file);
    return (ch == EOF) ? -1 : ch;
}

static int host_file_read_chars(LogoStream *stream, char *buffer, int count)
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

    size_t read = fread(buffer, 1, (size_t)count, ctx->file);
    return (int)read;
}

static int host_file_read_line(LogoStream *stream, char *buffer, size_t size)
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

    if (fgets(buffer, (int)size, ctx->file) == NULL)
    {
        return -1; // EOF or error
    }

    return (int)strlen(buffer);
}

static bool host_file_can_read(LogoStream *stream)
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

    // Check if we're at EOF
    int ch = fgetc(ctx->file);
    if (ch == EOF)
    {
        return false;
    }
    ungetc(ch, ctx->file);
    return true;
}

static void host_file_write(LogoStream *stream, const char *text)
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

    fputs(text, ctx->file);
}

static void host_file_flush(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return;
    }

    FileContext *ctx = (FileContext *)stream->context;
    if (ctx->file)
    {
        fflush(ctx->file);
    }
}

static long host_file_get_read_pos(LogoStream *stream)
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

    return ftell(ctx->file);
}

static bool host_file_set_read_pos(LogoStream *stream, long pos)
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

    return fseek(ctx->file, pos, SEEK_SET) == 0;
}

static long host_file_get_write_pos(LogoStream *stream)
{
    // For simple files, read and write position are the same
    return host_file_get_read_pos(stream);
}

static bool host_file_set_write_pos(LogoStream *stream, long pos)
{
    // For simple files, read and write position are the same
    return host_file_set_read_pos(stream, pos);
}

static long host_file_get_length(LogoStream *stream)
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

    // Save current position
    long current = ftell(ctx->file);
    if (current < 0)
    {
        return -1;
    }

    // Seek to end
    if (fseek(ctx->file, 0, SEEK_END) != 0)
    {
        return -1;
    }

    // Get length
    long length = ftell(ctx->file);

    // Restore position
    fseek(ctx->file, current, SEEK_SET);

    return length;
}

static void host_file_close(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return;
    }

    FileContext *ctx = (FileContext *)stream->context;
    if (ctx->file)
    {
        fclose(ctx->file);
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
static const LogoStreamOps host_file_ops = {
    .read_char = host_file_read_char,
    .read_chars = host_file_read_chars,
    .read_line = host_file_read_line,
    .can_read = host_file_can_read,
    .write = host_file_write,
    .flush = host_file_flush,
    .get_read_pos = host_file_get_read_pos,
    .set_read_pos = host_file_set_read_pos,
    .get_write_pos = host_file_get_write_pos,
    .set_write_pos = host_file_set_write_pos,
    .get_length = host_file_get_length,
    .close = host_file_close,
};

//
// Storage API implementation
//

static LogoStream *logo_host_file_open(const char *pathname)
{
    if (!pathname)
    {
        return NULL;
    }

    // Try to open existing file for read/write
    FILE *file = fopen(pathname, "r+");
    if (!file)
    {
        // Try to create the file
        file = fopen(pathname, "w+");
    }
    if (!file)
    {
        return NULL;
    }

    // Allocate context
    FileContext *ctx = (FileContext *)malloc(sizeof(FileContext));
    if (!ctx)
    {
        fclose(file);
        return NULL;
    }
    ctx->file = file;

    // Allocate stream
    LogoStream *stream = (LogoStream *)malloc(sizeof(LogoStream));
    if (!stream)
    {
        fclose(file);
        free(ctx);
        return NULL;
    }

    // Initialize stream
    stream->type = LOGO_STREAM_FILE;
    stream->ops = &host_file_ops;
    stream->context = ctx;
    stream->is_open = true;

    // Copy pathname to stream name
    strncpy(stream->name, pathname, LOGO_STREAM_NAME_MAX - 1);
    stream->name[LOGO_STREAM_NAME_MAX - 1] = '\0';

    return stream;
}

static bool logo_host_file_exists(const char *pathname)
{
    if (!pathname)
    {
        return false;
    }

    struct stat st;
    if (stat(pathname, &st) != 0)
    {
        return false;
    }

    return S_ISREG(st.st_mode);
}

static bool logo_host_dir_exists(const char *pathname)
{
    if (!pathname)
    {
        return false;
    }

    struct stat st;
    if (stat(pathname, &st) != 0)
    {
        return false;
    }

    return S_ISDIR(st.st_mode);
}

static bool logo_host_file_delete(const char *pathname)
{
    if (!pathname)
    {
        return false;
    }

    return remove(pathname) == 0;
}

static bool logo_host_dir_create(const char *pathname)
{
    if (!pathname)
    {
        return false;
    }

#ifdef _WIN32
    return mkdir(pathname) == 0;
#else
    return mkdir(pathname, 0755) == 0;
#endif
}

static bool logo_host_dir_delete(const char *pathname)
{
    if (!pathname)
    {
        return false;
    }

    return rmdir(pathname) == 0;
}

static bool logo_host_rename(const char *old_path, const char *new_path)
{
    if (!old_path || !new_path)
    {
        return false;
    }

    return rename(old_path, new_path) == 0;
}

static long logo_host_file_size(const char *pathname)
{
    if (!pathname)
    {
        return -1;
    }

    struct stat st;
    if (stat(pathname, &st) != 0)
    {
        return -1;
    }

    return (long)st.st_size;
}

static bool logo_host_list_directory(const char *pathname, LogoDirCallback callback,
                                     void *user_data, const char *filter)
{
    if (!pathname || !callback)
    {
        return false;
    }

    DIR *dir = opendir(pathname);
    if (!dir)
    {
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // Determine entry type
        LogoEntryType type;
        
        // Build full path to check type
        char full_path[1024];
        size_t dir_len = strlen(pathname);
        size_t name_len = strlen(entry->d_name);
        
        if (dir_len + 1 + name_len >= sizeof(full_path))
        {
            continue; // Path too long, skip
        }
        
        strcpy(full_path, pathname);
        if (pathname[dir_len - 1] != '/')
        {
            strcat(full_path, "/");
        }
        strcat(full_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0)
        {
            continue; // Can't stat, skip
        }

        if (S_ISDIR(st.st_mode))
        {
            type = LOGO_ENTRY_DIRECTORY;
        }
        else if (S_ISREG(st.st_mode))
        {
            type = LOGO_ENTRY_FILE;
            
            // Apply extension filter for files
            if (filter && strcmp(filter, "*") != 0)
            {
                // Find the extension in the filename
                const char *dot = strrchr(entry->d_name, '.');
                if (!dot || dot == entry->d_name)
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
        if (!callback(entry->d_name, type, user_data))
        {
            break; // Callback requested stop
        }
    }

    closedir(dir);
    return true;
}

//
// Storage operations table
//
static const LogoStorageOps host_storage_ops = {
    .open = logo_host_file_open,
    .file_exists = logo_host_file_exists,
    .dir_exists = logo_host_dir_exists,
    .file_delete = logo_host_file_delete,
    .dir_create = logo_host_dir_create,
    .dir_delete = logo_host_dir_delete,
    .rename = logo_host_rename,
    .file_size = logo_host_file_size,
    .list_directory = logo_host_list_directory,
};

//
// LogoStorage lifecycle functions
//

LogoStorage *logo_host_storage_create(void)
{
    LogoStorage *storage = (LogoStorage *)malloc(sizeof(LogoStorage));

    if (!storage)
    {
        return NULL;
    }

    logo_storage_init(storage, &host_storage_ops);

    return storage;
}

void logo_host_storage_destroy(LogoStorage *storage)
{
    if (!storage)
    {
        return;
    }

    free(storage);
}
