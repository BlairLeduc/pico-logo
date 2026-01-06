//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Files primitives: open, openread, openwrite, openappend, openupdate, close, closeall,
//                    setread, setwrite, reader, writer, allopen, readpos, setreadpos,
//                    writepos, setwritepos, filelen, dribble, nodribble, load, save
//

#include "primitives.h"
#include "procedures.h"
#include "variables.h"
#include "properties.h"
#include "error.h"
#include "eval.h"
#include "format.h"
#include "lexer.h"
#include "devices/io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

//==========================================================================
// File management primitives
//==========================================================================

// open file - opens file for read/write, creates if doesn't exist
static Result prim_open(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *pathname = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if file is already open
    if (logo_io_is_open(io, pathname))
    {
        return result_error_arg(ERR_FILE_ALREADY_OPEN, NULL, pathname);
    }

    LogoStream *stream = logo_io_open(io, pathname);
    if (!stream)
    {
        // Check if we hit the file limit
        if (logo_io_open_count(io) >= LOGO_MAX_OPEN_FILES)
        {
            return result_error(ERR_NO_FILE_BUFFERS);
        }
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_none();
}

// close file - closes the named file
static Result prim_close(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *pathname = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if file is open
    if (!logo_io_is_open(io, pathname))
    {
        return result_error_arg(ERR_FILE_NOT_OPEN, NULL, pathname);
    }

    logo_io_close(io, pathname);
    return result_none();
}

// closeall - closes all open files (not dribble)
static Result prim_closeall(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (io)
    {
        logo_io_close_all(io);
    }

    return result_none();
}

// setread file - sets current reader to file (empty list for keyboard)
static Result prim_setread(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Empty list means reset to keyboard
    if (args[0].type == VALUE_LIST && mem_is_nil(args[0].as.node))
    {
        logo_io_set_reader(io, NULL);
        return result_none();
    }

    if (args[0].type != VALUE_WORD)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    const char *pathname = mem_word_ptr(args[0].as.node);
    
    // Find the open stream
    LogoStream *stream = logo_io_find_open(io, pathname);
    if (!stream)
    {
        return result_error_arg(ERR_FILE_NOT_OPEN, NULL, pathname);
    }

    logo_io_set_reader(io, stream);
    return result_none();
}

// setwrite file - sets current writer to file (empty list for screen)
static Result prim_setwrite(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Empty list means reset to screen
    if (args[0].type == VALUE_LIST && mem_is_nil(args[0].as.node))
    {
        logo_io_set_writer(io, NULL);
        return result_none();
    }

    if (args[0].type != VALUE_WORD)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    const char *pathname = mem_word_ptr(args[0].as.node);
    
    // Find the open stream
    LogoStream *stream = logo_io_find_open(io, pathname);
    if (!stream)
    {
        return result_error_arg(ERR_FILE_NOT_OPEN, NULL, pathname);
    }

    logo_io_set_writer(io, stream);
    return result_none();
}

// reader - outputs the current reader name (empty list for keyboard)
static Result prim_reader(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_list(NODE_NIL));
    }

    if (logo_io_reader_is_keyboard(io))
    {
        return result_ok(value_list(NODE_NIL));
    }

    const char *name = logo_io_get_reader_name(io);
    return result_ok(value_word(mem_atom_cstr(name)));
}

// writer - outputs the current writer name (empty list for screen)
static Result prim_writer(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_list(NODE_NIL));
    }

    if (logo_io_writer_is_screen(io))
    {
        return result_ok(value_list(NODE_NIL));
    }

    const char *name = logo_io_get_writer_name(io);
    return result_ok(value_word(mem_atom_cstr(name)));
}

// allopen - outputs a list of all open files
static Result prim_allopen(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_list(NODE_NIL));
    }

    // Build list of open file names
    Node list = NODE_NIL;
    int count = logo_io_open_count(io);
    
    // Build in reverse order so first file ends up first in list
    for (int i = count - 1; i >= 0; i--)
    {
        LogoStream *stream = logo_io_get_open(io, i);
        if (stream)
        {
            Node name_node = mem_atom_cstr(stream->name);
            list = mem_cons(name_node, list);
        }
    }

    return result_ok(value_list(list));
}

// readpos - outputs the current read position in the current file
static Result prim_readpos(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (!io || !io->reader)
    {
        return result_error(ERR_NO_FILE_SELECTED);
    }

    // Error if reader is keyboard (no file selected)
    if (logo_io_reader_is_keyboard(io))
    {
        return result_error(ERR_NO_FILE_SELECTED);
    }

    long pos = logo_stream_get_read_pos(io->reader);
    if (pos < 0)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_ok(value_number((float)pos));
}

// setreadpos integer - sets the read position in the current file
static Result prim_setreadpos(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], pos_f);
    long pos = (long)pos_f;
    if (pos < 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    LogoIO *io = primitives_get_io();
    if (!io || !io->reader)
    {
        return result_error(ERR_NO_FILE_SELECTED);
    }

    // Error if reader is keyboard (no file selected)
    if (logo_io_reader_is_keyboard(io))
    {
        return result_error(ERR_NO_FILE_SELECTED);
    }

    if (!logo_stream_set_read_pos(io->reader, pos))
    {
        return result_error(ERR_FILE_POS_OUT_OF_RANGE);
    }

    return result_none();
}

// writepos - outputs the current write position in the current file
static Result prim_writepos(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (!io || !io->writer)
    {
        return result_error(ERR_NO_FILE_SELECTED);
    }

    // Error if writer is screen (no file selected)
    if (logo_io_writer_is_screen(io))
    {
        return result_error(ERR_NO_FILE_SELECTED);
    }

    long pos = logo_stream_get_write_pos(io->writer);
    if (pos < 0)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_ok(value_number((float)pos));
}

// setwritepos integer - sets the write position in the current file
static Result prim_setwritepos(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], pos_f);
    long pos = (long)pos_f;
    if (pos < 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    LogoIO *io = primitives_get_io();
    if (!io || !io->writer)
    {
        return result_error(ERR_NO_FILE_SELECTED);
    }

    // Error if writer is screen (no file selected)
    if (logo_io_writer_is_screen(io))
    {
        return result_error(ERR_NO_FILE_SELECTED);
    }

    if (!logo_stream_set_write_pos(io->writer, pos))
    {
        return result_error(ERR_FILE_POS_OUT_OF_RANGE);
    }

    return result_none();
}

// filelen pathname - outputs the length in bytes of the file
// The file must be open
static Result prim_filelen(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *pathname = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Find the open stream
    LogoStream *stream = logo_io_find_open(io, pathname);
    if (!stream)
    {
        return result_error_arg(ERR_FILE_NOT_OPEN, NULL, pathname);
    }

    long len = logo_stream_get_length(stream);
    if (len < 0)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_ok(value_number((float)len));
}

// dribble file - starts dribbling output to file
static Result prim_dribble(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *pathname = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if already dribbling
    if (logo_io_is_dribbling(io))
    {
        return result_error(ERR_ALREADY_DRIBBLING);
    }

    if (!logo_io_start_dribble(io, pathname))
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_none();
}

// nodribble - stops dribbling
static Result prim_nodribble(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (io)
    {
        logo_io_stop_dribble(io);
    }

    return result_none();
}

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
static Result prim_catalog(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_none();
    }

    // Get current directory (use prefix if set, otherwise ".")
    const char *dir = logo_io_get_prefix(io);
    if (!dir || dir[0] == '\0')
    {
        dir = ".";
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
// File I/O: load and save
//==========================================================================

// Helper to check if a line starts with "to " (case-insensitive)
static bool line_starts_with_to(const char *line)
{
    // Skip leading whitespace
    while (*line && isspace((unsigned char)*line))
        line++;

    if (strncasecmp(line, "to", 2) != 0)
        return false;

    // Must be followed by whitespace or end of line
    char c = line[2];
    return c == '\0' || isspace((unsigned char)c);
}

// Helper to check if a line is just "end" (case-insensitive)
static bool line_is_end(const char *line)
{
    // Skip leading whitespace
    while (*line && isspace((unsigned char)*line))
        line++;

    if (strncasecmp(line, "end", 3) != 0)
        return false;

    // Must be followed by whitespace or end of string
    line += 3;
    
    // Skip any trailing whitespace
    while (*line && isspace((unsigned char)*line))
        line++;
    
    // Line must be empty after "end" and whitespace
    return *line == '\0';
}

// Maximum line length for load
#define LOAD_MAX_LINE 256

// Maximum procedure buffer for load
#define LOAD_MAX_PROC 4096

// load pathname - loads and executes file contents
static Result prim_load(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *pathname = mem_word_ptr(args[0].as.node);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if file exists (logo_io_file_exists resolves path internally)
    if (!logo_io_file_exists(io, pathname))
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", pathname);
    }

    // Remember startup variable state before loading
    // We only run startup if the loaded file sets it (changes the value)
    bool startup_existed_before = var_exists("startup");
    Value startup_before = {0};
    if (startup_existed_before)
    {
        var_get("startup", &startup_before);
    }

    // Open the file for reading (logo_io_open resolves path internally)
    LogoStream *stream = logo_io_open(io, pathname);
    if (!stream)
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", pathname);
    }

    // Read and execute the file line by line
    char line[LOAD_MAX_LINE];
    char proc_buffer[LOAD_MAX_PROC];
    size_t proc_len = 0;
    bool in_procedure_def = false;
    Result result = result_none();

    while (logo_stream_read_line(stream, line, sizeof(line)) >= 0)
    {
        // Strip trailing newline/carriage return
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        {
            line[--len] = '\0';
        }

        // Skip empty lines
        if (len == 0)
        {
            continue;
        }

        // Handle multi-line procedure definitions
        if (!in_procedure_def && line_starts_with_to(line))
        {
            // Start collecting procedure definition
            in_procedure_def = true;
            proc_len = 0;

            // Copy the "to" line to buffer with newline
            if (len + 2 <= LOAD_MAX_PROC - 10)
            {
                memcpy(proc_buffer, line, len);
                proc_buffer[len] = '\n';
                proc_len = len + 1;
            }
            else
            {
                // Initial "to" line does not fit in the procedure buffer - skip this procedure
                in_procedure_def = false;
                proc_len = 0;
                // Note: Error is not reported to avoid breaking load operation on partial file read
            }
            continue;
        }

        if (in_procedure_def)
        {
            if (line_is_end(line))
            {
                // Complete the procedure definition
                if (proc_len + 4 < LOAD_MAX_PROC)
                {
                    memcpy(proc_buffer + proc_len, "end", 3);
                    proc_len += 3;
                    proc_buffer[proc_len] = '\0';
                }

                in_procedure_def = false;

                // Parse and define the procedure
                Result r = proc_define_from_text(proc_buffer);
                if (r.status == RESULT_ERROR)
                {
                    result = r;
                    break;
                }

                proc_len = 0;
            }
            else
            {
                // Append line to procedure buffer with newline
                if (proc_len + len + 2 <= LOAD_MAX_PROC - 10)
                {
                    memcpy(proc_buffer + proc_len, line, len);
                    proc_buffer[proc_len + len] = '\n';
                    proc_len += len + 1;
                }
                else
                {
                    // Line does not fit - silently skip to avoid breaking load on partial file read
                    // The procedure will be incomplete but load continues
                }
            }
            continue;
        }

        // Regular instruction - evaluate it
        Lexer lexer;
        Evaluator load_eval;
        lexer_init(&lexer, line);
        eval_init(&load_eval, &lexer);

        // Evaluate all instructions on the line
        while (!eval_at_end(&load_eval))
        {
            Result r = eval_instruction(&load_eval);

            if (r.status == RESULT_ERROR)
            {
                result = r;
                break;
            }
            else if (r.status == RESULT_THROW)
            {
                // Propagate throw
                result = r;
                break;
            }
            else if (r.status == RESULT_OK)
            {
                // Expression returned a value - ignore in load
                // (Unlike REPL, we don't report "I don't know what to do with")
            }
            // RESULT_NONE means command completed successfully - continue
        }

        if (result.status != RESULT_NONE)
        {
            break;
        }
    }

    // Close the file (logo_io_close resolves path internally)
    logo_io_close(io, pathname);

    // If load was successful, check if the file set the startup variable
    // Run startup only if the loaded file set it (value changed or newly created)
    if (result.status == RESULT_NONE)
    {
        Value startup_after;
        if (var_get("startup", &startup_after))
        {
            // Check if startup was set by the loaded file:
            // - It didn't exist before, or
            // - It existed but the value (node pointer) changed
            bool startup_set_by_file = !startup_existed_before ||
                                       (startup_after.as.node != startup_before.as.node);
            
            if (startup_set_by_file && value_is_list(startup_after))
            {
                // Execute the startup list
                result = eval_run_list(eval, startup_after.as.node);
            }
        }
    }

    return result;
}

//==========================================================================
// Save helper functions
//==========================================================================

// Output callback for file saving (always succeeds since logo_io_write handles errors)
static bool save_output(void *ctx, const char *str)
{
    (void)ctx;
    LogoIO *io = primitives_get_io();
    if (io)
    {
        logo_io_write(io, str);
    }
    return true;
}

// save pathname - saves all unburied procedures, variables, and properties
static Result prim_save(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *pathname = mem_word_ptr(args[0].as.node);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if file already exists (logo_io_file_exists resolves path internally)
    if (logo_io_file_exists(io, pathname))
    {
        return result_error_arg(ERR_FILE_EXISTS, "", pathname);
    }

    // Open the file for writing (logo_io_open resolves path internally)
    LogoStream *stream = logo_io_open(io, pathname);
    if (!stream)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    // Save current writer and set to file
    LogoStream *old_writer = io->writer;
    logo_io_set_writer(io, stream);

    // Save all procedures (not buried)
    int proc_cnt = proc_count(true); // Get ALL, filter by buried in loop
    for (int i = 0; i < proc_cnt; i++)
    {
        UserProcedure *proc = proc_get_by_index(i);
        if (proc && !proc->buried)
        {
            format_procedure_definition(save_output, NULL, proc);
            save_output(NULL, "\n");
        }
    }

    // Save all variables (not buried)
    int var_cnt = var_global_count(false);
    for (int i = 0; i < var_cnt; i++)
    {
        const char *name;
        Value value;
        if (var_get_global_by_index(i, false, &name, &value))
        {
            format_variable(save_output, NULL, name, value);
        }
    }

    // Save all property lists
    int prop_cnt = prop_name_count();
    for (int i = 0; i < prop_cnt; i++)
    {
        const char *name;
        if (prop_get_name_by_index(i, &name))
        {
            Node list = prop_get_list(name);
            format_property_list(save_output, NULL, name, list);
        }
    }

    // Restore writer and close file (logo_io_close resolves path internally)
    logo_io_set_writer(io, old_writer == &io->console->output ? NULL : old_writer);
    logo_io_close(io, pathname);

    return result_none();
}

// savel name pathname or savel [name1 name2 ...] pathname
// Save specified procedure(s) and all unburied variables and properties
static Result prim_savel(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[1]);

    const char *pathname = mem_word_ptr(args[1].as.node);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if file already exists
    if (logo_io_file_exists(io, pathname))
    {
        return result_error_arg(ERR_FILE_EXISTS, "", pathname);
    }

    // First argument is name or list of names - validate procedures exist first
    if (value_is_word(args[0]))
    {
        const char *name = mem_word_ptr(args[0].as.node);
        if (!proc_find(name))
        {
            return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
        }
    }
    else if (value_is_list(args[0]))
    {
        Node curr = args[0].as.node;
        while (!mem_is_nil(curr))
        {
            Node elem = mem_car(curr);
            if (mem_is_word(elem))
            {
                const char *name = mem_word_ptr(elem);
                if (!proc_find(name))
                {
                    return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
                }
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    // Open the file for writing
    LogoStream *stream = logo_io_open(io, pathname);
    if (!stream)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    // Save current writer and set to file
    LogoStream *old_writer = io->writer;
    logo_io_set_writer(io, stream);

    // Save the specified procedure(s)
    if (value_is_word(args[0]))
    {
        const char *name = mem_word_ptr(args[0].as.node);
        UserProcedure *proc = proc_find(name);
        if (proc)
        {
            format_procedure_definition(save_output, NULL, proc);
            save_output(NULL, "\n");
        }
    }
    else if (value_is_list(args[0]))
    {
        Node curr = args[0].as.node;
        while (!mem_is_nil(curr))
        {
            Node elem = mem_car(curr);
            if (mem_is_word(elem))
            {
                const char *name = mem_word_ptr(elem);
                UserProcedure *proc = proc_find(name);
                if (proc)
                {
                    format_procedure_definition(save_output, NULL, proc);
                    save_output(NULL, "\n");
                }
            }
            curr = mem_cdr(curr);
        }
    }

    // Save all variables (not buried)
    int var_cnt = var_global_count(false);
    for (int i = 0; i < var_cnt; i++)
    {
        const char *name;
        Value value;
        if (var_get_global_by_index(i, false, &name, &value))
        {
            format_variable(save_output, NULL, name, value);
        }
    }

    // Save all property lists
    int prop_cnt = prop_name_count();
    for (int i = 0; i < prop_cnt; i++)
    {
        const char *name;
        if (prop_get_name_by_index(i, &name))
        {
            Node list = prop_get_list(name);
            format_property_list(save_output, NULL, name, list);
        }
    }

    // Restore writer and close file
    logo_io_set_writer(io, old_writer == &io->console->output ? NULL : old_writer);
    logo_io_close(io, pathname);

    return result_none();
}

// savepic pathname - saves the graphics screen as a BMP file
static Result prim_savepic(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *pathname = mem_word_ptr(args[0].as.node);

    LogoIO *io = primitives_get_io();
    if (!io || !io->console || !io->console->turtle)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    const LogoConsoleTurtle *turtle = io->console->turtle;
    if (!turtle->gfx_save)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if file already exists (logo_io_file_exists resolves path internally)
    if (logo_io_file_exists(io, pathname))
    {
        return result_error_arg(ERR_FILE_EXISTS, "", pathname);
    }

    // Resolve path with prefix for the actual save
    char resolved[LOGO_STREAM_NAME_MAX];
    char *full_path = logo_io_resolve_path(io, pathname, resolved, sizeof(resolved));
    if (!full_path)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    // Call the turtle graphics save function
    int err = turtle->gfx_save(full_path);
    if (err != 0)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_none();
}

// loadpic pathname - loads an 8-bit indexed color BMP file to the graphics screen
static Result prim_loadpic(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *pathname = mem_word_ptr(args[0].as.node);

    LogoIO *io = primitives_get_io();
    if (!io || !io->console || !io->console->turtle)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    const LogoConsoleTurtle *turtle = io->console->turtle;
    if (!turtle->gfx_load)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if file exists (logo_io_file_exists resolves path internally)
    if (!logo_io_file_exists(io, pathname))
    {
        return result_error(ERR_FILE_NOT_FOUND);
    }

    // Resolve path with prefix for the actual load
    char resolved[LOGO_STREAM_NAME_MAX];
    char *full_path = logo_io_resolve_path(io, pathname, resolved, sizeof(resolved));
    if (!full_path)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    // Call the turtle graphics load function
    int err = turtle->gfx_load(full_path);
    if (err != 0)
    {
        // Check for specific error codes
        if (err == EINVAL)
        {
            return result_error(ERR_FILE_WRONG_TYPE);
        }
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_none();
}

// Maximum line size for pofile
#define POFILE_MAX_LINE 256

// pofile pathname - prints the contents of a file to the screen
// Always prints to the display, not to the current writer.
// Errors if the file is already open.
static Result prim_pofile(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *pathname = mem_word_ptr(args[0].as.node);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if file is already open - this is an error per the spec
    // (logo_io_is_open resolves path internally)
    if (logo_io_is_open(io, pathname))
    {
        return result_error_arg(ERR_FILE_ALREADY_OPEN, NULL, pathname);
    }

    // Check if file exists (logo_io_file_exists resolves path internally)
    if (!logo_io_file_exists(io, pathname))
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", pathname);
    }

    // Open the file for reading (logo_io_open resolves path internally)
    LogoStream *stream = logo_io_open(io, pathname);
    if (!stream)
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", pathname);
    }

    // Read and print each line to the console (not the writer)
    char line[POFILE_MAX_LINE];
    int len;
    while ((len = logo_stream_read_line(stream, line, sizeof(line))) >= 0)
    {
        // Strip trailing newline/carriage return
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        {
            line[--len] = '\0';
        }

        // Print directly to console (bypasses writer)
        logo_io_console_write_line(io, line);
    }

    // Close the file (logo_io_close resolves path internally)
    logo_io_close(io, pathname);

    return result_none();
}

//==========================================================================
// Registration
//==========================================================================

void primitives_files_init(void)
{
    // File management
    primitive_register("open", 1, prim_open);
    primitive_register("close", 1, prim_close);
    primitive_register("closeall", 0, prim_closeall);
    primitive_register("setread", 1, prim_setread);
    primitive_register("setwrite", 1, prim_setwrite);
    primitive_register("reader", 0, prim_reader);
    primitive_register("writer", 0, prim_writer);
    primitive_register("allopen", 0, prim_allopen);
    primitive_register("readpos", 0, prim_readpos);
    primitive_register("setreadpos", 1, prim_setreadpos);
    primitive_register("writepos", 0, prim_writepos);
    primitive_register("setwritepos", 1, prim_setwritepos);
    primitive_register("filelen", 1, prim_filelen);
    primitive_register("dribble", 1, prim_dribble);
    primitive_register("nodribble", 0, prim_nodribble);

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

    // Load and save
    primitive_register("load", 1, prim_load);
    primitive_register("save", 1, prim_save);
    primitive_register("savel", 2, prim_savel);
    primitive_register("savepic", 1, prim_savepic);
    primitive_register("loadpic", 1, prim_loadpic);
    primitive_register("pofile", 1, prim_pofile);
}
