//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for Files primitives: open, openread, openwrite, openappend, openupdate, close, closeall,
//                              setread, setwrite, reader, writer, allopen, readpos, setreadpos,
//                              writepos, setwritepos, filelen, dribble, nodribble, load, save
//

#include "test_scaffold.h"
#include <stdlib.h>
#include <string.h>

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
} MockFile;

// Mock file storage
static MockFile mock_files[MOCK_MAX_FILES];

// Mock file stream context
typedef struct MockFileContext
{
    MockFile *file;
    size_t read_pos;
    size_t write_pos;
    LogoFileMode mode;
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
    }
}

// Reset the mock file system
static void mock_fs_reset(void)
{
    for (int i = 0; i < MOCK_MAX_FILES; i++)
    {
        mock_files[i].exists = false;
        mock_files[i].name[0] = '\0';
        mock_files[i].data[0] = '\0';
        mock_files[i].size = 0;
    }
}

// Mock file opener callback
static LogoStream *mock_file_opener(const char *pathname, LogoFileMode mode)
{
    MockFile *file = NULL;
    
    switch (mode)
    {
    case LOGO_FILE_READ:
    case LOGO_FILE_UPDATE:
        // File must exist
        file = mock_fs_get_file(pathname, false);
        if (!file)
            return NULL;
        break;
        
    case LOGO_FILE_WRITE:
        // Create/truncate file
        file = mock_fs_get_file(pathname, true);
        if (!file)
            return NULL;
        file->size = 0;
        file->data[0] = '\0';
        break;
        
    case LOGO_FILE_APPEND:
        // Create if doesn't exist
        file = mock_fs_get_file(pathname, true);
        if (!file)
            return NULL;
        break;
    }
    
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
    ctx->write_pos = (mode == LOGO_FILE_APPEND) ? file->size : 0;
    ctx->mode = mode;
    
    logo_stream_init(stream, LOGO_STREAM_FILE, &mock_file_ops, ctx, pathname);
    stream->is_open = true;
    
    return stream;
}

//==========================================================================
// Test Setup/Teardown
//==========================================================================

void setUp(void)
{
    test_scaffold_setUp();
    mock_fs_reset();
    
    // Set the mock file opener
    logo_io_set_file_opener(&mock_io, mock_file_opener);
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

void test_openread_existing_file(void)
{
    mock_fs_create_file("existing.txt", "hello world");
    
    Result r = run_string("openread \"existing.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_openread_nonexistent_file_error(void)
{
    Result r = run_string("openread \"nonexistent.txt");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_openwrite_creates_file(void)
{
    Result r = run_string("openwrite \"newfile.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    MockFile *file = mock_fs_get_file("newfile.txt", false);
    TEST_ASSERT_NOT_NULL(file);
}

void test_openwrite_truncates_existing(void)
{
    mock_fs_create_file("existing.txt", "original content");
    
    Result r = run_string("openwrite \"existing.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    MockFile *file = mock_fs_get_file("existing.txt", false);
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL(0, file->size);
}

void test_openappend_creates_file(void)
{
    Result r = run_string("openappend \"appendfile.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    MockFile *file = mock_fs_get_file("appendfile.txt", false);
    TEST_ASSERT_NOT_NULL(file);
}

void test_openupdate_existing_file(void)
{
    mock_fs_create_file("update.txt", "data");
    
    Result r = run_string("openupdate \"update.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_openupdate_nonexistent_file_error(void)
{
    Result r = run_string("openupdate \"nonexistent.txt");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_close_file(void)
{
    mock_fs_create_file("toclose.txt", "data");
    run_string("openread \"toclose.txt");
    
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
    
    run_string("openread \"file1.txt");
    run_string("openread \"file2.txt");
    
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
    run_string("openread \"input.txt");
    run_string("setread \"input.txt");
    
    Result r = eval_string("reader");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("input.txt", mem_word_ptr(r.value.as.node));
}

void test_setread_back_to_keyboard(void)
{
    mock_fs_create_file("input.txt", "content");
    run_string("openread \"input.txt");
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
    run_string("openwrite \"output.txt");
    run_string("setwrite \"output.txt");
    
    Result r = eval_string("writer");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("output.txt", mem_word_ptr(r.value.as.node));
}

void test_setwrite_back_to_screen(void)
{
    run_string("openwrite \"output.txt");
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
    run_string("openread \"file.txt");
    
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
    
    run_string("openread \"a.txt");
    run_string("openread \"b.txt");
    
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
    run_string("openread \"pos.txt");
    run_string("setread \"pos.txt");
    
    Result r = eval_string("readpos");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_number(r.value));
    TEST_ASSERT_EQUAL_FLOAT(0.0, r.value.as.number);
}

void test_readpos_after_read(void)
{
    mock_fs_create_file("pos.txt", "hello world");
    run_string("openread \"pos.txt");
    run_string("setread \"pos.txt");
    eval_string("readchars 5");  // Read "hello"
    
    Result r = eval_string("readpos");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(5.0, r.value.as.number);
}

void test_setreadpos(void)
{
    mock_fs_create_file("pos.txt", "hello world");
    run_string("openread \"pos.txt");
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
    run_string("openwrite \"pos.txt");
    run_string("setwrite \"pos.txt");
    
    Result r = eval_string("writepos");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_number(r.value));
    TEST_ASSERT_EQUAL_FLOAT(0.0, r.value.as.number);
}

void test_writepos_after_write(void)
{
    run_string("openwrite \"pos.txt");
    run_string("setwrite \"pos.txt");
    run_string("type \"hello");
    
    Result r = eval_string("writepos");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(5.0, r.value.as.number);
}

void test_setwritepos(void)
{
    mock_fs_create_file("pos.txt", "hello world");
    run_string("openupdate \"pos.txt");
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
    run_string("openread \"sized.txt");
    
    Result r = eval_string("filelen \"sized.txt");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_number(r.value));
    TEST_ASSERT_EQUAL_FLOAT(20.0, r.value.as.number);
}

void test_filelen_empty_file(void)
{
    mock_fs_create_file("empty.txt", "");
    run_string("openread \"empty.txt");
    
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
    run_string("openwrite \"data.txt");
    run_string("setwrite \"data.txt");
    run_string("print \"hello");
    run_string("setwrite []");
    run_string("close \"data.txt");
    
    // Read from file
    run_string("openread \"data.txt");
    run_string("setread \"data.txt");
    Result r = eval_string("readword");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("hello", mem_word_ptr(r.value.as.node));
}

void test_append_to_file(void)
{
    mock_fs_create_file("append.txt", "first\n");
    
    run_string("openappend \"append.txt");
    run_string("setwrite \"append.txt");
    run_string("print \"second");
    run_string("setwrite []");
    run_string("close \"append.txt");
    
    MockFile *file = mock_fs_get_file("append.txt", false);
    TEST_ASSERT_EQUAL_STRING("first\nsecond\n", file->data);
}

void test_readlist_from_file(void)
{
    mock_fs_create_file("list.txt", "hello world\n");
    
    run_string("openread \"list.txt");
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
    
    run_string("openread \"chars.txt");
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
    run_string("openread \"pos.txt");
    run_string("setread \"pos.txt");
    
    Result r = run_string("setreadpos \"abc");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setreadpos_negative(void)
{
    mock_fs_create_file("pos.txt", "data");
    run_string("openread \"pos.txt");
    run_string("setread \"pos.txt");
    
    Result r = run_string("setreadpos -1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setwritepos_invalid_input(void)
{
    run_string("openwrite \"pos.txt");
    run_string("setwrite \"pos.txt");
    
    Result r = run_string("setwritepos \"abc");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setwritepos_negative(void)
{
    run_string("openwrite \"pos.txt");
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
// Main
//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    
    // Open/Close tests
    RUN_TEST(test_open_creates_new_file);
    RUN_TEST(test_openread_existing_file);
    RUN_TEST(test_openread_nonexistent_file_error);
    RUN_TEST(test_openwrite_creates_file);
    RUN_TEST(test_openwrite_truncates_existing);
    RUN_TEST(test_openappend_creates_file);
    RUN_TEST(test_openupdate_existing_file);
    RUN_TEST(test_openupdate_nonexistent_file_error);
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

    return UNITY_END();
}
