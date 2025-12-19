//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for the I/O state manager (devices/io.c).
//

#include "unity.h"
#include "devices/io.h"
#include "devices/storage.h"
#include "devices/hardware.h"
#include "mock_device.h"
#include <string.h>
#include <stdlib.h>

//
// Mock Storage Implementation
//

static bool mock_file_exists_result = false;
static bool mock_dir_exists_result = false;
static bool mock_file_delete_result = false;
static bool mock_dir_create_result = false;
static bool mock_dir_delete_result = false;
static bool mock_rename_result = false;
static long mock_file_size_result = -1;
static LogoStream *mock_open_result = NULL;

static char last_opened_path[256];
static char last_deleted_file[256];
static char last_created_dir[256];
static char last_deleted_dir[256];
static char last_renamed_old[256];
static char last_renamed_new[256];

static void reset_mock_storage(void)
{
    mock_file_exists_result = false;
    mock_dir_exists_result = false;
    mock_file_delete_result = false;
    mock_dir_create_result = false;
    mock_dir_delete_result = false;
    mock_rename_result = false;
    mock_file_size_result = -1;
    mock_open_result = NULL;

    last_opened_path[0] = '\0';
    last_deleted_file[0] = '\0';
    last_created_dir[0] = '\0';
    last_deleted_dir[0] = '\0';
    last_renamed_old[0] = '\0';
    last_renamed_new[0] = '\0';
}

static bool mock_file_exists(const char *path)
{
    return mock_file_exists_result;
}

static bool mock_dir_exists(const char *path)
{
    return mock_dir_exists_result;
}

static bool mock_file_delete(const char *path)
{
    strncpy(last_deleted_file, path, sizeof(last_deleted_file) - 1);
    return mock_file_delete_result;
}

static bool mock_dir_create(const char *path)
{
    strncpy(last_created_dir, path, sizeof(last_created_dir) - 1);
    return mock_dir_create_result;
}

static bool mock_dir_delete(const char *path)
{
    strncpy(last_deleted_dir, path, sizeof(last_deleted_dir) - 1);
    return mock_dir_delete_result;
}

static bool mock_rename(const char *old_path, const char *new_path)
{
    strncpy(last_renamed_old, old_path, sizeof(last_renamed_old) - 1);
    strncpy(last_renamed_new, new_path, sizeof(last_renamed_new) - 1);
    return mock_rename_result;
}

static long mock_file_size(const char *path)
{
    return mock_file_size_result;
}

static bool mock_list_directory(const char *path, LogoDirCallback callback, void *user_data, const char *filter)
{
    return true;
}

static LogoStream *mock_open(const char *path)
{
    strncpy(last_opened_path, path, sizeof(last_opened_path) - 1);
    return mock_open_result;
}

static const LogoStorageOps mock_storage_ops = {
    .file_exists = mock_file_exists,
    .dir_exists = mock_dir_exists,
    .file_delete = mock_file_delete,
    .dir_create = mock_dir_create,
    .dir_delete = mock_dir_delete,
    .rename = mock_rename,
    .file_size = mock_file_size,
    .list_directory = mock_list_directory,
    .open = mock_open
};

static LogoStorage mock_storage = {
    .ops = &mock_storage_ops
};

//
// Mock Hardware Implementation
//

static void mock_sleep(int ms) {}
static uint32_t mock_random(void) { return 42; }
static void mock_get_battery_level(int *level, bool *charging) { *level = 100; *charging = false; }
static bool mock_check_user_interrupt(void) { return false; }
static void mock_clear_user_interrupt(void) {}

static LogoHardwareOps mock_hardware_ops = {
    .sleep = mock_sleep,
    .random = mock_random,
    .get_battery_level = mock_get_battery_level,
    .check_user_interrupt = mock_check_user_interrupt,
    .clear_user_interrupt = mock_clear_user_interrupt
};

static LogoHardware mock_hardware = {
    .ops = &mock_hardware_ops
};

//
// Test Setup
//

static LogoIO io;

void setUp(void)
{
    mock_device_init();
    reset_mock_storage();
    logo_io_init(&io, mock_device_get_console(), &mock_storage, &mock_hardware);
}

void tearDown(void)
{
    logo_io_cleanup(&io);
}

//
// Tests
//

void test_init_defaults(void)
{
    TEST_ASSERT_NOT_NULL(io.console);
    TEST_ASSERT_NOT_NULL(io.storage);
    TEST_ASSERT_NOT_NULL(io.hardware);
    TEST_ASSERT_EQUAL_PTR(&io.console->input, io.reader);
    TEST_ASSERT_EQUAL_PTR(&io.console->output, io.writer);
    TEST_ASSERT_NULL(io.dribble);
    TEST_ASSERT_EQUAL(0, io.open_count);
    TEST_ASSERT_EQUAL_STRING("", io.prefix);
}

void test_prefix_management(void)
{
    logo_io_set_prefix(&io, "test_prefix");
    TEST_ASSERT_EQUAL_STRING("test_prefix", logo_io_get_prefix(&io));

    char buffer[256];
    
    // Test resolve with prefix
    TEST_ASSERT_NOT_NULL(logo_io_resolve_path(&io, "file.txt", buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("test_prefix/file.txt", buffer);

    // Test resolve absolute path (should ignore prefix)
    TEST_ASSERT_NOT_NULL(logo_io_resolve_path(&io, "/abs/file.txt", buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("/abs/file.txt", buffer);

    // Clear prefix
    logo_io_set_prefix(&io, "");
    TEST_ASSERT_EQUAL_STRING("", logo_io_get_prefix(&io));
    
    // Test resolve without prefix
    TEST_ASSERT_NOT_NULL(logo_io_resolve_path(&io, "file.txt", buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("file.txt", buffer);
}

void test_file_exists(void)
{
    mock_file_exists_result = true;
    TEST_ASSERT_TRUE(logo_io_file_exists(&io, "test.txt"));
    
    mock_file_exists_result = false;
    TEST_ASSERT_FALSE(logo_io_file_exists(&io, "test.txt"));
}

void test_open_file(void)
{
    // Setup a mock stream to return
    LogoStream *stream = malloc(sizeof(LogoStream));
    memset(stream, 0, sizeof(LogoStream));
    strncpy(stream->name, "test.txt", sizeof(stream->name) - 1);
    
    // Mock stream ops
    static LogoStreamOps stream_ops = {0};
    stream->ops = &stream_ops;

    mock_open_result = stream;

    LogoStream *opened = logo_io_open(&io, "test.txt");
    TEST_ASSERT_NOT_NULL(opened);
    TEST_ASSERT_EQUAL_PTR(stream, opened);
    TEST_ASSERT_EQUAL(1, io.open_count);
    TEST_ASSERT_TRUE(logo_io_is_open(&io, "test.txt"));
    TEST_ASSERT_EQUAL_STRING("test.txt", last_opened_path);

    // Test finding open file
    TEST_ASSERT_EQUAL_PTR(stream, logo_io_find_open(&io, "test.txt"));

    // Test closing file
    logo_io_close(&io, "test.txt");
    TEST_ASSERT_EQUAL(0, io.open_count);
    TEST_ASSERT_FALSE(logo_io_is_open(&io, "test.txt"));
    
    // Note: logo_io_close frees the stream, so we don't need to free it here if close was called
}

void test_reader_writer_control(void)
{
    // Initially console
    TEST_ASSERT_TRUE(logo_io_reader_is_keyboard(&io));
    TEST_ASSERT_TRUE(logo_io_writer_is_screen(&io));
    TEST_ASSERT_EQUAL_STRING("", logo_io_get_reader_name(&io));
    TEST_ASSERT_EQUAL_STRING("", logo_io_get_writer_name(&io));

    // Create a dummy stream
    LogoStream stream;
    memset(&stream, 0, sizeof(stream));
    strncpy(stream.name, "test_stream", sizeof(stream.name) - 1);

    // Set reader
    logo_io_set_reader(&io, &stream);
    TEST_ASSERT_FALSE(logo_io_reader_is_keyboard(&io));
    TEST_ASSERT_EQUAL_PTR(&stream, io.reader);
    TEST_ASSERT_EQUAL_STRING("test_stream", logo_io_get_reader_name(&io));

    // Set writer
    logo_io_set_writer(&io, &stream);
    TEST_ASSERT_FALSE(logo_io_writer_is_screen(&io));
    TEST_ASSERT_EQUAL_PTR(&stream, io.writer);
    TEST_ASSERT_EQUAL_STRING("test_stream", logo_io_get_writer_name(&io));

    // Reset to console
    logo_io_set_reader(&io, NULL);
    logo_io_set_writer(&io, NULL);
    TEST_ASSERT_TRUE(logo_io_reader_is_keyboard(&io));
    TEST_ASSERT_TRUE(logo_io_writer_is_screen(&io));
}

// Mock stream ops for dribble
static int mock_read_char_result = 'A';
static int mock_read_char(LogoStream *stream) { return mock_read_char_result; }

static char last_written_text[256];
static void mock_write(LogoStream *stream, const char *text) {
    strncpy(last_written_text, text, sizeof(last_written_text) - 1);
}

static void mock_stream_close(LogoStream *stream) {}
static void mock_stream_write(LogoStream *stream, const char *text) {
    mock_write(stream, text);
}
static bool mock_stream_set_write_pos(LogoStream *stream, long pos) { return true; }
static long mock_stream_get_length(LogoStream *stream) { return 0; }

static LogoStreamOps dribble_ops = {
    .close = mock_stream_close,
    .write = mock_stream_write,
    .set_write_pos = mock_stream_set_write_pos,
    .get_length = mock_stream_get_length
};

static LogoStreamOps rw_ops = {
    .read_char = mock_read_char,
    .write = mock_write
};

void test_dribble(void)
{
    // Setup mock stream for dribble
    LogoStream *stream = malloc(sizeof(LogoStream));
    memset(stream, 0, sizeof(LogoStream));
    stream->ops = &dribble_ops;
    mock_open_result = stream;

    TEST_ASSERT_FALSE(logo_io_is_dribbling(&io));

    // Start dribble
    bool result = logo_io_start_dribble(&io, "dribble.txt");
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(logo_io_is_dribbling(&io));
    TEST_ASSERT_NOT_NULL(io.dribble);

    // Stop dribble
    logo_io_stop_dribble(&io);
    TEST_ASSERT_FALSE(logo_io_is_dribbling(&io));
    TEST_ASSERT_NULL(io.dribble);
}

void test_file_operations(void)
{
    // Delete file
    mock_file_delete_result = true;
    TEST_ASSERT_TRUE(logo_io_file_delete(&io, "del.txt"));
    TEST_ASSERT_EQUAL_STRING("del.txt", last_deleted_file);

    // Create dir
    mock_dir_create_result = true;
    TEST_ASSERT_TRUE(logo_io_dir_create(&io, "newdir"));
    TEST_ASSERT_EQUAL_STRING("newdir", last_created_dir);

    // Delete dir
    mock_dir_delete_result = true;
    TEST_ASSERT_TRUE(logo_io_dir_delete(&io, "olddir"));
    TEST_ASSERT_EQUAL_STRING("olddir", last_deleted_dir);

    // Rename
    mock_rename_result = true;
    TEST_ASSERT_TRUE(logo_io_rename(&io, "old", "new"));
    TEST_ASSERT_EQUAL_STRING("old", last_renamed_old);
    TEST_ASSERT_EQUAL_STRING("new", last_renamed_new);

    // File size
    mock_file_size_result = 1234;
    TEST_ASSERT_EQUAL(1234, logo_io_file_size(&io, "size.txt"));
}

void test_read_write_operations(void)
{
    LogoStream stream;
    memset(&stream, 0, sizeof(stream));
    stream.ops = &rw_ops;
    stream.is_open = true;
    strncpy(stream.name, "rw_stream", sizeof(stream.name) - 1);

    // Test read
    logo_io_set_reader(&io, &stream);
    mock_read_char_result = 'X';
    TEST_ASSERT_EQUAL('X', logo_io_read_char(&io));

    // Test write
    logo_io_set_writer(&io, &stream);
    last_written_text[0] = '\0';
    logo_io_write(&io, "Hello");
    TEST_ASSERT_EQUAL_STRING("Hello", last_written_text);
}

void test_hardware_wrappers(void)
{
    // Just verify they don't crash and call the mock
    logo_io_sleep(&io, 10);
    TEST_ASSERT_EQUAL(42, logo_io_random(&io));
    
    int level;
    bool charging;
    logo_io_get_battery_level(&io, &level, &charging);
    TEST_ASSERT_EQUAL(100, level);
    TEST_ASSERT_FALSE(charging);
    
    TEST_ASSERT_FALSE(logo_io_check_user_interrupt(&io));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_defaults);
    RUN_TEST(test_prefix_management);
    RUN_TEST(test_file_exists);
    RUN_TEST(test_open_file);
    RUN_TEST(test_reader_writer_control);
    RUN_TEST(test_dribble);
    RUN_TEST(test_file_operations);
    RUN_TEST(test_read_write_operations);
    RUN_TEST(test_hardware_wrappers);
    return UNITY_END();
}
