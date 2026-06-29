//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Storage router implementation. See storage_router.h.
//

#include "storage_router.h"
#include "stream.h"

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

static bool router_rename(const char *old_path, const char *new_path)
{
    const char *o = sd_subpath(old_path);
    const char *n = sd_subpath(new_path);
    if ((o != NULL) != (n != NULL))
    {
        // Cross-mount move: rejected here. The cross-filesystem copy+delete is a
        // later phase (see docs section 7).
        return false;
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
