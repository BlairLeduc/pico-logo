//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for Files primitives: open, close, closeall,
//                              setread, setwrite, reader, writer, allopen, readpos, setreadpos,
//                              writepos, setwritepos, filelen, dribble, nodribble, load, save
//

#include "test_scaffold.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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
    ctx->write_pos = 0;
    
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
// Test Setup/Teardown
//==========================================================================

void setUp(void)
{
    test_scaffold_setUp();
    mock_fs_reset();
    
    // Initialize mock storage with our operations
    logo_storage_init(&mock_storage, &mock_storage_ops);
    
    // Re-initialize I/O with mock storage (using mock_console from scaffold)
    logo_io_init(&mock_io, &mock_console, &mock_storage, NULL);
    primitives_set_io(&mock_io);
}

void tearDown(void)
{
    // Close all files first
    logo_io_close_all(&mock_io);
    mock_fs_reset();
    test_scaffold_tearDown();
}

//==========================================================================
// Open/Close Tests
//==========================================================================

void test_open_creates_new_file(void)
{
    Result r = run_string("open \"testfile.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Verify file was created
    MockFile *file = mock_fs_get_file("testfile.txt", false);
    TEST_ASSERT_NOT_NULL(file);
}

void test_open_existing_file(void)
{
    mock_fs_create_file("existing.txt", "hello world");
    
    Result r = run_string("open \"existing.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_close_file(void)
{
    mock_fs_create_file("toclose.txt", "data");
    run_string("open \"toclose.txt");
    
    Result r = run_string("close \"toclose.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_close_unopened_file_error(void)
{
    Result r = run_string("close \"notopen.txt");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_closeall(void)
{
    mock_fs_create_file("file1.txt", "a");
    mock_fs_create_file("file2.txt", "b");
    
    run_string("open \"file1.txt");
    run_string("open \"file2.txt");
    
    Result r = run_string("closeall");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Verify allopen returns empty list
    Result r2 = eval_string("allopen");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_TRUE(value_is_list(r2.value));
    TEST_ASSERT_TRUE(mem_is_nil(r2.value.as.node));
}

//==========================================================================
// Reader/Writer Tests
//==========================================================================

void test_reader_default_keyboard(void)
{
    Result r = eval_string("reader");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_writer_default_screen(void)
{
    Result r = eval_string("writer");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_setread_to_file(void)
{
    mock_fs_create_file("input.txt", "file content\n");
    run_string("open \"input.txt");
    run_string("setread \"input.txt");
    
    Result r = eval_string("reader");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("input.txt", mem_word_ptr(r.value.as.node));
}

void test_setread_back_to_keyboard(void)
{
    mock_fs_create_file("input.txt", "content");
    run_string("open \"input.txt");
    run_string("setread \"input.txt");
    run_string("setread []");
    
    Result r = eval_string("reader");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_setread_unopened_file_error(void)
{
    Result r = run_string("setread \"notopen.txt");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setwrite_to_file(void)
{
    run_string("open \"output.txt");
    run_string("setwrite \"output.txt");
    
    Result r = eval_string("writer");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("output.txt", mem_word_ptr(r.value.as.node));
}

void test_setwrite_back_to_screen(void)
{
    run_string("open \"output.txt");
    run_string("setwrite \"output.txt");
    run_string("setwrite []");
    
    Result r = eval_string("writer");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_setwrite_unopened_file_error(void)
{
    Result r = run_string("setwrite \"notopen.txt");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

//==========================================================================
// Allopen Tests
//==========================================================================

void test_allopen_empty(void)
{
    Result r = eval_string("allopen");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_allopen_one_file(void)
{
    mock_fs_create_file("file.txt", "data");
    run_string("open \"file.txt");
    
    Result r = eval_string("allopen");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_FALSE(mem_is_nil(r.value.as.node));
    
    Node first = mem_car(r.value.as.node);
    TEST_ASSERT_TRUE(mem_is_word(first));
    TEST_ASSERT_EQUAL_STRING("file.txt", mem_word_ptr(first));
}

void test_allopen_multiple_files(void)
{
    mock_fs_create_file("a.txt", "a");
    mock_fs_create_file("b.txt", "b");
    
    run_string("open \"a.txt");
    run_string("open \"b.txt");
    
    Result r = eval_string("allopen");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    // Should have two elements
    Node list = r.value.as.node;
    TEST_ASSERT_FALSE(mem_is_nil(list));
    TEST_ASSERT_FALSE(mem_is_nil(mem_cdr(list)));
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(mem_cdr(list))));
}

//==========================================================================
// File Position Tests
//==========================================================================

void test_readpos_at_start(void)
{
    mock_fs_create_file("pos.txt", "hello world");
    run_string("open \"pos.txt");
    run_string("setread \"pos.txt");
    
    Result r = eval_string("readpos");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_number(r.value));
    TEST_ASSERT_EQUAL_FLOAT(0.0, r.value.as.number);
}

void test_readpos_after_read(void)
{
    mock_fs_create_file("pos.txt", "hello world");
    run_string("open \"pos.txt");
    run_string("setread \"pos.txt");
    eval_string("readchars 5");  // Read "hello"
    
    Result r = eval_string("readpos");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(5.0, r.value.as.number);
}

void test_setreadpos(void)
{
    mock_fs_create_file("pos.txt", "hello world");
    run_string("open \"pos.txt");
    run_string("setread \"pos.txt");
    run_string("setreadpos 6");
    
    Result r = eval_string("readchars 5");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("world", mem_word_ptr(r.value.as.node));
}

void test_readpos_keyboard_error(void)
{
    // Reader is keyboard by default
    Result r = eval_string("readpos");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_writepos_at_start(void)
{
    run_string("open \"pos.txt");
    run_string("setwrite \"pos.txt");
    
    Result r = eval_string("writepos");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_number(r.value));
    TEST_ASSERT_EQUAL_FLOAT(0.0, r.value.as.number);
}

void test_writepos_after_write(void)
{
    run_string("open \"pos.txt");
    run_string("setwrite \"pos.txt");
    run_string("type \"hello");
    
    Result r = eval_string("writepos");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(5.0, r.value.as.number);
}

void test_setwritepos(void)
{
    mock_fs_create_file("pos.txt", "hello world");
    run_string("open \"pos.txt");
    run_string("setwrite \"pos.txt");
    run_string("setwritepos 6");
    run_string("type \"WORLD");
    
    MockFile *file = mock_fs_get_file("pos.txt", false);
    TEST_ASSERT_EQUAL_STRING("hello WORLD", file->data);
}

void test_writepos_screen_error(void)
{
    // Writer is screen by default
    Result r = eval_string("writepos");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

//==========================================================================
// Filelen Tests
//==========================================================================

void test_filelen_returns_size(void)
{
    mock_fs_create_file("sized.txt", "12345678901234567890");  // 20 chars
    run_string("open \"sized.txt");
    
    Result r = eval_string("filelen \"sized.txt");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_number(r.value));
    TEST_ASSERT_EQUAL_FLOAT(20.0, r.value.as.number);
}

void test_filelen_empty_file(void)
{
    mock_fs_create_file("empty.txt", "");
    run_string("open \"empty.txt");
    
    Result r = eval_string("filelen \"empty.txt");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(0.0, r.value.as.number);
}

void test_filelen_unopened_file_error(void)
{
    mock_fs_create_file("notopen.txt", "data");
    
    Result r = eval_string("filelen \"notopen.txt");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

//==========================================================================
// Dribble Tests
//==========================================================================

void test_dribble_starts(void)
{
    Result r = run_string("dribble \"dribble.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_nodribble_stops(void)
{
    run_string("dribble \"dribble.txt");
    Result r = run_string("nodribble");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_nodribble_when_not_dribbling(void)
{
    // Should not error
    Result r = run_string("nodribble");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

//==========================================================================
// File I/O Integration Tests
//==========================================================================

void test_write_and_read_file(void)
{
    // Write to file
    run_string("open \"data.txt");
    run_string("setwrite \"data.txt");
    run_string("print \"hello");
    run_string("setwrite []");
    run_string("close \"data.txt");
    
    // Read from file
    run_string("open \"data.txt");
    run_string("setread \"data.txt");
    Result r = eval_string("readword");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("hello", mem_word_ptr(r.value.as.node));
}

void test_append_to_file(void)
{
    mock_fs_create_file("append.txt", "first\n");
    
    // Open file and set write position to end to simulate append
    run_string("open \"append.txt");
    run_string("setwrite \"append.txt");
    // Move write position to end of file (after "first\n" = 6 bytes)
    run_string("setwritepos 6");
    run_string("print \"second");
    run_string("setwrite []");
    run_string("close \"append.txt");
    
    MockFile *file = mock_fs_get_file("append.txt", false);
    TEST_ASSERT_EQUAL_STRING("first\nsecond\n", file->data);
}

void test_readlist_from_file(void)
{
    mock_fs_create_file("list.txt", "hello world\n");
    
    run_string("open \"list.txt");
    run_string("setread \"list.txt");
    Result r = eval_string("readlist");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("hello", mem_word_ptr(mem_car(list)));
    TEST_ASSERT_EQUAL_STRING("world", mem_word_ptr(mem_car(mem_cdr(list))));
}

void test_readchar_from_file(void)
{
    mock_fs_create_file("chars.txt", "ABC");
    
    run_string("open \"chars.txt");
    run_string("setread \"chars.txt");
    
    Result r1 = eval_string("readchar");
    TEST_ASSERT_EQUAL_STRING("A", mem_word_ptr(r1.value.as.node));
    
    Result r2 = eval_string("readchar");
    TEST_ASSERT_EQUAL_STRING("B", mem_word_ptr(r2.value.as.node));
    
    Result r3 = eval_string("readchar");
    TEST_ASSERT_EQUAL_STRING("C", mem_word_ptr(r3.value.as.node));
}

//==========================================================================
// Error Handling Tests
//==========================================================================

void test_open_invalid_input(void)
{
    Result r = run_string("open [not a word]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_close_invalid_input(void)
{
    Result r = run_string("close 123");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setread_invalid_input(void)
{
    Result r = run_string("setread 123");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setwrite_invalid_input(void)
{
    Result r = run_string("setwrite 123");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_filelen_invalid_input(void)
{
    Result r = eval_string("filelen [not a word]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setreadpos_invalid_input(void)
{
    mock_fs_create_file("pos.txt", "data");
    run_string("open \"pos.txt");
    run_string("setread \"pos.txt");
    
    Result r = run_string("setreadpos \"abc");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setreadpos_negative(void)
{
    mock_fs_create_file("pos.txt", "data");
    run_string("open \"pos.txt");
    run_string("setread \"pos.txt");
    
    Result r = run_string("setreadpos -1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setwritepos_invalid_input(void)
{
    run_string("open \"pos.txt");
    run_string("setwrite \"pos.txt");
    
    Result r = run_string("setwritepos \"abc");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setwritepos_negative(void)
{
    run_string("open \"pos.txt");
    run_string("setwrite \"pos.txt");
    
    Result r = run_string("setwritepos -1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}


//==========================================================================
// Directory listing tests: files, directories, catalog
//==========================================================================

void test_files_returns_list(void)
{
    // files should return a list (even if empty)
    Result r = eval_string("files");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
}

void test_files_with_extension_returns_list(void)
{
    // (files "txt") should return a list
    Result r = eval_string("(files \"txt)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
}

void test_files_with_star_returns_all(void)
{
    // (files "*") should return all files
    Result r = eval_string("(files \"*)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
}

void test_files_invalid_input_error(void)
{
    // (files [not a word]) should error
    Result r = eval_string("(files [not a word])");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_directories_returns_list(void)
{
    // directories should return a list (even if empty)
    Result r = eval_string("directories");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
}

void test_catalog_runs_without_error(void)
{
    // catalog should run without error (it prints to output)
    output_pos = 0;
    output_buffer[0] = '\0';
    Result r = run_string("catalog");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    // Output buffer should have something (or be empty if no files)
    // We just verify it doesn't crash
}

//==========================================================================
// Savepic/Loadpic Tests
//==========================================================================

// Special setup for savepic/loadpic tests - needs turtle and storage
// Note: This is called AFTER setUp(), so don't reinitialize mem/primitives
static void setUp_with_turtle(void)
{
    mock_fs_reset();
    
    // Initialize mock device (provides turtle with gfx_save/gfx_load)
    mock_device_init();
    
    // Re-initialize I/O with mock device console AND mock storage
    // (mock_storage was already initialized in setUp())
    logo_io_init(&mock_io, mock_device_get_console(), &mock_storage, NULL);
    primitives_set_io(&mock_io);
}

static void tearDown_with_turtle(void)
{
    logo_io_close_all(&mock_io);
    mock_fs_reset();
}

void test_savepic_creates_file(void)
{
    setUp_with_turtle();
    
    Result r = run_string("savepic \"test.bmp");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_NONE, r.status, "savepic should succeed");
    
    // Verify gfx_save was called
    TEST_ASSERT_EQUAL(1, mock_device_get_gfx_save_call_count());
    TEST_ASSERT_EQUAL_STRING("test.bmp", mock_device_get_last_gfx_save_filename());
    
    tearDown_with_turtle();
}

void test_savepic_file_exists_error(void)
{
    setUp_with_turtle();
    
    // Create an existing file
    mock_fs_create_file("exists.bmp", "existing content");
    
    Result r = run_string("savepic \"exists.bmp");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_ERROR, r.status, "savepic should error when file exists");
    
    // Verify gfx_save was NOT called (file exists check should fail first)
    TEST_ASSERT_EQUAL(0, mock_device_get_gfx_save_call_count());
    
    tearDown_with_turtle();
}

void test_savepic_disk_trouble_error(void)
{
    setUp_with_turtle();
    
    // Set up gfx_save to return an error
    mock_device_set_gfx_save_result(EIO);
    
    Result r = run_string("savepic \"trouble.bmp");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_ERROR, r.status, "savepic should error on disk trouble");
    
    // Verify gfx_save was called
    TEST_ASSERT_EQUAL(1, mock_device_get_gfx_save_call_count());
    
    tearDown_with_turtle();
}

void test_savepic_invalid_input_error(void)
{
    setUp_with_turtle();
    
    Result r = run_string("savepic [not a word]");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_ERROR, r.status, "savepic should error on non-word input");
    
    // Verify gfx_save was NOT called
    TEST_ASSERT_EQUAL(0, mock_device_get_gfx_save_call_count());
    
    tearDown_with_turtle();
}

void test_loadpic_loads_file(void)
{
    setUp_with_turtle();
    
    // Create the file to load
    mock_fs_create_file("picture.bmp", "BMP data");
    
    Result r = run_string("loadpic \"picture.bmp");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_NONE, r.status, "loadpic should succeed");
    
    // Verify gfx_load was called
    TEST_ASSERT_EQUAL(1, mock_device_get_gfx_load_call_count());
    TEST_ASSERT_EQUAL_STRING("picture.bmp", mock_device_get_last_gfx_load_filename());
    
    tearDown_with_turtle();
}

void test_loadpic_file_not_found_error(void)
{
    setUp_with_turtle();
    
    Result r = run_string("loadpic \"missing.bmp");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_ERROR, r.status, "loadpic should error when file not found");
    
    // Verify gfx_load was NOT called (file exists check should fail first)
    TEST_ASSERT_EQUAL(0, mock_device_get_gfx_load_call_count());
    
    tearDown_with_turtle();
}

void test_loadpic_wrong_type_error(void)
{
    setUp_with_turtle();
    
    // Create the file to load
    mock_fs_create_file("badpic.bmp", "bad data");
    
    // Set up gfx_load to return EINVAL (wrong file type)
    mock_device_set_gfx_load_result(EINVAL);
    
    Result r = run_string("loadpic \"badpic.bmp");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_ERROR, r.status, "loadpic should error on wrong file type");
    
    // Verify gfx_load was called
    TEST_ASSERT_EQUAL(1, mock_device_get_gfx_load_call_count());
    
    tearDown_with_turtle();
}

void test_loadpic_invalid_input_error(void)
{
    setUp_with_turtle();
    
    Result r = run_string("loadpic [not a word]");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_ERROR, r.status, "loadpic should error on non-word input");
    
    // Verify gfx_load was NOT called
    TEST_ASSERT_EQUAL(0, mock_device_get_gfx_load_call_count());
    
    tearDown_with_turtle();
}

void test_savepic_with_prefix(void)
{
    setUp_with_turtle();
    
    // Set prefix after setUp_with_turtle
    // Note: use prefix without trailing slash - resolve_path will add separator
    Result pr = run_string("setprefix \"pics");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_NONE, pr.status, "setprefix should succeed");
    
    Result r = run_string("savepic \"test.bmp");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_NONE, r.status, "savepic with prefix should succeed");
    
    // Verify gfx_save was called with full path
    TEST_ASSERT_EQUAL(1, mock_device_get_gfx_save_call_count());
    TEST_ASSERT_EQUAL_STRING("pics/test.bmp", mock_device_get_last_gfx_save_filename());
    
    tearDown_with_turtle();
}

void test_loadpic_with_prefix(void)
{
    setUp_with_turtle();
    
    // Create the file to load with prefix path
    mock_fs_create_file("pics/test.bmp", "BMP data");
    
    // Set prefix after setUp_with_turtle
    // Note: use prefix without trailing slash - resolve_path will add separator
    Result pr = run_string("setprefix \"pics");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_NONE, pr.status, "setprefix should succeed");
    
    Result r = run_string("loadpic \"test.bmp");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_NONE, r.status, "loadpic with prefix should succeed");
    
    // Verify gfx_load was called with full path
    TEST_ASSERT_EQUAL(1, mock_device_get_gfx_load_call_count());
    TEST_ASSERT_EQUAL_STRING("pics/test.bmp", mock_device_get_last_gfx_load_filename());
    
    tearDown_with_turtle();
}

//==========================================================================
// Directory Management Tests
//==========================================================================

void test_createdir(void)
{
    Result r = run_string("createdir \"newdir");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    TEST_ASSERT_TRUE(mock_storage_dir_exists("newdir"));
}

void test_erasedir(void)
{
    mock_storage_dir_create("todelete");
    
    Result r = run_string("erasedir \"todelete");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    TEST_ASSERT_FALSE(mock_storage_dir_exists("todelete"));
}

void test_erasefile(void)
{
    mock_fs_create_file("todelete.txt", "data");
    
    Result r = run_string("erasefile \"todelete.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    TEST_ASSERT_FALSE(mock_storage_file_exists("todelete.txt"));
}

void test_filep_true(void)
{
    mock_fs_create_file("exists.txt", "data");
    
    Result r = eval_string("file? \"exists.txt");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_filep_false(void)
{
    Result r = eval_string("file? \"missing.txt");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_dirp_true(void)
{
    mock_storage_dir_create("exists");
    
    Result r = eval_string("dir? \"exists");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_dirp_false(void)
{
    Result r = eval_string("dir? \"missing");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_rename_file(void)
{
    mock_fs_create_file("old.txt", "data");
    
    Result r = run_string("rename \"old.txt \"new.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    TEST_ASSERT_FALSE(mock_storage_file_exists("old.txt"));
    TEST_ASSERT_TRUE(mock_storage_file_exists("new.txt"));
}

void test_setprefix_and_prefix(void)
{
    Result r = run_string("setprefix \"my\\/path");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    Result r2 = eval_string("prefix");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_STRING("my\\/path", mem_word_ptr(r2.value.as.node));
}

//==========================================================================
// Load/Save Tests
//==========================================================================

void test_load_executes_file(void)
{
    mock_fs_create_file("script.logo", "make \"x 10\nmake \"y 20\n");
    
    Result r = run_string("load \"script.logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Check if variables were set
    Value val;
    TEST_ASSERT_TRUE(var_get("x", &val));
    TEST_ASSERT_EQUAL_FLOAT(10.0, val.as.number);
    TEST_ASSERT_TRUE(var_get("y", &val));
    TEST_ASSERT_EQUAL_FLOAT(20.0, val.as.number);
}

void test_load_defines_procedure(void)
{
    mock_fs_create_file("proc.logo", "to testproc\nmake \"x 100\nend\n");
    
    Result r = run_string("load \"proc.logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Check if procedure is defined
    TEST_ASSERT_TRUE(proc_exists("testproc"));
    
    // Run it
    run_string("testproc");
    Value val;
    TEST_ASSERT_TRUE(var_get("x", &val));
    TEST_ASSERT_EQUAL_FLOAT(100.0, val.as.number);
}

void test_load_runs_startup_from_file(void)
{
    // Create a file that sets startup variable
    mock_fs_create_file("startup.logo", "make \"startup [make \"ran_startup 1]\n");
    
    // Ensure startup doesn't exist before loading
    TEST_ASSERT_FALSE(var_exists("startup"));
    TEST_ASSERT_FALSE(var_exists("ran_startup"));
    
    Result r = run_string("load \"startup.logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // The startup should have been executed
    Value val;
    TEST_ASSERT_TRUE(var_get("ran_startup", &val));
    TEST_ASSERT_EQUAL_FLOAT(1.0, val.as.number);
}

void test_load_does_not_run_preexisting_startup(void)
{
    // Set up a startup variable before loading
    run_string("make \"startup [make \"ran_startup 1]");
    TEST_ASSERT_TRUE(var_exists("startup"));
    TEST_ASSERT_FALSE(var_exists("ran_startup"));
    
    // Create a file that does NOT set startup
    mock_fs_create_file("nostart.logo", "make \"loaded 1\n");
    
    Result r = run_string("load \"nostart.logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // The preexisting startup should NOT have been executed
    TEST_ASSERT_FALSE(var_exists("ran_startup"));
    
    // But the file contents should have executed
    Value val;
    TEST_ASSERT_TRUE(var_get("loaded", &val));
    TEST_ASSERT_EQUAL_FLOAT(1.0, val.as.number);
}

void test_load_runs_startup_when_file_overwrites(void)
{
    // Set up a startup variable before loading
    run_string("make \"startup [make \"ran_old_startup 1]");
    TEST_ASSERT_TRUE(var_exists("startup"));
    
    // Create a file that sets a different startup
    mock_fs_create_file("newstart.logo", "make \"startup [make \"ran_new_startup 1]\n");
    
    Result r = run_string("load \"newstart.logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // The old startup should NOT have been executed (it was overwritten)
    TEST_ASSERT_FALSE(var_exists("ran_old_startup"));
    // The new startup FROM THE FILE should have been executed
    Value val;
    TEST_ASSERT_TRUE(var_get("ran_new_startup", &val));
    TEST_ASSERT_EQUAL_FLOAT(1.0, val.as.number);
}

void test_save_writes_workspace(void)
{
    // Setup workspace
    run_string("define \"testproc [[] [print \"hello]]");
    run_string("make \"myvar 123");
    
    Result r = run_string("save \"workspace.logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Check file content
    MockFile *file = mock_fs_get_file("workspace.logo", false);
    TEST_ASSERT_NOT_NULL(file);
    
    // Should contain procedure and variable
    TEST_ASSERT_NOT_NULL(strstr(file->data, "to testproc"));
    TEST_ASSERT_NOT_NULL(strstr(file->data, "make \"myvar 123"));
}

void test_save_format_matches_poall(void)
{
    // Setup workspace with a simple procedure using define (no newlines)
    // Note: define creates a flat body list, so all instructions are on one line
    run_string("define \"testproc [[x y] [print :x] [print :y]]");
    run_string("make \"myvar [hello world]");
    
    Result r = run_string("save \"formatted.logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Check file content has proper formatting
    MockFile *file = mock_fs_get_file("formatted.logo", false);
    TEST_ASSERT_NOT_NULL(file);
    
    // Procedure should have proper formatting with indentation
    // With define, the body is flattened to one line with base indent
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(file->data, "to testproc :x :y\n"),
        "Title line should be formatted correctly");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(file->data, "  print :x print :y\n"),
        "Body should have 2-space indent and be on one line");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(file->data, "end\n"),
        "End should be present");
    
    // Variable should be properly formatted
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(file->data, "make \"myvar [hello world]\n"),
        "Variable should be formatted like make command");
}

void test_save_file_exists_error(void)
{
    mock_fs_create_file("exists.logo", "");
    
    Result r = run_string("save \"exists.logo");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

//==========================================================================
// Prefix Handling Tests
//==========================================================================

void test_open_close_with_prefix(void)
{
    // Create a file in a subdirectory
    mock_fs_create_file("subdir/file.txt", "content");
    
    // Set prefix
    Result pr = run_string("setprefix \"subdir");
    TEST_ASSERT_EQUAL(RESULT_NONE, pr.status);
    
    // Open the file using just the filename (prefix should resolve)
    Result r1 = run_string("open \"file.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r1.status);
    
    // Close using just the filename 
    Result r2 = run_string("close \"file.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r2.status);
}

void test_setread_setwrite_with_prefix(void)
{
    // Create a file in a subdirectory
    mock_fs_create_file("mydir/data.txt", "test data");
    
    // Set prefix
    run_string("setprefix \"mydir");
    
    // Open and set as reader
    run_string("open \"data.txt");
    Result r = run_string("setread \"data.txt");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_NONE, r.status, "setread with prefix should succeed");
    
    // Reset reader
    run_string("setread []");
    run_string("close \"data.txt");
}

void test_load_with_prefix(void)
{
    mock_fs_create_file("scripts/init.logo", "make \"loaded 42\n");
    
    run_string("setprefix \"scripts");
    
    Result r = run_string("load \"init.logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Verify the file was loaded
    Value val;
    TEST_ASSERT_TRUE(var_get("loaded", &val));
    TEST_ASSERT_EQUAL_FLOAT(42.0, val.as.number);
}

void test_save_with_prefix(void)
{
    // Set up
    run_string("make \"testvar 99");
    
    run_string("setprefix \"saves");
    
    Result r = run_string("save \"test.logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Verify the file was created at the right path
    MockFile *file = mock_fs_get_file("saves/test.logo", false);
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_NOT_NULL(strstr(file->data, "make \"testvar 99"));
}

//==========================================================================
// Pofile Tests
//==========================================================================

void test_pofile_prints_file_contents(void)
{
    mock_fs_create_file("test.txt", "Hello World\nSecond line\n");
    
    reset_output();
    Result r = run_string("pofile \"test.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Output should contain the file contents
    TEST_ASSERT_TRUE(strstr(output_buffer, "Hello World") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "Second line") != NULL);
}

void test_pofile_empty_file(void)
{
    mock_fs_create_file("empty.txt", "");
    
    reset_output();
    Result r = run_string("pofile \"empty.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Output should be empty (no lines)
    TEST_ASSERT_EQUAL_STRING("", output_buffer);
}

void test_pofile_file_not_found(void)
{
    Result r = run_string("pofile \"missing.txt");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_pofile_already_open_error(void)
{
    mock_fs_create_file("open.txt", "content");
    
    // Open the file first
    Result r1 = run_string("open \"open.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r1.status);
    
    // Now pofile should fail because file is already open
    Result r2 = run_string("pofile \"open.txt");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r2.status);
}

void test_pofile_invalid_input(void)
{
    Result r = run_string("pofile [not a word]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_pofile_with_prefix(void)
{
    mock_fs_create_file("subdir/test.txt", "Prefixed content\n");
    
    run_string("setprefix \"subdir");
    
    reset_output();
    Result r = run_string("pofile \"test.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    TEST_ASSERT_TRUE(strstr(output_buffer, "Prefixed content") != NULL);
}

//==========================================================================
// Main
//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    
    // Open/Close tests
    RUN_TEST(test_open_creates_new_file);
    RUN_TEST(test_open_existing_file);
    RUN_TEST(test_close_file);
    RUN_TEST(test_close_unopened_file_error);
    RUN_TEST(test_closeall);
    
    // Reader/Writer tests
    RUN_TEST(test_reader_default_keyboard);
    RUN_TEST(test_writer_default_screen);
    RUN_TEST(test_setread_to_file);
    RUN_TEST(test_setread_back_to_keyboard);
    RUN_TEST(test_setread_unopened_file_error);
    RUN_TEST(test_setwrite_to_file);
    RUN_TEST(test_setwrite_back_to_screen);
    RUN_TEST(test_setwrite_unopened_file_error);
    
    // Allopen tests
    RUN_TEST(test_allopen_empty);
    RUN_TEST(test_allopen_one_file);
    RUN_TEST(test_allopen_multiple_files);
    
    // File position tests
    RUN_TEST(test_readpos_at_start);
    RUN_TEST(test_readpos_after_read);
    RUN_TEST(test_setreadpos);
    RUN_TEST(test_readpos_keyboard_error);
    RUN_TEST(test_writepos_at_start);
    RUN_TEST(test_writepos_after_write);
    RUN_TEST(test_setwritepos);
    RUN_TEST(test_writepos_screen_error);
    
    // Filelen tests
    RUN_TEST(test_filelen_returns_size);
    RUN_TEST(test_filelen_empty_file);
    RUN_TEST(test_filelen_unopened_file_error);
    
    // Dribble tests
    RUN_TEST(test_dribble_starts);
    RUN_TEST(test_nodribble_stops);
    RUN_TEST(test_nodribble_when_not_dribbling);
    
    // Integration tests
    RUN_TEST(test_write_and_read_file);
    RUN_TEST(test_append_to_file);
    RUN_TEST(test_readlist_from_file);
    RUN_TEST(test_readchar_from_file);
    
    // Error handling tests
    RUN_TEST(test_open_invalid_input);
    RUN_TEST(test_close_invalid_input);
    RUN_TEST(test_setread_invalid_input);
    RUN_TEST(test_setwrite_invalid_input);
    RUN_TEST(test_filelen_invalid_input);
    RUN_TEST(test_setreadpos_invalid_input);
    RUN_TEST(test_setreadpos_negative);
    RUN_TEST(test_setwritepos_invalid_input);
    RUN_TEST(test_setwritepos_negative);
    
    // Directory listing tests
    RUN_TEST(test_files_returns_list);
    RUN_TEST(test_files_with_extension_returns_list);
    RUN_TEST(test_files_with_star_returns_all);
    RUN_TEST(test_files_invalid_input_error);
    RUN_TEST(test_directories_returns_list);
    RUN_TEST(test_catalog_runs_without_error);

    // Savepic/Loadpic tests
    RUN_TEST(test_savepic_creates_file);
    RUN_TEST(test_savepic_file_exists_error);
    RUN_TEST(test_savepic_disk_trouble_error);
    RUN_TEST(test_savepic_invalid_input_error);
    RUN_TEST(test_loadpic_loads_file);
    RUN_TEST(test_loadpic_file_not_found_error);
    RUN_TEST(test_loadpic_wrong_type_error);
    RUN_TEST(test_loadpic_invalid_input_error);
    RUN_TEST(test_savepic_with_prefix);
    RUN_TEST(test_loadpic_with_prefix);

    // Directory Management tests
    RUN_TEST(test_createdir);
    RUN_TEST(test_erasedir);
    RUN_TEST(test_erasefile);
    RUN_TEST(test_filep_true);
    RUN_TEST(test_filep_false);
    RUN_TEST(test_dirp_true);
    RUN_TEST(test_dirp_false);
    RUN_TEST(test_rename_file);
    RUN_TEST(test_setprefix_and_prefix);

    // Load/Save tests
    RUN_TEST(test_load_executes_file);
    RUN_TEST(test_load_defines_procedure);
    RUN_TEST(test_load_runs_startup_from_file);
    RUN_TEST(test_load_does_not_run_preexisting_startup);
    RUN_TEST(test_load_runs_startup_when_file_overwrites);
    RUN_TEST(test_save_writes_workspace);
    RUN_TEST(test_save_format_matches_poall);
    RUN_TEST(test_save_file_exists_error);

    // Prefix handling tests
    RUN_TEST(test_open_close_with_prefix);
    RUN_TEST(test_setread_setwrite_with_prefix);
    RUN_TEST(test_load_with_prefix);
    RUN_TEST(test_save_with_prefix);

    // Pofile tests
    RUN_TEST(test_pofile_prints_file_contents);
    RUN_TEST(test_pofile_empty_file);
    RUN_TEST(test_pofile_file_not_found);
    RUN_TEST(test_pofile_already_open_error);
    RUN_TEST(test_pofile_invalid_input);
    RUN_TEST(test_pofile_with_prefix);

    return UNITY_END();
}
