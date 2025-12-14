//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements the LogoStorage interface for PicoCalc device.
//

#include "../storage.h"
#include "picocalc_storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

//
// File stream context - wraps a FILE*
//
typedef struct HostFileContext
{
    FILE *file;
    LogoFileMode mode;
} HostFileContext;

//
// Stream operation implementations
//

static int picocalc_file_read_char(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return -1;
    }

    HostFileContext *ctx = (HostFileContext *)stream->context;
    if (!ctx->file)
    {
        return -1;
    }

    int ch = fgetc(ctx->file);
    return (ch == EOF) ? -1 : ch;
}

static int picocalc_file_read_chars(LogoStream *stream, char *buffer, int count)
{
    if (!stream || !stream->context || !buffer || count <= 0)
    {
        return -1;
    }

    HostFileContext *ctx = (HostFileContext *)stream->context;
    if (!ctx->file)
    {
        return -1;
    }

    size_t read = fread(buffer, 1, (size_t)count, ctx->file);
    return (int)read;
}

static int picocalc_file_read_line(LogoStream *stream, char *buffer, size_t size)
{
    if (!stream || !stream->context || !buffer || size == 0)
    {
        return -1;
    }

    HostFileContext *ctx = (HostFileContext *)stream->context;
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

static bool picocalc_file_can_read(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return false;
    }

    HostFileContext *ctx = (HostFileContext *)stream->context;
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

static void picocalc_file_write(LogoStream *stream, const char *text)
{
    if (!stream || !stream->context || !text)
    {
        return;
    }

    HostFileContext *ctx = (HostFileContext *)stream->context;
    if (!ctx->file)
    {
        return;
    }

    fputs(text, ctx->file);
}

static void picocalc_file_flush(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return;
    }

    HostFileContext *ctx = (HostFileContext *)stream->context;
    if (ctx->file)
    {
        fflush(ctx->file);
    }
}

static long picocalc_file_get_read_pos(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return -1;
    }

    HostFileContext *ctx = (HostFileContext *)stream->context;
    if (!ctx->file)
    {
        return -1;
    }

    return ftell(ctx->file);
}

static bool picocalc_file_set_read_pos(LogoStream *stream, long pos)
{
    if (!stream || !stream->context)
    {
        return false;
    }

    HostFileContext *ctx = (HostFileContext *)stream->context;
    if (!ctx->file)
    {
        return false;
    }

    return fseek(ctx->file, pos, SEEK_SET) == 0;
}

static long picocalc_file_get_write_pos(LogoStream *stream)
{
    // For simple files, read and write position are the same
    return picocalc_file_get_read_pos(stream);
}

static bool picocalc_file_set_write_pos(LogoStream *stream, long pos)
{
    // For simple files, read and write position are the same
    return picocalc_file_set_read_pos(stream, pos);
}

static long picocalc_file_get_length(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return -1;
    }

    HostFileContext *ctx = (HostFileContext *)stream->context;
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

static void picocalc_file_close(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return;
    }

    HostFileContext *ctx = (HostFileContext *)stream->context;
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

static LogoStream *logo_picocalc_file_open(const char *pathname, LogoFileMode mode)
{
    if (!pathname)
    {
        return NULL;
    }

    // Determine the fopen mode string
    const char *fmode;
    switch (mode)
    {
    case LOGO_FILE_READ:
        fmode = "r";
        break;
    case LOGO_FILE_WRITE:
        fmode = "w";
        break;
    case LOGO_FILE_APPEND:
        fmode = "a";
        break;
    case LOGO_FILE_UPDATE:
        fmode = "r+";
        break;
    default:
        return NULL;
    }

    // Force all file operations to be within the LOGO_STORAGE_ROOT directory
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", LOGO_STORAGE_ROOT, pathname);

    // Open the file
    FILE *file = fopen(full_path, fmode);
    if (!file)
    {
        return NULL;
    }

    // Allocate context
    HostFileContext *ctx = (HostFileContext *)malloc(sizeof(HostFileContext));
    if (!ctx)
    {
        fclose(file);
        return NULL;
    }
    ctx->file = file;
    ctx->mode = mode;

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

    // Force all file operations to be within the LOGO_STORAGE_ROOT directory
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", LOGO_STORAGE_ROOT, pathname);

    struct stat st;
    if (stat(full_path, &st) != 0)
    {
        return false;
    }

    return S_ISREG(st.st_mode);
}

static bool logo_picocalc_dir_exists(const char *pathname)
{
    if (!pathname)
    {
        return false;
    }

    // Force all file operations to be within the LOGO_STORAGE_ROOT directory
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", LOGO_STORAGE_ROOT, pathname);


    struct stat st;
    if (stat(full_path, &st) != 0)
    {
        return false;
    }

    return S_ISDIR(st.st_mode);
}

static bool logo_picocalc_file_delete(const char *pathname)
{
    if (!pathname)
    {
        return false;
    }

    // Force all file operations to be within the LOGO_STORAGE_ROOT directory
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", LOGO_STORAGE_ROOT, pathname);

    return remove(full_path) == 0;
}

static bool logo_picocalc_dir_delete(const char *pathname)
{
    if (!pathname)
    {
        return false;
    }

    // Force all file operations to be within the LOGO_STORAGE_ROOT directory
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", LOGO_STORAGE_ROOT, pathname);

    return unlink(full_path) == 0;
}

static bool logo_picocalc_rename(const char *old_path, const char *new_path)
{
    if (!old_path || !new_path)
    {
        return false;
    }

    // Force all file operations to be within the LOGO_STORAGE_ROOT directory
    char full_old_path[512], full_new_path[512];
    snprintf(full_old_path, sizeof(full_old_path), "%s%s", LOGO_STORAGE_ROOT, old_path);
    snprintf(full_new_path, sizeof(full_new_path), "%s%s", LOGO_STORAGE_ROOT, new_path);

    return rename(full_old_path, full_new_path) == 0;
}

static long logo_picocalc_file_size(const char *pathname)
{
    if (!pathname)
    {
        return -1;
    }

    // Force all file operations to be within the LOGO_STORAGE_ROOT directory
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", LOGO_STORAGE_ROOT, pathname);

    struct stat st;
    if (stat(full_path, &st) != 0)
    {
        return -1;
    }

    return (long)st.st_size;
}

static const LogoStorageOps picocalc_storage_ops = {
    .open = logo_picocalc_file_open,
    .file_exists = logo_picocalc_file_exists,
    .dir_exists = logo_picocalc_dir_exists,
    .file_delete = logo_picocalc_file_delete,
    .dir_delete = logo_picocalc_dir_delete,
    .rename = logo_picocalc_rename,
    .file_size = logo_picocalc_file_size,
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
