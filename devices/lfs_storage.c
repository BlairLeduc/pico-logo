//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  LittleFS-backed LogoStorage. See lfs_storage.h.
//

#include "lfs_storage.h"
#include "stream.h"
#include "lfs_backup.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h> // strcasecmp

// The mounted instance (one per process).
static lfs_t *g_lfs;

// Stream context: an open LittleFS file plus independent read/write positions
// (the Logo stream model reads and writes a file through separate cursors;
// writes default to appending at the original end of file).
typedef struct LfsStreamContext
{
    lfs_file_t file;
    long read_pos;
    long write_pos;
} LfsStreamContext;

//
// Stream operations
//

static int lfs_stream_read_char(LogoStream *stream)
{
    if (!g_lfs || !stream || !stream->context)
    {
        return -1;
    }
    LfsStreamContext *ctx = (LfsStreamContext *)stream->context;

    if (lfs_file_seek(g_lfs, &ctx->file, ctx->read_pos, LFS_SEEK_SET) < 0)
    {
        return -1;
    }
    unsigned char c;
    lfs_ssize_t n = lfs_file_read(g_lfs, &ctx->file, &c, 1);
    if (n == 1)
    {
        ctx->read_pos++;
        return c;
    }
    return -1;
}

static int lfs_stream_read_chars(LogoStream *stream, char *buffer, int count)
{
    if (!g_lfs || !stream || !stream->context || !buffer || count <= 0)
    {
        return -1;
    }
    LfsStreamContext *ctx = (LfsStreamContext *)stream->context;

    if (lfs_file_seek(g_lfs, &ctx->file, ctx->read_pos, LFS_SEEK_SET) < 0)
    {
        return -1;
    }
    lfs_ssize_t n = lfs_file_read(g_lfs, &ctx->file, buffer, (lfs_size_t)count);
    if (n < 0)
    {
        return -1;
    }
    ctx->read_pos += n;
    return (int)n;
}

static int lfs_stream_read_line(LogoStream *stream, char *buffer, size_t size)
{
    if (!g_lfs || !stream || !stream->context || !buffer || size == 0)
    {
        return -1;
    }
    LfsStreamContext *ctx = (LfsStreamContext *)stream->context;

    if (lfs_file_seek(g_lfs, &ctx->file, ctx->read_pos, LFS_SEEK_SET) < 0)
    {
        return -1;
    }

    size_t total = 0;
    bool line_ended = false;
    while (total < size - 1)
    {
        char ch;
        lfs_ssize_t n = lfs_file_read(g_lfs, &ctx->file, &ch, 1);
        if (n != 1)
        {
            break; // EOF or error
        }
        ctx->read_pos++;
        if (ch == '\n' || ch == '\r')
        {
            line_ended = true;
            break;
        }
        buffer[total++] = ch;
    }
    buffer[total] = '\0';
    return (total > 0 || line_ended) ? (int)total : -1;
}

static bool lfs_stream_can_read(LogoStream *stream)
{
    if (!g_lfs || !stream || !stream->context)
    {
        return false;
    }
    LfsStreamContext *ctx = (LfsStreamContext *)stream->context;
    return ctx->read_pos < (long)lfs_file_size(g_lfs, &ctx->file);
}

static void lfs_stream_write_bytes(LogoStream *stream, const char *buffer, size_t len)
{
    if (!g_lfs || !stream || !stream->context || !buffer)
    {
        return;
    }
    LfsStreamContext *ctx = (LfsStreamContext *)stream->context;

    // Only seek when the file is not already at write_pos. lfs_file_seek flushes
    // the write cache, so seeking before every write (as a streamed upload does,
    // one small chunk at a time) would force a flash flush per chunk -- slow and
    // heavy on copy-on-write block churn. Skipping it for sequential writes lets
    // LittleFS batch to block boundaries.
    if (lfs_file_tell(g_lfs, &ctx->file) != (lfs_soff_t)ctx->write_pos)
    {
        if (lfs_file_seek(g_lfs, &ctx->file, ctx->write_pos, LFS_SEEK_SET) < 0)
        {
            stream->write_error = true;
            return;
        }
    }
    lfs_ssize_t n = lfs_file_write(g_lfs, &ctx->file, buffer, (lfs_size_t)len);
    if (n < 0)
    {
        stream->write_error = true;
        return;
    }
    ctx->write_pos += n;
    if ((size_t)n < len)
    {
        stream->write_error = true;
    }
}

static void lfs_stream_write(LogoStream *stream, const char *text)
{
    if (!text)
    {
        return;
    }
    lfs_stream_write_bytes(stream, text, strlen(text));
}

static void lfs_stream_flush(LogoStream *stream)
{
    if (!g_lfs || !stream || !stream->context)
    {
        return;
    }
    LfsStreamContext *ctx = (LfsStreamContext *)stream->context;
    if (lfs_file_sync(g_lfs, &ctx->file) < 0)
    {
        stream->write_error = true;
    }
}

static long lfs_stream_get_read_pos(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return -1;
    }
    return ((LfsStreamContext *)stream->context)->read_pos;
}

static bool lfs_stream_set_read_pos(LogoStream *stream, long pos)
{
    if (!g_lfs || !stream || !stream->context || pos < 0)
    {
        return false;
    }
    LfsStreamContext *ctx = (LfsStreamContext *)stream->context;
    if (pos > (long)lfs_file_size(g_lfs, &ctx->file))
    {
        return false;
    }
    ctx->read_pos = pos;
    return true;
}

static long lfs_stream_get_write_pos(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return -1;
    }
    return ((LfsStreamContext *)stream->context)->write_pos;
}

static bool lfs_stream_set_write_pos(LogoStream *stream, long pos)
{
    if (!g_lfs || !stream || !stream->context || pos < 0)
    {
        return false;
    }
    LfsStreamContext *ctx = (LfsStreamContext *)stream->context;
    if (pos > (long)lfs_file_size(g_lfs, &ctx->file))
    {
        return false;
    }
    ctx->write_pos = pos;
    return true;
}

static long lfs_stream_get_length(LogoStream *stream)
{
    if (!g_lfs || !stream || !stream->context)
    {
        return -1;
    }
    return (long)lfs_file_size(g_lfs, &((LfsStreamContext *)stream->context)->file);
}

static void lfs_stream_close(LogoStream *stream)
{
    if (!stream || !stream->context)
    {
        return;
    }
    LfsStreamContext *ctx = (LfsStreamContext *)stream->context;
    if (g_lfs)
    {
        lfs_file_close(g_lfs, &ctx->file);
    }
    free(ctx);
    stream->context = NULL;
    stream->is_open = false;
}

static const LogoStreamOps lfs_stream_ops = {
    .read_char = lfs_stream_read_char,
    .read_chars = lfs_stream_read_chars,
    .read_line = lfs_stream_read_line,
    .can_read = lfs_stream_can_read,
    .write = lfs_stream_write,
    .write_bytes = lfs_stream_write_bytes,
    .flush = lfs_stream_flush,
    .get_read_pos = lfs_stream_get_read_pos,
    .set_read_pos = lfs_stream_set_read_pos,
    .get_write_pos = lfs_stream_get_write_pos,
    .set_write_pos = lfs_stream_set_write_pos,
    .get_length = lfs_stream_get_length,
    .close = lfs_stream_close,
};

//
// Storage operations
//

static LogoStream *lfs_storage_open(const char *pathname)
{
    if (!g_lfs || !pathname)
    {
        return NULL;
    }

    LfsStreamContext *ctx = (LfsStreamContext *)malloc(sizeof(LfsStreamContext));
    if (!ctx)
    {
        return NULL;
    }

    // Open read/write, creating the file if it does not yet exist. A directory
    // path yields LFS_ERR_ISDIR and is rejected.
    int err = lfs_file_open(g_lfs, &ctx->file, pathname,
                            LFS_O_RDWR | LFS_O_CREAT);
    if (err < 0)
    {
        free(ctx);
        return NULL;
    }
    ctx->read_pos = 0;
    ctx->write_pos = (long)lfs_file_size(g_lfs, &ctx->file);

    LogoStream *stream = (LogoStream *)malloc(sizeof(LogoStream));
    if (!stream)
    {
        lfs_file_close(g_lfs, &ctx->file);
        free(ctx);
        return NULL;
    }

    stream->type = LOGO_STREAM_FILE;
    stream->ops = &lfs_stream_ops;
    stream->context = ctx;
    stream->is_open = true;
    stream->write_error = false;
    strncpy(stream->name, pathname, LOGO_STREAM_NAME_MAX - 1);
    stream->name[LOGO_STREAM_NAME_MAX - 1] = '\0';
    return stream;
}

static bool lfs_storage_file_exists(const char *pathname)
{
    if (!g_lfs || !pathname)
    {
        return false;
    }
    struct lfs_info info;
    return lfs_stat(g_lfs, pathname, &info) >= 0 && info.type == LFS_TYPE_REG;
}

static bool lfs_storage_dir_exists(const char *pathname)
{
    if (!g_lfs || !pathname)
    {
        return false;
    }
    // Root always exists.
    if (pathname[0] == '\0' || (pathname[0] == '/' && pathname[1] == '\0'))
    {
        return true;
    }
    struct lfs_info info;
    return lfs_stat(g_lfs, pathname, &info) >= 0 && info.type == LFS_TYPE_DIR;
}

static bool lfs_storage_file_delete(const char *pathname)
{
    if (!g_lfs || !pathname)
    {
        return false;
    }
    struct lfs_info info;
    if (lfs_stat(g_lfs, pathname, &info) < 0 || info.type != LFS_TYPE_REG)
    {
        return false; // not a regular file
    }
    return lfs_remove(g_lfs, pathname) >= 0;
}

static bool lfs_storage_dir_create(const char *pathname)
{
    if (!g_lfs || !pathname)
    {
        return false;
    }
    return lfs_mkdir(g_lfs, pathname) >= 0;
}

static bool lfs_storage_dir_delete(const char *pathname)
{
    if (!g_lfs || !pathname)
    {
        return false;
    }
    struct lfs_info info;
    if (lfs_stat(g_lfs, pathname, &info) < 0 || info.type != LFS_TYPE_DIR)
    {
        return false; // not a directory
    }
    // lfs_remove fails with LFS_ERR_NOTEMPTY on a non-empty directory.
    return lfs_remove(g_lfs, pathname) >= 0;
}

static bool lfs_storage_rename(const char *old_path, const char *new_path)
{
    if (!g_lfs || !old_path || !new_path)
    {
        return false;
    }
    return lfs_rename(g_lfs, old_path, new_path) >= 0;
}

static long lfs_storage_file_size(const char *pathname)
{
    if (!g_lfs || !pathname)
    {
        return -1;
    }
    struct lfs_info info;
    if (lfs_stat(g_lfs, pathname, &info) < 0 || info.type != LFS_TYPE_REG)
    {
        return -1;
    }
    return (long)info.size;
}

static bool lfs_storage_list_directory(const char *pathname,
                                       LogoDirCallback callback, void *user_data,
                                       const char *filter)
{
    if (!g_lfs || !pathname || !callback)
    {
        return false;
    }

    lfs_dir_t dir;
    if (lfs_dir_open(g_lfs, &dir, pathname) < 0)
    {
        return false;
    }

    struct lfs_info info;
    int rc;
    while ((rc = lfs_dir_read(g_lfs, &dir, &info)) > 0)
    {
        // Skip "." and ".." pseudo-entries.
        if (info.name[0] == '.' &&
            (info.name[1] == '\0' || (info.name[1] == '.' && info.name[2] == '\0')))
        {
            continue;
        }

        LogoEntryType type =
            (info.type == LFS_TYPE_DIR) ? LOGO_ENTRY_DIRECTORY : LOGO_ENTRY_FILE;

        // Extension filter applies to files only (mirrors the FAT backend).
        if (type == LOGO_ENTRY_FILE && filter && strcmp(filter, "*") != 0)
        {
            const char *dot = strrchr(info.name, '.');
            if (!dot || dot == info.name || strcasecmp(dot + 1, filter) != 0)
            {
                continue;
            }
        }

        if (!callback(info.name, type, user_data))
        {
            break;
        }
    }

    lfs_dir_close(g_lfs, &dir);
    return true;
}

static bool lfs_storage_free_blocks(const char *pathname, uint32_t *free_blocks,
                                    uint32_t *block_size)
{
    (void)pathname;
    if (!g_lfs)
    {
        return false;
    }
    lfs_ssize_t used = lfs_fs_size(g_lfs);
    if (used < 0)
    {
        return false;
    }
    struct lfs_fsinfo info;
    if (lfs_fs_stat(g_lfs, &info) < 0)
    {
        return false;
    }
    if (free_blocks)
    {
        *free_blocks = (info.block_count > (lfs_size_t)used)
                           ? (uint32_t)(info.block_count - (lfs_size_t)used)
                           : 0;
    }
    if (block_size)
    {
        *block_size = (uint32_t)info.block_size;
    }
    return true;
}

static bool lfs_storage_mount_available(const char *pathname)
{
    (void)pathname;
    return g_lfs != NULL;
}

static bool lfs_storage_fs_image_backup(LogoStream *out)
{
    return g_lfs != NULL && logo_lfs_backup(g_lfs, out);
}

static bool lfs_storage_fs_image_restore(LogoStream *in)
{
    return g_lfs != NULL && logo_lfs_restore(g_lfs, in);
}

static const LogoStorageOps lfs_storage_ops = {
    .open = lfs_storage_open,
    .file_exists = lfs_storage_file_exists,
    .dir_exists = lfs_storage_dir_exists,
    .file_delete = lfs_storage_file_delete,
    .dir_create = lfs_storage_dir_create,
    .dir_delete = lfs_storage_dir_delete,
    .rename = lfs_storage_rename,
    .file_size = lfs_storage_file_size,
    .list_directory = lfs_storage_list_directory,
    .free_blocks = lfs_storage_free_blocks,
    .mount_available = lfs_storage_mount_available,
    .fs_image_backup = lfs_storage_fs_image_backup,
    .fs_image_restore = lfs_storage_fs_image_restore,
};

void logo_lfs_storage_init(LogoStorage *storage, lfs_t *lfs)
{
    g_lfs = lfs;
    logo_storage_init(storage, &lfs_storage_ops);
}
