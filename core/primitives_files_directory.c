//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  File directory primitives: files, directories, catalog, setprefix, prefix,
//                             erasefile, erasedir, createdir, file?, dir?, rename
//

#include "primitives.h"
#include "error.h"
#include "devices/io.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

//==========================================================================
// Directory listing primitives: files, directories, catalog
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

// Context for catalog - collects entries then sorts them
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

// catalog - prints a list of files and directories, sorted alphabetically
// (catalog pathname) - prints listing for the specified directory
static Result prim_catalog(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_none();
    }

    const char *dir = NULL;
    char resolved_path[256];

    // Check for optional pathname argument
    if (argc >= 1)
    {
        REQUIRE_WORD(args[0]);
        const char *pathname = mem_word_ptr(args[0].as.node);
        
        // Resolve pathname - if absolute, use directly; otherwise resolve against prefix
        if (pathname[0] == '/')
        {
            dir = pathname;
        }
        else
        {
            // Resolve relative pathname against current prefix
            const char *prefix = logo_io_get_prefix(io);
            if (prefix && prefix[0] != '\0')
            {
                snprintf(resolved_path, sizeof(resolved_path), "%s%s", prefix, pathname);
                dir = resolved_path;
            }
            else
            {
                dir = pathname;
            }
        }
    }
    else
    {
        // No argument - use current directory (prefix if set, otherwise ".")
        dir = logo_io_get_prefix(io);
        if (!dir || dir[0] == '\0')
        {
            dir = ".";
        }
    }

    CatalogContext ctx = {{{{0}}}, 0};
    
    if (!logo_io_list_directory(io, dir, catalog_callback, &ctx, "*"))
    {
        return result_none();
    }

    // Sort entries alphabetically
    if (ctx.count > 0)
    {
        qsort(ctx.entries, ctx.count, sizeof(CatalogEntry), catalog_compare);
    }

    // Print each entry
    for (int i = 0; i < ctx.count; i++)
    {
        CatalogEntry *entry = &ctx.entries[i];
        if (entry->is_directory)
        {
            logo_stream_write(io->writer, entry->name);
            logo_stream_write(io->writer, "/\n");
        }
        else
        {
            logo_stream_write(io->writer, entry->name);
            logo_stream_write(io->writer, "\n");
        }
        
        // Also write to dribble if active
        if (io->dribble)
        {
            if (entry->is_directory)
            {
                logo_stream_write(io->dribble, entry->name);
                logo_stream_write(io->dribble, "/\n");
            }
            else
            {
                logo_stream_write(io->dribble, entry->name);
                logo_stream_write(io->dribble, "\n");
            }
        }
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
        return result_ok(value_word(mem_atom("true", 4)));
    }
    return result_ok(value_word(mem_atom("false", 5)));
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
        return result_ok(value_word(mem_atom("true", 4)));
    }
    return result_ok(value_word(mem_atom("false", 5)));
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
        return result_error_arg(ERR_FILE_NOT_FOUND, "", old_name);
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

//==========================================================================
// Registration
//==========================================================================

void primitives_files_directory_init(void)
{
    // Directory listing
    primitive_register("files", 0, prim_files);
    primitive_register("directories", 0, prim_directories);
    primitive_register("catalog", 0, prim_catalog);
    primitive_register("setprefix", 1, prim_setprefix);
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
}
