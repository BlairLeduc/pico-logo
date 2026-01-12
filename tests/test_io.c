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
    .clear_user_interrupt = mock_clear_user_interrupt,
    .toot = NULL,  // Mock: no audio
    .network_tcp_connect = NULL,
    .network_tcp_close = NULL,
    .network_tcp_read = NULL,
    .network_tcp_write = NULL,
    .network_tcp_can_read = NULL,
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

void test_resolve_path_with_parent_dir(void)
{
    char buffer[256];
    
    // Test ".." with prefix
    logo_io_set_prefix(&io, "/Logo/apple/");
    TEST_ASSERT_NOT_NULL(logo_io_resolve_path(&io, "..", buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("/Logo", buffer);
    
    // Test "../banana" with prefix
    logo_io_set_prefix(&io, "/Logo/apple/");
    TEST_ASSERT_NOT_NULL(logo_io_resolve_path(&io, "../banana", buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("/Logo/banana", buffer);
    
    // Test multiple ".." segments
    logo_io_set_prefix(&io, "/Logo/apple/banana/");
    TEST_ASSERT_NOT_NULL(logo_io_resolve_path(&io, "../../..", buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("/", buffer);
    
    // Test ".." at root (should stay at root)
    logo_io_set_prefix(&io, "/Logo/");
    TEST_ASSERT_NOT_NULL(logo_io_resolve_path(&io, "..", buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("/", buffer);
    
    // Test ".." going past root (should clamp to root)
    logo_io_set_prefix(&io, "/");
    TEST_ASSERT_NOT_NULL(logo_io_resolve_path(&io, "..", buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("/", buffer);
    
    // Test "." (current directory) should be normalized out
    logo_io_set_prefix(&io, "/Logo/");
    TEST_ASSERT_NOT_NULL(logo_io_resolve_path(&io, "./apple", buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("/Logo/apple", buffer);
    
    // Test combined "." and ".."
    logo_io_set_prefix(&io, "/Logo/apple/");
    TEST_ASSERT_NOT_NULL(logo_io_resolve_path(&io, "./../banana/./cherry", buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("/Logo/banana/cherry", buffer);
    
    // Clear prefix
    logo_io_set_prefix(&io, "");
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
    stream->is_open = true;  // Mark stream as open
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

void test_dribble_input(void)
{
    // Setup mock stream for dribble
    LogoStream *stream = malloc(sizeof(LogoStream));
    memset(stream, 0, sizeof(LogoStream));
    stream->ops = &dribble_ops;
    stream->is_open = true;  // Mark stream as open so writes work
    mock_open_result = stream;

    // Start dribble
    bool result = logo_io_start_dribble(&io, "dribble.txt");
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(logo_io_is_dribbling(&io));

    // Clear the last written text
    last_written_text[0] = '\0';

    // Dribble input should write to dribble file (text + newline)
    logo_io_dribble_input(&io, "repeat 5 [pr random 10]");
    
    // The last write is the newline (function writes text, then newline separately)
    TEST_ASSERT_EQUAL_STRING("\n", last_written_text);

    // Stop dribble
    logo_io_stop_dribble(&io);

    // Dribble input should do nothing when not dribbling
    last_written_text[0] = '\0';
    logo_io_dribble_input(&io, "should not write");
    TEST_ASSERT_EQUAL_STRING("", last_written_text);
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

//
// Network address parsing tests
//

void test_parse_network_address_valid(void)
{
    char host[256];
    uint16_t port;
    
    // Simple hostname:port
    TEST_ASSERT_TRUE(logo_io_parse_network_address("localhost:8080", host, sizeof(host), &port));
    TEST_ASSERT_EQUAL_STRING("localhost", host);
    TEST_ASSERT_EQUAL_UINT16(8080, port);
    
    // Domain name:port
    TEST_ASSERT_TRUE(logo_io_parse_network_address("example.com:80", host, sizeof(host), &port));
    TEST_ASSERT_EQUAL_STRING("example.com", host);
    TEST_ASSERT_EQUAL_UINT16(80, port);
    
    // IPv4 address:port
    TEST_ASSERT_TRUE(logo_io_parse_network_address("192.168.1.100:8080", host, sizeof(host), &port));
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", host);
    TEST_ASSERT_EQUAL_UINT16(8080, port);
    
    // Port boundaries
    TEST_ASSERT_TRUE(logo_io_parse_network_address("host:1", host, sizeof(host), &port));
    TEST_ASSERT_EQUAL_UINT16(1, port);
    
    TEST_ASSERT_TRUE(logo_io_parse_network_address("host:65535", host, sizeof(host), &port));
    TEST_ASSERT_EQUAL_UINT16(65535, port);
}

void test_parse_network_address_invalid(void)
{
    char host[256];
    uint16_t port;
    
    // No colon
    TEST_ASSERT_FALSE(logo_io_parse_network_address("localhost", host, sizeof(host), &port));
    
    // No port number
    TEST_ASSERT_FALSE(logo_io_parse_network_address("localhost:", host, sizeof(host), &port));
    
    // No host
    TEST_ASSERT_FALSE(logo_io_parse_network_address(":8080", host, sizeof(host), &port));
    
    // Port out of range (too large)
    TEST_ASSERT_FALSE(logo_io_parse_network_address("host:65536", host, sizeof(host), &port));
    
    // Port out of range (0)
    TEST_ASSERT_FALSE(logo_io_parse_network_address("host:0", host, sizeof(host), &port));
    
    // Port with non-numeric characters
    TEST_ASSERT_FALSE(logo_io_parse_network_address("host:abc", host, sizeof(host), &port));
    
    // Port with mixed characters
    TEST_ASSERT_FALSE(logo_io_parse_network_address("host:123abc", host, sizeof(host), &port));
}

void test_is_network_address(void)
{
    // Valid network addresses
    TEST_ASSERT_TRUE(logo_io_is_network_address("localhost:8080"));
    TEST_ASSERT_TRUE(logo_io_is_network_address("example.com:80"));
    TEST_ASSERT_TRUE(logo_io_is_network_address("192.168.1.100:8080"));
    TEST_ASSERT_TRUE(logo_io_is_network_address("host:1"));
    TEST_ASSERT_TRUE(logo_io_is_network_address("host:65535"));
    
    // Invalid (not network addresses)
    TEST_ASSERT_FALSE(logo_io_is_network_address("startup"));
    TEST_ASSERT_FALSE(logo_io_is_network_address("/path/to/file"));
    TEST_ASSERT_FALSE(logo_io_is_network_address("file.txt"));
    TEST_ASSERT_FALSE(logo_io_is_network_address("localhost:"));
    TEST_ASSERT_FALSE(logo_io_is_network_address(":8080"));
    TEST_ASSERT_FALSE(logo_io_is_network_address("host:0"));
    TEST_ASSERT_FALSE(logo_io_is_network_address("host:65536"));
}

void test_network_timeout(void)
{
    // Default timeout
    TEST_ASSERT_EQUAL(LOGO_DEFAULT_NETWORK_TIMEOUT, logo_io_get_timeout(&io));
    
    // Set new timeout
    logo_io_set_timeout(&io, 200);
    TEST_ASSERT_EQUAL(200, logo_io_get_timeout(&io));
    
    // Set timeout to 0 (no timeout)
    logo_io_set_timeout(&io, 0);
    TEST_ASSERT_EQUAL(0, logo_io_get_timeout(&io));
    
    // Restore default for other tests
    logo_io_set_timeout(&io, LOGO_DEFAULT_NETWORK_TIMEOUT);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_defaults);
    RUN_TEST(test_prefix_management);
    RUN_TEST(test_resolve_path_with_parent_dir);
    RUN_TEST(test_file_exists);
    RUN_TEST(test_open_file);
    RUN_TEST(test_reader_writer_control);
    RUN_TEST(test_dribble);
    RUN_TEST(test_dribble_input);
    RUN_TEST(test_file_operations);
    RUN_TEST(test_read_write_operations);
    RUN_TEST(test_hardware_wrappers);
    RUN_TEST(test_parse_network_address_valid);
    RUN_TEST(test_parse_network_address_invalid);
    RUN_TEST(test_is_network_address);
    RUN_TEST(test_network_timeout);
    return UNITY_END();
}
