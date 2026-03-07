//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Shared mock filesystem for file-related tests.
//  This is a header-only mock with static functions/data,
//  included by each test file that needs filesystem support.
//

#pragma once

#include "test_scaffold.h"
#include <stdlib.h>
#include <string.h>
#include "devices/console.h"
#include "devices/storage.h"

//==========================================================================
// Mock File System
//==========================================================================

#define MOCK_MAX_FILES 10
#define MOCK_FILE_SIZE 1024

typedef struct MockFile
{
    char name[LOGO_STREAM_NAME_MAX];
    char data[MOCK_FILE_SIZE];
    size_t size;
    bool exists;
    bool is_directory;
} MockFile;

// Mock file storage
static MockFile mock_files[MOCK_MAX_FILES];

// Mock file stream context
typedef struct MockFileContext
{
    MockFile *file;
    size_t read_pos;
    size_t write_pos;
} MockFileContext;

// Mock file stream operations
static int mock_file_read_char(LogoStream *stream)
{
    MockFileContext *ctx = (MockFileContext *)stream->context;
    if (!ctx || !ctx->file || ctx->read_pos >= ctx->file->size)
    {
        return -1;
    }
    return (unsigned char)ctx->file->data[ctx->read_pos++];
}

static int mock_file_read_chars(LogoStream *stream, char *buffer, int count)
{
    MockFileContext *ctx = (MockFileContext *)stream->context;
    if (!ctx || !ctx->file)
    {
        return -1;
    }
    
    int i;
    for (i = 0; i < count && ctx->read_pos < ctx->file->size; i++)
    {
        buffer[i] = ctx->file->data[ctx->read_pos++];
    }
    return i;
}

static int mock_file_read_line(LogoStream *stream, char *buffer, size_t buffer_size)
{
    MockFileContext *ctx = (MockFileContext *)stream->context;
    if (!ctx || !ctx->file || ctx->read_pos >= ctx->file->size)
    {
        return -1;
    }
    
    size_t i = 0;
    while (i < buffer_size - 1 && ctx->read_pos < ctx->file->size)
    {
        char c = ctx->file->data[ctx->read_pos++];
        buffer[i++] = c;
        if (c == '\n')
            break;
    }
    buffer[i] = '\0';
    return (int)i;
}

static bool mock_file_can_read(LogoStream *stream)
{
    MockFileContext *ctx = (MockFileContext *)stream->context;
    return ctx && ctx->file && ctx->read_pos < ctx->file->size;
}

static void mock_file_write(LogoStream *stream, const char *text)
{
    MockFileContext *ctx = (MockFileContext *)stream->context;
    if (!ctx || !ctx->file)
        return;
    
    size_t len = strlen(text);
    for (size_t i = 0; i < len && ctx->write_pos < MOCK_FILE_SIZE - 1; i++)
    {
        ctx->file->data[ctx->write_pos++] = text[i];
    }
    if (ctx->write_pos > ctx->file->size)
    {
        ctx->file->size = ctx->write_pos;
    }
    ctx->file->data[ctx->file->size] = '\0';
}

static void mock_file_flush(LogoStream *stream)
{
    (void)stream;
}

static long mock_file_get_read_pos(LogoStream *stream)
{
    MockFileContext *ctx = (MockFileContext *)stream->context;
    return ctx ? (long)ctx->read_pos : -1;
}

static bool mock_file_set_read_pos(LogoStream *stream, long pos)
{
    MockFileContext *ctx = (MockFileContext *)stream->context;
    if (!ctx || !ctx->file || pos < 0 || (size_t)pos > ctx->file->size)
    {
        return false;
    }
    ctx->read_pos = (size_t)pos;
    return true;
}

static long mock_file_get_write_pos(LogoStream *stream)
{
    MockFileContext *ctx = (MockFileContext *)stream->context;
    return ctx ? (long)ctx->write_pos : -1;
}

static bool mock_file_set_write_pos(LogoStream *stream, long pos)
{
    MockFileContext *ctx = (MockFileContext *)stream->context;
    if (!ctx || !ctx->file || pos < 0 || (size_t)pos > ctx->file->size)
    {
        return false;
    }
    ctx->write_pos = (size_t)pos;
    return true;
}

static long mock_file_get_length(LogoStream *stream)
{
    MockFileContext *ctx = (MockFileContext *)stream->context;
    return ctx && ctx->file ? (long)ctx->file->size : -1;
}

static void mock_file_close(LogoStream *stream)
{
    MockFileContext *ctx = (MockFileContext *)stream->context;
    if (ctx)
    {
        free(ctx);
        stream->context = NULL;
    }
}

static LogoStreamOps mock_file_ops = {
    .read_char = mock_file_read_char,
    .read_chars = mock_file_read_chars,
    .read_line = mock_file_read_line,
    .can_read = mock_file_can_read,
    .write = mock_file_write,
    .flush = mock_file_flush,
    .get_read_pos = mock_file_get_read_pos,
    .set_read_pos = mock_file_set_read_pos,
    .get_write_pos = mock_file_get_write_pos,
    .set_write_pos = mock_file_set_write_pos,
    .get_length = mock_file_get_length,
    .close = mock_file_close
};

// Find or create a mock file
static MockFile *mock_fs_get_file(const char *name, bool create)
{
    // Look for existing file
    for (int i = 0; i < MOCK_MAX_FILES; i++)
    {
        if (mock_files[i].exists && strcmp(mock_files[i].name, name) == 0)
        {
            return &mock_files[i];
        }
    }
    
    if (!create)
    {
        return NULL;
    }
    
    // Find empty slot
    for (int i = 0; i < MOCK_MAX_FILES; i++)
    {
        if (!mock_files[i].exists)
        {
            strncpy(mock_files[i].name, name, LOGO_STREAM_NAME_MAX - 1);
            mock_files[i].name[LOGO_STREAM_NAME_MAX - 1] = '\0';
            mock_files[i].data[0] = '\0';
            mock_files[i].size = 0;
            mock_files[i].exists = true;
            mock_files[i].is_directory = false;
            return &mock_files[i];
        }
    }
    
    return NULL;
}

// Create a mock file with content
static void mock_fs_create_file(const char *name, const char *content)
{
    MockFile *file = mock_fs_get_file(name, true);
    if (file)
    {
        size_t len = strlen(content);
        if (len > MOCK_FILE_SIZE - 1)
            len = MOCK_FILE_SIZE - 1;
        memcpy(file->data, content, len);
        file->data[len] = '\0';
        file->size = len;
        file->is_directory = false;
    }
}

// Create a mock directory
static void mock_fs_create_dir(const char *name)
{
    MockFile *file = mock_fs_get_file(name, true);
    if (file)
    {
        file->is_directory = true;
    }
}

// Reset the mock file system
static void mock_fs_reset(void)
{
    for (int i = 0; i < MOCK_MAX_FILES; i++)
    {
        mock_files[i].exists = false;
        mock_files[i].is_directory = false;
        mock_files[i].name[0] = '\0';
        mock_files[i].data[0] = '\0';
        mock_files[i].size = 0;
    }
}

// Mock file opener - creates file if it doesn't exist (matches new storage API)
static LogoStream *mock_storage_open(const char *pathname)
{
    // Create file if it doesn't exist
    MockFile *file = mock_fs_get_file(pathname, true);
    if (!file)
        return NULL;
    
    // Create the stream
    LogoStream *stream = malloc(sizeof(LogoStream));
    if (!stream)
        return NULL;
    
    MockFileContext *ctx = malloc(sizeof(MockFileContext));
    if (!ctx)
    {
        free(stream);
        return NULL;
    }
    
    ctx->file = file;
    ctx->read_pos = 0;
    ctx->write_pos = file->size;  // Write position starts at end of file
    
    logo_stream_init(stream, LOGO_STREAM_FILE, &mock_file_ops, ctx, pathname);
    stream->is_open = true;
    
    return stream;
}

// Mock storage operations
static bool mock_storage_file_exists(const char *pathname)
{
    MockFile *file = mock_fs_get_file(pathname, false);
    return file && !file->is_directory;
}

static bool mock_storage_dir_exists(const char *pathname)
{
    MockFile *file = mock_fs_get_file(pathname, false);
    return file && file->is_directory;
}

static bool mock_storage_file_delete(const char *pathname)
{
    MockFile *file = mock_fs_get_file(pathname, false);
    if (!file || file->is_directory)
        return false;
    file->exists = false;
    return true;
}

static bool mock_storage_dir_create(const char *pathname)
{
    MockFile *file = mock_fs_get_file(pathname, true);
    if (file)
    {
        file->is_directory = true;
        return true;
    }
    return false;
}

static bool mock_storage_dir_delete(const char *pathname)
{
    MockFile *file = mock_fs_get_file(pathname, false);
    if (!file || !file->is_directory)
        return false;
    file->exists = false;
    return true;
}

static bool mock_storage_rename(const char *old_path, const char *new_path)
{
    MockFile *file = mock_fs_get_file(old_path, false);
    if (!file)
        return false;
    strncpy(file->name, new_path, LOGO_STREAM_NAME_MAX - 1);
    file->name[LOGO_STREAM_NAME_MAX - 1] = '\0';
    return true;
}

static long mock_storage_file_size(const char *pathname)
{
    MockFile *file = mock_fs_get_file(pathname, false);
    return file ? (long)file->size : -1;
}

static bool mock_storage_list_directory(const char *pathname, LogoDirCallback callback,
                                        void *user_data, const char *filter)
{
    (void)pathname; // Ignore path for flat mock fs
    
    for (int i = 0; i < MOCK_MAX_FILES; i++)
    {
        if (mock_files[i].exists)
        {
            // Simple filter check (extension)
            if (filter && strcmp(filter, "*") != 0)
            {
                // Check extension
                const char *ext = strrchr(mock_files[i].name, '.');
                if (!ext || strcmp(ext + 1, filter) != 0)
                {
                    continue;
                }
            }
            
            LogoEntryType type = mock_files[i].is_directory ? LOGO_ENTRY_DIRECTORY : LOGO_ENTRY_FILE;
            if (!callback(mock_files[i].name, type, user_data))
            {
                return false;
            }
        }
    }
    return true;
}

static LogoStorageOps mock_storage_ops = {
    .open = mock_storage_open,
    .file_exists = mock_storage_file_exists,
    .dir_exists = mock_storage_dir_exists,
    .file_delete = mock_storage_file_delete,
    .dir_create = mock_storage_dir_create,
    .dir_delete = mock_storage_dir_delete,
    .rename = mock_storage_rename,
    .file_size = mock_storage_file_size,
    .list_directory = mock_storage_list_directory
};

static LogoStorage mock_storage;

//==========================================================================
// Common Setup/Teardown for file tests
//==========================================================================

static void mock_fs_setUp(void)
{
    test_scaffold_setUp();
    mock_fs_reset();
    
    // Initialize mock storage with our operations
    logo_storage_init(&mock_storage, &mock_storage_ops);
    
    // Re-initialize I/O with mock storage (using mock_console from scaffold)
    logo_io_init(&mock_io, &mock_console, &mock_storage, NULL);
    primitives_set_io(&mock_io);
}

static void mock_fs_tearDown(void)
{
    // Close all files first
    logo_io_close_all(&mock_io);
    mock_fs_reset();
    test_scaffold_tearDown();
}
