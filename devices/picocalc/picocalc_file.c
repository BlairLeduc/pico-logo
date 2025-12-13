//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Host (desktop) file stream implementation using standard C FILE*.
//

#include "picocalc_file.h"

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

static int host_file_read_char(LogoStream *stream)
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

static int host_file_read_chars(LogoStream *stream, char *buffer, int count)
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

static int host_file_read_line(LogoStream *stream, char *buffer, size_t size)
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

static bool host_file_can_read(LogoStream *stream)
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

static void host_file_write(LogoStream *stream, const char *text)
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

static void host_file_flush(LogoStream *stream)
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

static long host_file_get_read_pos(LogoStream *stream)
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

static bool host_file_set_read_pos(LogoStream *stream, long pos)
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

static void host_file_close(LogoStream *stream)
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
// Public API
//

LogoStream *logo_host_file_open(const char *pathname, LogoFileMode mode)
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

    // Open the file
    FILE *file = fopen(pathname, fmode);
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
    stream->ops = &host_file_ops;
    stream->context = ctx;
    stream->is_open = true;

    // Copy pathname to stream name
    strncpy(stream->name, pathname, LOGO_STREAM_NAME_MAX - 1);
    stream->name[LOGO_STREAM_NAME_MAX - 1] = '\0';

    return stream;
}

bool logo_host_file_exists(const char *pathname)
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

bool logo_host_dir_exists(const char *pathname)
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

bool logo_host_file_delete(const char *pathname)
{
    if (!pathname)
    {
        return false;
    }

    return remove(pathname) == 0;
}

bool logo_host_dir_delete(const char *pathname)
{
    if (!pathname)
    {
        return false;
    }

    return rmdir(pathname) == 0;
}

bool logo_host_rename(const char *old_path, const char *new_path)
{
    if (!old_path || !new_path)
    {
        return false;
    }

    return rename(old_path, new_path) == 0;
}

long logo_host_file_size(const char *pathname)
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
