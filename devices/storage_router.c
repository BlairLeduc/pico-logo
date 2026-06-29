//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Storage router implementation. See storage_router.h.
//

#include "storage_router.h"
#include "stream.h"

#include <stdlib.h>
#include <string.h>

// Module state: the two backends and the SD availability predicate.
static const LogoStorageOps *g_root_ops;
static const LogoStorageOps *g_sd_ops;
static LogoMountAvailableFn g_sd_available;

// If `path` is under the "/sd" mount, return the SD-relative remainder:
//   "/sd"      -> "/"        (the mount root)
//   "/sd/foo"  -> "/foo"
// Otherwise return NULL (route to root). The "sd" segment is matched
// case-insensitively and only as a whole component ("/sdcard" does NOT match).
static const char *sd_subpath(const char *path)
{
    if (!path || path[0] != '/')
    {
        return NULL;
    }
    char c1 = path[1], c2 = path[2];
    bool is_sd = (c1 == 's' || c1 == 'S') && (c2 == 'd' || c2 == 'D');
    if (!is_sd)
    {
        return NULL;
    }
    if (path[3] == '\0')
    {
        return "/"; // exactly "/sd" -> SD root
    }
    if (path[3] == '/')
    {
        return path + 3; // "/sd/foo" -> "/foo"
    }
    return NULL; // e.g. "/sdcard"
}

// True for the root directory path ("" or "/").
static bool is_root(const char *path)
{
    return path && (path[0] == '\0' || (path[0] == '/' && path[1] == '\0'));
}

static LogoStream *router_open(const char *pathname)
{
    const char *sub = sd_subpath(pathname);
    if (sub)
    {
        LogoStream *s = g_sd_ops->open(sub);
        if (s)
        {
            // The io layer tracks open streams by their full (routed) path, so
            // restore the original "/sd/..." name the backend didn't see.
            strncpy(s->name, pathname, LOGO_STREAM_NAME_MAX - 1);
            s->name[LOGO_STREAM_NAME_MAX - 1] = '\0';
        }
        return s;
    }
    return g_root_ops->open(pathname);
}

static bool router_file_exists(const char *pathname)
{
    const char *sub = sd_subpath(pathname);
    return sub ? g_sd_ops->file_exists(sub) : g_root_ops->file_exists(pathname);
}

static bool router_dir_exists(const char *pathname)
{
    const char *sub = sd_subpath(pathname);
    return sub ? g_sd_ops->dir_exists(sub) : g_root_ops->dir_exists(pathname);
}

static bool router_file_delete(const char *pathname)
{
    const char *sub = sd_subpath(pathname);
    return sub ? g_sd_ops->file_delete(sub) : g_root_ops->file_delete(pathname);
}

static bool router_dir_create(const char *pathname)
{
    const char *sub = sd_subpath(pathname);
    return sub ? g_sd_ops->dir_create(sub) : g_root_ops->dir_create(pathname);
}

static bool router_dir_delete(const char *pathname)
{
    const char *sub = sd_subpath(pathname);
    return sub ? g_sd_ops->dir_delete(sub) : g_root_ops->dir_delete(pathname);
}

// Move a single file from one backend to another by streamed copy, then delete
// the source. Files only — cross-filesystem directory moves are not supported
// (decision #6). On any failure the source is left intact and a partial
// destination is removed, so the move is all-or-nothing from the user's view.
static bool cross_fs_move(const LogoStorageOps *src_ops, const char *src,
                          const LogoStorageOps *dst_ops, const char *dst)
{
    // The source must be an existing regular file. file_exists() is false for a
    // missing path and for a directory, so this both reports "not found" for a
    // bad source and rejects directory moves — and it stops the backend's
    // create-on-open from silently fabricating an empty source.
    if (!src_ops->file_exists(src))
    {
        return false;
    }
    if (dst_ops->dir_exists(dst))
    {
        return false; // never overwrite a directory with a file
    }
    // Overwrite an existing destination file cleanly (open() would otherwise
    // position writes at end-of-file and append).
    if (dst_ops->file_exists(dst))
    {
        dst_ops->file_delete(dst);
    }

    LogoStream *in = src_ops->open(src);
    if (!in)
    {
        return false;
    }
    LogoStream *out = dst_ops->open(dst);
    if (!out)
    {
        logo_stream_close(in);
        free(in);
        return false;
    }
    // Start from a known-clean write-error state: a freshly opened stream may
    // carry a stale flag from a backend that does not initialise it, and we use
    // this flag below to detect a failed copy.
    logo_stream_clear_write_error(out);

    char buf[257];
    bool ok = true;
    int n;
    while ((n = logo_stream_read_chars(in, buf, (int)sizeof(buf) - 1)) > 0)
    {
        buf[n] = '\0';
        logo_stream_write(out, buf);
        if (logo_stream_has_write_error(out))
        {
            ok = false;
            break;
        }
    }
    if (n < 0)
    {
        ok = false; // read error mid-copy
    }
    logo_stream_flush(out);
    logo_stream_close(in);
    free(in);
    logo_stream_close(out);
    free(out);

    if (!ok)
    {
        dst_ops->file_delete(dst); // roll back the partial destination
        return false;
    }
    return src_ops->file_delete(src); // remove source only after a clean copy
}

static bool router_rename(const char *old_path, const char *new_path)
{
    const char *o = sd_subpath(old_path);
    const char *n = sd_subpath(new_path);
    if ((o != NULL) != (n != NULL))
    {
        // Cross-mount move: copy across filesystems then delete the source.
        const LogoStorageOps *src_ops = o ? g_sd_ops : g_root_ops;
        const LogoStorageOps *dst_ops = n ? g_sd_ops : g_root_ops;
        const char *src = o ? o : old_path;
        const char *dst = n ? n : new_path;
        return cross_fs_move(src_ops, src, dst_ops, dst);
    }
    if (o)
    {
        return g_sd_ops->rename(o, n);
    }
    return g_root_ops->rename(old_path, new_path);
}

static long router_file_size(const char *pathname)
{
    const char *sub = sd_subpath(pathname);
    return sub ? g_sd_ops->file_size(sub) : g_root_ops->file_size(pathname);
}

static bool router_list_directory(const char *pathname, LogoDirCallback callback,
                                  void *user_data, const char *filter)
{
    const char *sub = sd_subpath(pathname);
    if (sub)
    {
        return g_sd_ops->list_directory(sub, callback, user_data, filter);
    }

    // Listing the root: surface the SD mount as a synthetic "sd" subdirectory,
    // but only when a card is actually present.
    if (is_root(pathname) && g_sd_available && g_sd_available())
    {
        if (!callback(LOGO_SD_MOUNT_NAME, LOGO_ENTRY_DIRECTORY, user_data))
        {
            return true; // callback asked to stop before the real entries
        }
    }
    return g_root_ops->list_directory(pathname, callback, user_data, filter);
}

static const LogoStorageOps router_ops = {
    .open = router_open,
    .file_exists = router_file_exists,
    .dir_exists = router_dir_exists,
    .file_delete = router_file_delete,
    .dir_create = router_dir_create,
    .dir_delete = router_dir_delete,
    .rename = router_rename,
    .file_size = router_file_size,
    .list_directory = router_list_directory,
};

void logo_storage_router_init(LogoStorage *router,
                              const LogoStorageOps *root_ops,
                              const LogoStorageOps *sd_ops,
                              LogoMountAvailableFn sd_available)
{
    g_root_ops = root_ops;
    g_sd_ops = sd_ops;
    g_sd_available = sd_available;
    logo_storage_init(router, &router_ops);
}
