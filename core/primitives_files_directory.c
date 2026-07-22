//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  File directory primitives: files, directories, cat, catalog, setprefix,
//                             prefix, erasefile, erasedir, createdir, file?,
//                             dir?, rename
//

#include "primitives.h"
#include "error.h"
#include "format.h"
#include "limits.h"
#include "devices/io.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

//==========================================================================
// Directory listing primitives: files, directories, cat, catalog
//==========================================================================

// Context for building file list
typedef struct FileListContext
{
    Node list;       // The list being built (in reverse order)
    bool files_only; // If true, only include files; if false, only directories
} FileListContext;

// Callback for building file/directory list
static bool file_list_callback(const char *name, LogoEntryType type, void *user_data)
{
    FileListContext *ctx = (FileListContext *)user_data;
    
    // Filter by type
    if (ctx->files_only && type != LOGO_ENTRY_FILE)
    {
        return true; // Skip directories
    }
    if (!ctx->files_only && type != LOGO_ENTRY_DIRECTORY)
    {
        return true; // Skip files
    }

    // Add to list
    Node name_node = mem_atom_cstr(name);
    ctx->list = mem_cons(name_node, ctx->list);
    
    return true; // Continue
}

// files - outputs a list of file names in the current directory
// (files ext) - outputs files with the specified extension
static Result prim_files(Evaluator *eval, int argc, Value *args)
{
    (void)eval;

    const char *filter = NULL;
    
    // Check for optional extension argument
    if (argc >= 1)
    {
        if (!value_is_word(args[0]))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
        }
        filter = mem_word_ptr(args[0].as.node);
    }

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_list(NODE_NIL));
    }

    // Get current directory (use prefix if set, otherwise ".")
    const char *dir = logo_io_get_prefix(io);
    if (!dir || dir[0] == '\0')
    {
        dir = ".";
    }

    FileListContext ctx = {NODE_NIL, true};
    
    if (!logo_io_list_directory(io, dir, file_list_callback, &ctx, filter))
    {
        return result_ok(value_list(NODE_NIL));
    }

    return result_ok(value_list(ctx.list));
}

// directories - outputs a list of directory names in the current directory
static Result prim_directories(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_list(NODE_NIL));
    }

    // Get current directory (use prefix if set, otherwise ".")
    const char *dir = logo_io_get_prefix(io);
    if (!dir || dir[0] == '\0')
    {
        dir = ".";
    }

    FileListContext ctx = {NODE_NIL, false};
    
    if (!logo_io_list_directory(io, dir, file_list_callback, &ctx, NULL))
    {
        return result_ok(value_list(NODE_NIL));
    }

    return result_ok(value_list(ctx.list));
}

// Context for cat / catalog - collects entries then sorts them
#define CATALOG_MAX_ENTRIES 256

typedef struct CatalogEntry
{
    char name[LOGO_STREAM_NAME_MAX];
    bool is_directory;
} CatalogEntry;

typedef struct CatalogContext
{
    CatalogEntry entries[CATALOG_MAX_ENTRIES];
    int count;
} CatalogContext;

// Callback for collecting catalog entries
static bool catalog_callback(const char *name, LogoEntryType type, void *user_data)
{
    CatalogContext *ctx = (CatalogContext *)user_data;

    if (ctx->count >= CATALOG_MAX_ENTRIES)
    {
        return false; // Stop - too many entries
    }

    CatalogEntry *entry = &ctx->entries[ctx->count++];
    strncpy(entry->name, name, LOGO_STREAM_NAME_MAX - 1);
    entry->name[LOGO_STREAM_NAME_MAX - 1] = '\0';
    entry->is_directory = (type == LOGO_ENTRY_DIRECTORY);

    return true;
}

// Compare function for sorting catalog entries alphabetically
static int catalog_compare(const void *a, const void *b)
{
    const CatalogEntry *ea = (const CatalogEntry *)a;
    const CatalogEntry *eb = (const CatalogEntry *)b;
    return strcasecmp(ea->name, eb->name);
}

// Resolves the directory to list from the optional pathname argument. On
// success returns true and sets *out_dir to point at resolved_path or ".".
// On failure returns false with the error in *err.
static bool catalog_resolve_dir(LogoIO *io, int argc, Value *args,
                                char *resolved_path, size_t resolved_size,
                                const char **out_dir, Result *err)
{
    if (argc >= 1)
    {
        if (!value_is_word(args[0]))
        {
            *err = result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
            return false;
        }
        const char *pathname = mem_word_ptr(args[0].as.node);

        // Resolve against the prefix via the shared helper, which normalizes
        // ".." and returns NULL rather than silently truncating an over-long path.
        char *resolved = logo_io_resolve_path(io, pathname, resolved_path, resolved_size);
        if (!resolved)
        {
            *err = result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
            return false;
        }
        *out_dir = resolved;
    }
    else
    {
        // No argument - use the current directory (prefix if set, otherwise ".")
        const char *dir = logo_io_get_prefix(io);
        if (!dir || dir[0] == '\0')
        {
            dir = ".";
        }
        *out_dir = dir;
    }
    return true;
}

// Collects the entries of dir into ctx, sorted alphabetically. Returns false if
// the directory could not be listed.
static bool catalog_collect(LogoIO *io, const char *dir, CatalogContext *ctx)
{
    ctx->count = 0;
    if (!logo_io_list_directory(io, dir, catalog_callback, ctx, "*"))
    {
        return false;
    }
    if (ctx->count > 0)
    {
        qsort(ctx->entries, ctx->count, sizeof(CatalogEntry), catalog_compare);
    }
    return true;
}

// Looks up the byte size of an entry within dir. Returns -1 when unknown.
static long catalog_entry_size(LogoIO *io, const char *dir, const CatalogEntry *entry)
{
    if (!io->storage || !io->storage->ops->file_size)
    {
        return -1;
    }

    // Big enough for a resolved directory (the caller's 256-byte buffer) plus a
    // "/" and an entry name. A truncated path could stat an unrelated ancestor
    // directory, so bail if the join would not fit.
    char path[256 + LOGO_STREAM_NAME_MAX];
    int n;
    if (entry->name[0] == '/')
    {
        // Already an absolute path (some backends list full paths).
        n = snprintf(path, sizeof(path), "%s", entry->name);
    }
    else if (dir[0] == '.' && dir[1] == '\0')
    {
        // "." means the filesystem root; storage paths are absolute.
        n = snprintf(path, sizeof(path), "/%s", entry->name);
    }
    else
    {
        size_t dl = strlen(dir);
        const char *sep = (dl > 0 && dir[dl - 1] == '/') ? "" : "/";
        n = snprintf(path, sizeof(path), "%s%s%s", dir, sep, entry->name);
    }

    if (n < 0 || (size_t)n >= sizeof(path))
    {
        return -1;
    }

    return io->storage->ops->file_size(path);
}

// cat - prints files and directories in a tight multi-column layout (like `ls`),
// sorted alphabetically. Directories have a trailing "/".
// (cat pathname) - lists the specified directory.
static Result prim_cat(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_none();
    }

    char resolved_path[256];
    const char *dir = NULL;
    Result err;
    if (!catalog_resolve_dir(io, argc, args, resolved_path, sizeof(resolved_path), &dir, &err))
    {
        return err;
    }

    CatalogContext ctx;
    if (!catalog_collect(io, dir, &ctx) || ctx.count == 0)
    {
        return result_none();
    }

    // Widest display name (directories carry a trailing "/").
    size_t widest = 0;
    for (int i = 0; i < ctx.count; i++)
    {
        size_t len = strlen(ctx.entries[i].name) + (ctx.entries[i].is_directory ? 1 : 0);
        if (len > widest)
        {
            widest = len;
        }
    }

    // Fit as many columns as the display width allows (2-space gutter).
    size_t col_width = widest + 2;
    int ncols = (int)(CATALOG_DISPLAY_WIDTH / col_width);
    if (ncols < 1)
    {
        ncols = 1;
    }
    int nrows = (ctx.count + ncols - 1) / ncols;

    // Column-major layout: fill down each column, then across (like `ls`).
    // A row is at most the padded columns plus one (possibly wide) trailing name.
    for (int row = 0; row < nrows; row++)
    {
        char line[CATALOG_DISPLAY_WIDTH + LOGO_STREAM_NAME_MAX + 4];
        int pos = 0;
        for (int col = 0; col < ncols; col++)
        {
            int idx = col * nrows + row;
            if (idx >= ctx.count)
            {
                break;
            }
            CatalogEntry *entry = &ctx.entries[idx];
            bool last = (col == ncols - 1) || (idx + nrows >= ctx.count);
            int n = snprintf(line + pos, sizeof(line) - pos, "%s%s",
                             entry->name, entry->is_directory ? "/" : "");
            if (n < 0 || (size_t)(pos + n) >= sizeof(line))
            {
                break; // Line buffer full - emit what we have.
            }
            pos += n;
            if (!last)
            {
                // Pad to the column boundary before the next entry.
                size_t cell = strlen(entry->name) + (entry->is_directory ? 1 : 0);
                for (size_t p = cell; p < col_width && pos < (int)sizeof(line) - 1; p++)
                {
                    line[pos++] = ' ';
                }
                line[pos] = '\0';
            }
        }
        logo_io_console_write(io, line);
        logo_io_console_write(io, "\n");
    }

    return result_none();
}

// catalog - prints a detailed listing (like `ls -l`): one entry per line with a
// right-aligned size column, sorted alphabetically. Directories have a blank
// size column and carry a trailing "/".
// (catalog pathname) - lists the specified directory.
static Result prim_catalog(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_none();
    }

    char resolved_path[256];
    const char *dir = NULL;
    Result err;
    if (!catalog_resolve_dir(io, argc, args, resolved_path, sizeof(resolved_path), &dir, &err))
    {
        return err;
    }

    CatalogContext ctx;
    if (!catalog_collect(io, dir, &ctx))
    {
        return result_none();
    }

    for (int i = 0; i < ctx.count; i++)
    {
        CatalogEntry *entry = &ctx.entries[i];
        char line[LOGO_STREAM_NAME_MAX + 16];

        if (entry->is_directory)
        {
            // Directories have no size - leave the column blank.
            snprintf(line, sizeof(line), "%7s  %s/\n", "", entry->name);
        }
        else
        {
            long size = catalog_entry_size(io, dir, entry);
            if (size >= 0)
            {
                snprintf(line, sizeof(line), "%7ld  %s\n", size, entry->name);
            }
            else
            {
                // Size unavailable - leave the column blank.
                snprintf(line, sizeof(line), "%7s  %s\n", "", entry->name);
            }
        }

        logo_io_console_write(io, line);
    }

    return result_none();
}

//==========================================================================
// Prefix and file/directory management
//==========================================================================

// Sets the file prefix
static Result prim_setprefix(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *prefix = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if the directory exists
    // For relative paths, resolve with the current prefix first
    // For absolute paths (starting with "/"), check directly or skip if it's just "/"
    bool is_absolute = (prefix[0] == '/');
    bool exists = false;
    char resolved[LOGO_STREAM_NAME_MAX];
    const char *final_prefix = prefix;  // What we'll actually set
    
    if (is_absolute)
    {
        // Root directory "/" always exists
        if (prefix[1] == '\0')
        {
            exists = true;
        }
        else if (io->storage && io->storage->ops->dir_exists)
        {
            exists = io->storage->ops->dir_exists(prefix);
        }
        else
        {
            exists = true; // No storage or no dir_exists, assume it exists
        }
        // For absolute paths, use as-is
        final_prefix = prefix;
    }
    else
    {
        // Relative path - resolve with current prefix and check
        char *resolved_path = logo_io_resolve_path(io, prefix, resolved, sizeof(resolved));
        if (resolved_path)
        {
            // Root directory "/" always exists (can happen with ".." navigation)
            if (resolved_path[0] == '/' && resolved_path[1] == '\0')
            {
                exists = true;
            }
            else if (io->storage && io->storage->ops->dir_exists)
            {
                exists = io->storage->ops->dir_exists(resolved_path);
            }
            else
            {
                exists = true; // No storage or no dir_exists, assume it exists
            }
            // For relative paths, set the resolved path as the new prefix
            final_prefix = resolved_path;
        }
    }
    
    if (!exists)
    {
        // Distinguish "the volume isn't there" (e.g. no SD card under /sd) from
        // "the directory doesn't exist on a mounted volume".
        if (!logo_io_mount_available(io, final_prefix))
        {
            return result_error(ERR_NO_SD_CARD);
        }
        return result_error_arg(ERR_SUBDIR_NOT_FOUND, prefix, NULL);
    }

    // Set the prefix to the resolved path
    size_t len = strlen(final_prefix);
    if (len >= LOGO_PREFIX_MAX - 1)
    {
        len = LOGO_PREFIX_MAX - 2;  // Leave room for trailing '/' and null
    }
    strncpy(io->prefix, final_prefix, len);
    
    // Ensure trailing slash (unless empty)
    if (len > 0 && io->prefix[len - 1] != '/')
    {
        io->prefix[len] = '/';
        io->prefix[len + 1] = '\0';
    }
    else
    {
        io->prefix[len] = '\0';
    }

    return result_none();
}

// Gets the file prefix
static Result prim_getprefix(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_word(mem_atom_cstr("")));
    }

    return result_ok(value_word(mem_atom_cstr(io->prefix)));
}

// Erase the file
static Result prim_erase_file(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *filename = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    if (!logo_io_file_delete(io, filename))
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", filename);
    }
    return result_none();
}

// Erase the directory
static Result prim_erase_directory(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *dirname = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    if (!logo_io_dir_delete(io, dirname))
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", dirname);
    }
    return result_none();
}

// Check if files exists
static Result prim_filep(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);
    const char *filename = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    if (logo_io_file_exists(io, filename))
    {
        return result_ok(value_bool(true));
    }
    return result_ok(value_bool(false));
} 

// Check if directory exists
static Result prim_dirp(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);
    const char *dirname = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    if (logo_io_dir_exists(io, dirname))
    {
        return result_ok(value_bool(true));
    }
    return result_ok(value_bool(false));
}

// Rename a file or directory
static Result prim_rename(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);
    REQUIRE_WORD(args[1]);

    const char *old_name = mem_word_ptr(args[0].as.node);
    const char *new_name = mem_word_ptr(args[1].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    if (!logo_io_rename(io, old_name, new_name))
    {
        // A directory source that fails to rename means a cross-filesystem
        // directory move, which is not supported (files only). Report the type
        // mismatch rather than a misleading "file not found".
        if (logo_io_dir_exists(io, old_name))
        {
            return result_error(ERR_FILE_WRONG_TYPE);
        }
        return result_error_arg(ERR_FILE_NOT_FOUND, "", old_name);
    }
    return result_none();
}

// copyfile source destination - copies a file (binary-safe, so images and other
// binary files copy intact). Works within or across filesystems (e.g. /sd to /).
static Result prim_copyfile(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);
    REQUIRE_WORD(args[1]);

    const char *src = mem_word_ptr(args[0].as.node);
    const char *dst = mem_word_ptr(args[1].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    if (!logo_io_copy_file(io, src, dst))
    {
        // Mirror rename's diagnostics: a directory source can't be copied (files
        // only), and an existing directory at the destination is a type clash;
        // otherwise the source was not found.
        if (logo_io_dir_exists(io, src) || logo_io_dir_exists(io, dst))
        {
            return result_error(ERR_FILE_WRONG_TYPE);
        }
        return result_error_arg(ERR_FILE_NOT_FOUND, "", src);
    }
    return result_none();
}

// backup pathname - writes a sparse image of the internal filesystem to
// `pathname`, which must be on the SD card (the image cannot live on the volume
// it is imaging). Only blocks in use are stored, so the file stays small.
static Result prim_backup(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *path = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }
    if (!logo_io_is_external_path(io, path))
    {
        return result_error(ERR_BACKUP_LOCATION);
    }
    // Overwrite any previous backup cleanly (open would otherwise append).
    if (logo_io_file_exists(io, path))
    {
        logo_io_file_delete(io, path);
    }
    LogoStream *out = logo_io_open(io, path);
    if (!out)
    {
        return result_error(ERR_DISK_TROUBLE);
    }
    bool ok = logo_io_fs_image_backup(io, out);
    logo_io_close(io, path);
    if (!ok)
    {
        logo_io_file_delete(io, path); // remove the partial image
        return result_error(ERR_DISK_TROUBLE);
    }
    return result_none();
}

// .restore pathname - DANGEROUS. Erases the internal filesystem and replaces it
// with the image in `pathname` (which must be on the SD card). Tolerates an
// image from a smaller device, growing to fill this one. The leading "." marks
// it as a dangerous operation, per Logo convention.
static Result prim_restore(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *path = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }
    if (!logo_io_file_exists(io, path))
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", path);
    }
    // The image must be on the SD card: restore unmounts and reflashes the root
    // volume, so an image stored there would vanish mid-restore.
    if (!logo_io_is_external_path(io, path))
    {
        return result_error(ERR_BACKUP_LOCATION);
    }
    // Close every open file before the root volume is unmounted and reflashed.
    logo_io_close_all(io);
    LogoStream *in = logo_io_open(io, path);
    if (!in)
    {
        return result_error(ERR_DISK_TROUBLE);
    }
    bool ok = logo_io_fs_image_restore(io, in);
    logo_io_close(io, path);
    if (!ok)
    {
        return result_error(ERR_BACKUP_INVALID);
    }
    return result_none();
}

// Create a new directory
static Result prim_createdir(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *filename = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    if (!logo_io_dir_create(io, filename))
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", filename);
    }
    return result_none();
}

// free [pathname] - outputs a two-element list [free_blocks block_size] for the
// filesystem backing `pathname` (or the current prefix's filesystem when no
// argument is given): the number of free allocation blocks and the size of one
// block in bytes. Block sizes differ between volumes (e.g. `/` vs `/sd`), so the
// unit size lets you convert to bytes: `free_blocks * block_size`.
static Result prim_free(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    const char *path = "."; // current prefix's volume by default
    if (argc >= 1)
    {
        REQUIRE_WORD(args[0]);
        path = mem_word_ptr(args[0].as.node);
    }

    uint32_t free_blocks = 0, block_size = 0;
    if (!logo_io_free_blocks(io, path, &free_blocks, &block_size))
    {
        // Most commonly: no SD card for a /sd path.
        return result_error(ERR_NO_SD_CARD);
    }

    Node list = mem_cons(number_to_word((float)free_blocks),
                         mem_cons(number_to_word((float)block_size), NODE_NIL));
    return result_ok(value_list(list));
}

//==========================================================================
// Registration
//==========================================================================

void primitives_files_directory_init(void)
{
    // Directory listing
    primitive_register("files", 0, prim_files);
    primitive_register("free", 0, prim_free);
    primitive_register("directories", 0, prim_directories);
    primitive_register("cat", 0, prim_cat);
    primitive_register("catalog", 0, prim_catalog);
    primitive_register("setprefix", 1, prim_setprefix);
    primitive_register("sp", 1, prim_setprefix);
    primitive_register("prefix", 0, prim_getprefix);
    primitive_register("erasefile", 1, prim_erase_file);
    primitive_register("erf", 1, prim_erase_file);
    primitive_register("createdir", 1, prim_createdir);
    primitive_register("erasedir", 1, prim_erase_directory);
    primitive_register("file?", 1, prim_filep);
    primitive_register("filep", 1, prim_filep);
    primitive_register("dir?", 1, prim_dirp);
    primitive_register("dirp", 1, prim_dirp);
    primitive_register("rename", 2, prim_rename);
    primitive_register("copyfile", 2, prim_copyfile);
    primitive_register("backup", 1, prim_backup);
    primitive_register(".restore", 1, prim_restore);
}
