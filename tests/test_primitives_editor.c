//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "test_scaffold.h"
#include <stdlib.h>
#include <string.h>

//==========================================================================
// Mock File System for editfile tests
//==========================================================================

#define MOCK_MAX_FILES 10
#define MOCK_FILE_SIZE 8192

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

// Get mock file content
static const char *mock_fs_get_content(const char *name)
{
    MockFile *file = mock_fs_get_file(name, false);
    return file ? file->data : NULL;
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

// Mock file opener - creates file if it doesn't exist
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
    return file != NULL;
}

static bool mock_storage_dir_exists(const char *pathname)
{
    (void)pathname;
    return false;  // No directories in simple mock
}

static bool mock_storage_file_delete(const char *pathname)
{
    MockFile *file = mock_fs_get_file(pathname, false);
    if (!file)
        return false;
    file->exists = false;
    file->size = 0;
    file->data[0] = '\0';
    return true;
}

static bool mock_storage_dir_create(const char *pathname)
{
    (void)pathname;
    return false;  // Not supported in simple mock
}

static bool mock_storage_dir_delete(const char *pathname)
{
    (void)pathname;
    return false;  // Not supported in simple mock
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
    (void)pathname;
    (void)callback;
    (void)user_data;
    (void)filter;
    return true;  // No-op for simple mock
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

// Flag to indicate if we need file system support
static bool use_mock_fs = false;

void setUp(void)
{
    // Use device setup to get mock editor support
    test_scaffold_setUp_with_device();
    
    // Also set up mock file system for editfile tests
    mock_fs_reset();
    use_mock_fs = false;
}

// Setup with mock file system support for editfile tests
static void setUp_with_storage(void)
{
    test_scaffold_setUp_with_device();
    mock_fs_reset();
    use_mock_fs = true;
    
    // Initialize mock storage with our operations
    logo_storage_init(&mock_storage, &mock_storage_ops);
    
    // Re-initialize I/O with mock storage
    logo_io_init(&mock_io, mock_device_get_console(), &mock_storage, NULL);
    primitives_set_io(&mock_io);
}

void tearDown(void)
{
    if (use_mock_fs)
    {
        logo_io_close_all(&mock_io);
        mock_fs_reset();
    }
    test_scaffold_tearDown();
}

//==========================================================================
// Editor Primitive Tests
//==========================================================================

void test_edit_requires_editor(void)
{
    // Define a procedure
    const char *params[] = {};
    define_proc("myproc", params, 0, "print 1");
    
    // Mock editor should be called when edit is invoked
    mock_device_clear_editor();
    
    Result r = run_string("edit \"myproc");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(mock_device_was_editor_called());
}

void test_edit_formats_procedure_definition(void)
{
    // Define a procedure
    const char *params[] = {};
    define_proc("hello", params, 0, "print \"world");
    
    mock_device_clear_editor();
    
    run_string("edit \"hello");
    
    // Check the input passed to editor contains po format
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "to hello"));
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "print"));
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "end"));
}

void test_edit_with_parameters(void)
{
    Node p = mem_atom("x", 1);
    const char *params[] = {mem_word_ptr(p)};
    define_proc("double", params, 1, "output :x * 2");
    
    mock_device_clear_editor();
    
    run_string("edit \"double");
    
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "to double :x"));
}

void test_edit_list_of_procedures(void)
{
    const char *params[] = {};
    define_proc("proca", params, 0, "print 1");
    define_proc("procb", params, 0, "print 2");
    
    mock_device_clear_editor();
    
    run_string("edit [proca procb]");
    
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "to proca"));
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "to procb"));
    
    // Check for blank line between definitions
    const char *proca_end = strstr(editor_input, "end\n");
    TEST_ASSERT_NOT_NULL(proca_end);
    const char *blank_line = strstr(proca_end, "\n\nto procb");
    TEST_ASSERT_NOT_NULL(blank_line);
}

void test_edit_undefined_procedure_opens_with_template(void)
{
    mock_device_clear_editor();
    
    Result r = run_string("edit \"newproc");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(mock_device_was_editor_called());
    
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_EQUAL_STRING("to newproc\n", editor_input);
}

void test_edit_list_with_undefined_procedure_opens_with_template(void)
{
    // edit [test2] should open editor with template for test2
    mock_device_clear_editor();
    
    Result r = run_string("edit [test2]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(mock_device_was_editor_called());
    
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_EQUAL_STRING("to test2\n", editor_input);
}

void test_edit_undefined_procedure_accept_creates_procedure(void)
{
    mock_device_clear_editor();
    mock_device_set_editor_result(LOGO_EDITOR_ACCEPT);
    mock_device_set_editor_content("to newproc\nprint \"hello\nend\n");
    
    Result r = run_string("edit \"newproc");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(proc_exists("newproc"));
}

void test_edit_cancel_does_nothing(void)
{
    const char *params[] = {};
    define_proc("myproc", params, 0, "print 1");
    
    mock_device_clear_editor();
    mock_device_set_editor_result(LOGO_EDITOR_CANCEL);
    
    Result r = run_string("edit \"myproc");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Procedure should be unchanged
    TEST_ASSERT_TRUE(proc_exists("myproc"));
}

void test_edit_accept_redefines_procedure(void)
{
    const char *params[] = {};
    define_proc("myproc", params, 0, "print 1");
    
    mock_device_clear_editor();
    mock_device_set_editor_result(LOGO_EDITOR_ACCEPT);
    mock_device_set_editor_content("to myproc\nprint 2\nend\n");
    
    Result r = run_string("edit \"myproc");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Procedure should be redefined
    TEST_ASSERT_TRUE(proc_exists("myproc"));
}

void test_ed_abbreviation(void)
{
    const char *params[] = {};
    define_proc("myproc", params, 0, "print 1");
    
    mock_device_clear_editor();
    
    Result r = run_string("ed \"myproc");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(mock_device_was_editor_called());
}

void test_edn_formats_variable(void)
{
    run_string("make \"myvar 42");
    
    mock_device_clear_editor();
    
    run_string("edn \"myvar");
    
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "make \"myvar 42"));
}

void test_edn_formats_word_variable(void)
{
    run_string("make \"myvar \"hello");
    
    mock_device_clear_editor();
    
    run_string("edn \"myvar");
    
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "make \"myvar \"hello"));
}

void test_edn_formats_list_variable(void)
{
    run_string("make \"myvar [1 2 3]");
    
    mock_device_clear_editor();
    
    run_string("edn \"myvar");
    
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "make \"myvar [1 2 3]"));
}

void test_edn_list_of_variables(void)
{
    run_string("make \"vara 1");
    run_string("make \"varb 2");
    
    mock_device_clear_editor();
    
    run_string("edn [vara varb]");
    
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "make \"vara 1"));
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "make \"varb 2"));
}

void test_edn_unknown_variable_error(void)
{
    mock_device_clear_editor();
    
    Result r = run_string("edn \"nonexistent");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NO_VALUE, r.error_code);
}

void test_edns_formats_all_variables(void)
{
    run_string("make \"vara 1");
    run_string("make \"varb 2");
    
    mock_device_clear_editor();
    
    run_string("edns");
    
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "make \"vara"));
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "make \"varb"));
}

void test_edit_no_args_preserves_buffer(void)
{
    // First, edit a procedure to populate the buffer
    const char *params[] = {};
    define_proc("spiral", params, 0, "print 1");
    
    mock_device_clear_editor();
    mock_device_set_editor_result(LOGO_EDITOR_CANCEL);  // Cancel to keep buffer
    
    Result r = run_string("edit \"spiral");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(mock_device_was_editor_called());
    
    // Verify the buffer had content from spiral
    const char *first_input = mock_device_get_editor_input();
    TEST_ASSERT_NOT_NULL(strstr(first_input, "to spiral"));
    TEST_ASSERT_NOT_NULL(strstr(first_input, "print 1"));
    TEST_ASSERT_NOT_NULL(strstr(first_input, "end"));
    
    // Now call (edit) with no args - should preserve the buffer
    mock_device_clear_editor();
    mock_device_set_editor_result(LOGO_EDITOR_CANCEL);
    
    r = run_string("(edit)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(mock_device_was_editor_called());
    
    // Buffer should still have the spiral content
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "to spiral"));
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "print 1"));
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "end"));
}

void test_edit_runs_regular_commands(void)
{
    // Editor content should be run as if typed at top level
    mock_device_clear_editor();
    mock_device_set_editor_result(LOGO_EDITOR_ACCEPT);
    mock_device_set_editor_content("make \"myvar 42\n");
    
    // Ensure variable doesn't exist first
    Value dummy;
    TEST_ASSERT_FALSE(var_get("myvar", &dummy));
    
    Result r = run_string("(edit)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Variable should now exist
    Value value;
    TEST_ASSERT_TRUE(var_get("myvar", &value));
    TEST_ASSERT_EQUAL(VALUE_NUMBER, value.type);
    TEST_ASSERT_EQUAL(42, value.as.number);
}

void test_edit_runs_multiple_commands(void)
{
    mock_device_clear_editor();
    mock_device_set_editor_result(LOGO_EDITOR_ACCEPT);
    mock_device_set_editor_content("make \"x 10\nmake \"y 20\n");
    
    Result r = run_string("(edit)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    Value value;
    TEST_ASSERT_TRUE(var_get("x", &value));
    TEST_ASSERT_EQUAL(10, value.as.number);
    
    TEST_ASSERT_TRUE(var_get("y", &value));
    TEST_ASSERT_EQUAL(20, value.as.number);
}

void test_edit_runs_mixed_content(void)
{
    // Test both procedure definition and regular commands
    mock_device_clear_editor();
    mock_device_set_editor_result(LOGO_EDITOR_ACCEPT);
    mock_device_set_editor_content("make \"before 1\nto myproc\nprint \"hello\nend\nmake \"after 2\n");
    
    Result r = run_string("(edit)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Both variables should exist
    Value value;
    TEST_ASSERT_TRUE(var_get("before", &value));
    TEST_ASSERT_EQUAL(1, value.as.number);
    
    TEST_ASSERT_TRUE(var_get("after", &value));
    TEST_ASSERT_EQUAL(2, value.as.number);
    
    // And procedure should exist
    TEST_ASSERT_TRUE(proc_exists("myproc"));
}

void test_edall_formats_all_procedures(void)
{
    const char *params[] = {};
    define_proc("proca", params, 0, "print 1");
    define_proc("procb", params, 0, "print 2");
    
    mock_device_clear_editor();
    
    run_string("edall");
    
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "to proca"));
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "to procb"));
}

void test_edall_formats_all_variables(void)
{
    run_string("make \"vara 1");
    run_string("make \"varb 2");
    
    mock_device_clear_editor();
    
    run_string("edall");
    
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "make \"vara"));
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "make \"varb"));
}

void test_edall_formats_procedures_and_variables(void)
{
    const char *params[] = {};
    define_proc("myproc", params, 0, "print 1");
    run_string("make \"myvar 42");
    
    mock_device_clear_editor();
    
    run_string("edall");
    
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "to myproc"));
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "make \"myvar 42"));
}

void test_edall_excludes_buried_procedures(void)
{
    const char *params[] = {};
    define_proc("visible", params, 0, "print 1");
    define_proc("hidden", params, 0, "print 2");
    run_string("bury \"hidden");
    
    mock_device_clear_editor();
    
    run_string("edall");
    
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "to visible"));
    TEST_ASSERT_NULL(strstr(editor_input, "to hidden"));
}

void test_edall_excludes_buried_variables(void)
{
    run_string("make \"visible 1");
    run_string("make \"hidden 2");
    run_string("buryname \"hidden");
    
    mock_device_clear_editor();
    
    run_string("edall");
    
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "make \"visible"));
    TEST_ASSERT_NULL(strstr(editor_input, "make \"hidden"));
}

void test_edall_formats_property_lists(void)
{
    run_string("pprop \"myobj \"color \"red");
    
    mock_device_clear_editor();
    
    run_string("edall");
    
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "pprop \"myobj \"color \"red"));
}

void test_edall_formats_numeric_property_values(void)
{
    // Numeric property values should be output without quotes
    run_string("pprop \"item \"count 42");
    run_string("pprop \"item \"price 3.14");
    
    mock_device_clear_editor();
    
    run_string("edall");
    
    const char *editor_input = mock_device_get_editor_input();
    // Numbers should NOT have quotes
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "pprop \"item \"count 42"));
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "pprop \"item \"price 3.14"));
    // Make sure they don't have quotes around the value
    TEST_ASSERT_NULL(strstr(editor_input, "\"count \"42"));
    TEST_ASSERT_NULL(strstr(editor_input, "\"price \"3.14"));
}

void test_edall_empty_workspace(void)
{
    mock_device_clear_editor();
    
    run_string("edall");
    
    TEST_ASSERT_TRUE(mock_device_was_editor_called());
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_EQUAL_STRING("", editor_input);
}

//==========================================================================
// editfile Tests
//==========================================================================

void test_editfile_creates_new_file(void)
{
    setUp_with_storage();
    
    mock_device_clear_editor();
    mock_device_set_editor_result(LOGO_EDITOR_ACCEPT);
    mock_device_set_editor_content("Hello world!\n");
    
    // File doesn't exist yet
    TEST_ASSERT_NULL(mock_fs_get_file("newfile.txt", false));
    
    Result r = run_string("editfile \"newfile.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // File should now exist with content
    const char *content = mock_fs_get_content("newfile.txt");
    TEST_ASSERT_NOT_NULL(content);
    TEST_ASSERT_EQUAL_STRING("Hello world!\n", content);
}

void test_editfile_loads_existing_file(void)
{
    setUp_with_storage();
    
    // Create existing file
    mock_fs_create_file("existing.txt", "Original content\n");
    
    mock_device_clear_editor();
    mock_device_set_editor_result(LOGO_EDITOR_CANCEL);  // Cancel to just check loading
    
    Result r = run_string("editfile \"existing.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(mock_device_was_editor_called());
    
    // Editor should have received file content
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "Original content"));
}

void test_editfile_modifies_existing_file(void)
{
    setUp_with_storage();
    
    // Create existing file
    mock_fs_create_file("modify.txt", "Old content\n");
    
    mock_device_clear_editor();
    mock_device_set_editor_result(LOGO_EDITOR_ACCEPT);
    mock_device_set_editor_content("New content\n");
    
    Result r = run_string("editfile \"modify.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // File should have new content
    const char *content = mock_fs_get_content("modify.txt");
    TEST_ASSERT_NOT_NULL(content);
    TEST_ASSERT_EQUAL_STRING("New content\n", content);
}

void test_editfile_cancel_preserves_file(void)
{
    setUp_with_storage();
    
    // Create existing file
    mock_fs_create_file("preserve.txt", "Original content\n");
    
    mock_device_clear_editor();
    mock_device_set_editor_result(LOGO_EDITOR_CANCEL);
    mock_device_set_editor_content("This should not be saved\n");
    
    Result r = run_string("editfile \"preserve.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // File should have original content (unchanged)
    const char *content = mock_fs_get_content("preserve.txt");
    TEST_ASSERT_NOT_NULL(content);
    TEST_ASSERT_EQUAL_STRING("Original content\n", content);
}

void test_editfile_cancel_does_not_create_file(void)
{
    setUp_with_storage();
    
    mock_device_clear_editor();
    mock_device_set_editor_result(LOGO_EDITOR_CANCEL);
    
    // File doesn't exist
    TEST_ASSERT_NULL(mock_fs_get_file("nocreate.txt", false));
    
    Result r = run_string("editfile \"nocreate.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // File should still not exist
    TEST_ASSERT_NULL(mock_fs_get_file("nocreate.txt", false));
}

void test_editfile_requires_word_argument(void)
{
    setUp_with_storage();
    
    // List argument should fail
    Result r = run_string("editfile [test.txt]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_editfile_multiline_content(void)
{
    setUp_with_storage();
    
    // Create file with multiple lines
    mock_fs_create_file("multi.txt", "Line 1\nLine 2\nLine 3\n");
    
    mock_device_clear_editor();
    mock_device_set_editor_result(LOGO_EDITOR_CANCEL);
    
    Result r = run_string("editfile \"multi.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Editor should have received all lines
    const char *editor_input = mock_device_get_editor_input();
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "Line 1"));
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "Line 2"));
    TEST_ASSERT_NOT_NULL(strstr(editor_input, "Line 3"));
}

void test_editfile_does_not_run_content(void)
{
    setUp_with_storage();
    
    // Create file with Logo code - should NOT be executed
    mock_fs_create_file("norun.txt", "make \"testvar 999\n");
    
    mock_device_clear_editor();
    mock_device_set_editor_result(LOGO_EDITOR_ACCEPT);
    mock_device_set_editor_content("make \"testvar 999\n");
    
    // Ensure variable doesn't exist
    Value dummy;
    TEST_ASSERT_FALSE(var_get("testvar", &dummy));
    
    Result r = run_string("editfile \"norun.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Variable should NOT exist - editfile doesn't run code
    TEST_ASSERT_FALSE(var_get("testvar", &dummy));
}

void test_editfile_empty_file(void)
{
    setUp_with_storage();
    
    // Create empty file
    mock_fs_create_file("empty.txt", "");
    
    mock_device_clear_editor();
    mock_device_set_editor_result(LOGO_EDITOR_ACCEPT);
    mock_device_set_editor_content("Now has content\n");
    
    Result r = run_string("editfile \"empty.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // File should have new content
    const char *content = mock_fs_get_content("empty.txt");
    TEST_ASSERT_NOT_NULL(content);
    TEST_ASSERT_EQUAL_STRING("Now has content\n", content);
}

void test_editfile_preserves_non_logo_content(void)
{
    setUp_with_storage();
    
    // Create file with arbitrary text (not Logo code)
    const char *original = "This is just plain text.\nNot Logo code at all!\n# Some comment\n";
    mock_fs_create_file("plain.txt", original);
    
    mock_device_clear_editor();
    mock_device_set_editor_result(LOGO_EDITOR_CANCEL);
    
    Result r = run_string("editfile \"plain.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // File should still have original content
    const char *content = mock_fs_get_content("plain.txt");
    TEST_ASSERT_NOT_NULL(content);
    TEST_ASSERT_EQUAL_STRING(original, content);
}

void test_editfile_already_open_error(void)
{
    setUp_with_storage();
    
    // Create and open a file
    mock_fs_create_file("alreadyopen.txt", "content");
    run_string("open \"alreadyopen.txt");
    
    mock_device_clear_editor();
    
    // Try to editfile - should fail because file is open
    Result r = run_string("editfile \"alreadyopen.txt");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_FILE_ALREADY_OPEN, r.error_code);
    
    // Editor should not have been called
    TEST_ASSERT_FALSE(mock_device_was_editor_called());
}

//==========================================================================
// Main
//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    
    RUN_TEST(test_edit_requires_editor);
    RUN_TEST(test_edit_formats_procedure_definition);
    RUN_TEST(test_edit_with_parameters);
    RUN_TEST(test_edit_list_of_procedures);
    RUN_TEST(test_edit_undefined_procedure_opens_with_template);
    RUN_TEST(test_edit_list_with_undefined_procedure_opens_with_template);
    RUN_TEST(test_edit_undefined_procedure_accept_creates_procedure);
    RUN_TEST(test_edit_cancel_does_nothing);
    RUN_TEST(test_edit_accept_redefines_procedure);
    RUN_TEST(test_ed_abbreviation);
    RUN_TEST(test_edn_formats_variable);
    RUN_TEST(test_edn_formats_word_variable);
    RUN_TEST(test_edn_formats_list_variable);
    RUN_TEST(test_edn_list_of_variables);
    RUN_TEST(test_edn_unknown_variable_error);
    RUN_TEST(test_edns_formats_all_variables);
    RUN_TEST(test_edall_formats_all_procedures);
    RUN_TEST(test_edall_formats_all_variables);
    RUN_TEST(test_edall_formats_procedures_and_variables);
    RUN_TEST(test_edall_excludes_buried_procedures);
    RUN_TEST(test_edall_excludes_buried_variables);
    RUN_TEST(test_edall_formats_property_lists);
    RUN_TEST(test_edall_formats_numeric_property_values);
    RUN_TEST(test_edall_empty_workspace);
    RUN_TEST(test_edit_no_args_preserves_buffer);
    RUN_TEST(test_edit_runs_regular_commands);
    RUN_TEST(test_edit_runs_multiple_commands);
    RUN_TEST(test_edit_runs_mixed_content);
    
    // editfile tests
    RUN_TEST(test_editfile_creates_new_file);
    RUN_TEST(test_editfile_loads_existing_file);
    RUN_TEST(test_editfile_modifies_existing_file);
    RUN_TEST(test_editfile_cancel_preserves_file);
    RUN_TEST(test_editfile_cancel_does_not_create_file);
    RUN_TEST(test_editfile_requires_word_argument);
    RUN_TEST(test_editfile_multiline_content);
    RUN_TEST(test_editfile_does_not_run_content);
    RUN_TEST(test_editfile_empty_file);
    RUN_TEST(test_editfile_preserves_non_logo_content);
    RUN_TEST(test_editfile_already_open_error);
    
    return UNITY_END();
}
