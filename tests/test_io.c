//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
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
static bool mock_interrupt_flag = false;
static bool mock_check_user_interrupt(void) { return mock_interrupt_flag; }
static void mock_clear_user_interrupt(void) { mock_interrupt_flag = false; }

static bool mock_pause_flag = false;
static bool mock_check_pause(void) { return mock_pause_flag; }
static void mock_clear_pause(void) { mock_pause_flag = false; }

static bool mock_freeze_flag = false;
static bool mock_check_freeze(void) { return mock_freeze_flag; }
static void mock_clear_freeze(void) { mock_freeze_flag = false; }

static LogoHardwareOps mock_hardware_ops = {
    .sleep = mock_sleep,
    .random = mock_random,
    .get_battery_level = mock_get_battery_level,
    .check_user_interrupt = mock_check_user_interrupt,
    .clear_user_interrupt = mock_clear_user_interrupt,
    .check_pause_request = mock_check_pause,
    .clear_pause_request = mock_clear_pause,
    .check_freeze_request = mock_check_freeze,
    .clear_freeze_request = mock_clear_freeze,
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
    mock_interrupt_flag = false;
    mock_pause_flag = false;
    mock_freeze_flag = false;
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

//
// Pause/Freeze request tests
//

void test_check_pause_request(void)
{
    // No pause requested
    TEST_ASSERT_FALSE(logo_io_check_pause_request(&io));
    
    // Set pause flag
    mock_pause_flag = true;
    TEST_ASSERT_TRUE(logo_io_check_pause_request(&io));
    // Flag is NOT cleared by check (caller decides)
    TEST_ASSERT_TRUE(mock_pause_flag);
}

void test_clear_pause_request(void)
{
    mock_pause_flag = true;
    logo_io_clear_pause_request(&io);
    TEST_ASSERT_FALSE(mock_pause_flag);
}

void test_check_freeze_request(void)
{
    // No freeze requested
    TEST_ASSERT_FALSE(logo_io_check_freeze_request(&io));
    
    // Set freeze flag
    mock_freeze_flag = true;
    TEST_ASSERT_TRUE(logo_io_check_freeze_request(&io));
    // Freeze flag IS cleared by check
    TEST_ASSERT_FALSE(mock_freeze_flag);
}

void test_clear_freeze_request(void)
{
    mock_freeze_flag = true;
    logo_io_clear_freeze_request(&io);
    TEST_ASSERT_FALSE(mock_freeze_flag);
}

//
// Close all / get open tests
//

void test_close_all(void)
{
    // Open two mock files
    LogoStream *s1 = malloc(sizeof(LogoStream));
    memset(s1, 0, sizeof(LogoStream));
    strncpy(s1->name, "file1.txt", sizeof(s1->name) - 1);
    static LogoStreamOps close_all_ops = {0};
    s1->ops = &close_all_ops;
    mock_open_result = s1;
    logo_io_open(&io, "file1.txt");
    
    LogoStream *s2 = malloc(sizeof(LogoStream));
    memset(s2, 0, sizeof(LogoStream));
    strncpy(s2->name, "file2.txt", sizeof(s2->name) - 1);
    s2->ops = &close_all_ops;
    mock_open_result = s2;
    logo_io_open(&io, "file2.txt");
    
    TEST_ASSERT_EQUAL(2, logo_io_open_count(&io));
    
    logo_io_close_all(&io);
    TEST_ASSERT_EQUAL(0, logo_io_open_count(&io));
    
    // Reader/writer should be reset to console
    TEST_ASSERT_TRUE(logo_io_reader_is_keyboard(&io));
    TEST_ASSERT_TRUE(logo_io_writer_is_screen(&io));
}

void test_get_open_by_index(void)
{
    // Open a mock file
    LogoStream *s1 = malloc(sizeof(LogoStream));
    memset(s1, 0, sizeof(LogoStream));
    strncpy(s1->name, "myfile.txt", sizeof(s1->name) - 1);
    static LogoStreamOps idx_ops = {0};
    s1->ops = &idx_ops;
    mock_open_result = s1;
    logo_io_open(&io, "myfile.txt");
    
    // Index 0 should return the file
    LogoStream *got = logo_io_get_open(&io, 0);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL_STRING("myfile.txt", got->name);
    
    // Index 1 should return NULL
    TEST_ASSERT_NULL(logo_io_get_open(&io, 1));
    
    // Negative index should return NULL
    TEST_ASSERT_NULL(logo_io_get_open(&io, -1));
}

//
// is_network_stream test
//

void test_is_network_stream(void)
{
    LogoStream stream;
    memset(&stream, 0, sizeof(stream));
    
    stream.type = LOGO_STREAM_NETWORK;
    TEST_ASSERT_TRUE(logo_io_is_network_stream(&stream));
    
    stream.type = LOGO_STREAM_FILE;
    TEST_ASSERT_FALSE(logo_io_is_network_stream(&stream));
    
    TEST_ASSERT_FALSE(logo_io_is_network_stream(NULL));
}

//
// dir_exists test
//

void test_dir_exists(void)
{
    mock_dir_exists_result = true;
    TEST_ASSERT_TRUE(logo_io_dir_exists(&io, "mydir"));
    
    mock_dir_exists_result = false;
    TEST_ASSERT_FALSE(logo_io_dir_exists(&io, "mydir"));
}

//
// High-level I/O operation tests
//

static char rw_buffer[1024];
static int rw_buffer_pos = 0;

static void rw_write_capture(LogoStream *stream, const char *text)
{
    size_t len = strlen(text);
    if (rw_buffer_pos + (int)len < (int)sizeof(rw_buffer))
    {
        memcpy(rw_buffer + rw_buffer_pos, text, len);
        rw_buffer_pos += (int)len;
        rw_buffer[rw_buffer_pos] = '\0';
    }
}

static bool rw_flushed = false;
static void rw_flush(LogoStream *stream) { rw_flushed = true; }

static const char *rw_input_str = NULL;
static int rw_input_pos = 0;

static int rw_read_char(LogoStream *stream)
{
    if (!rw_input_str || rw_input_str[rw_input_pos] == '\0')
        return LOGO_STREAM_EOF;
    return (unsigned char)rw_input_str[rw_input_pos++];
}

static int rw_read_chars(LogoStream *stream, char *buffer, int count)
{
    if (!rw_input_str) return -1;
    int i = 0;
    while (i < count && rw_input_str[rw_input_pos] != '\0')
    {
        buffer[i++] = rw_input_str[rw_input_pos++];
    }
    return i > 0 ? i : -1;
}

static int rw_read_line(LogoStream *stream, char *buffer, size_t size)
{
    if (!rw_input_str) return -1;
    size_t i = 0;
    while (i < size - 1 && rw_input_str[rw_input_pos] != '\0' &&
           rw_input_str[rw_input_pos] != '\n')
    {
        buffer[i++] = rw_input_str[rw_input_pos++];
    }
    if (rw_input_str[rw_input_pos] == '\n') rw_input_pos++;
    buffer[i] = '\0';
    return (i > 0 || rw_input_str[rw_input_pos] != '\0') ? (int)i : -1;
}

static bool rw_can_read_val = false;
static bool rw_can_read(LogoStream *stream) { return rw_can_read_val; }

static LogoStreamOps full_rw_ops = {
    .read_char = rw_read_char,
    .read_chars = rw_read_chars,
    .read_line = rw_read_line,
    .can_read = rw_can_read,
    .write = rw_write_capture,
    .flush = rw_flush,
};

void test_read_chars_operation(void)
{
    LogoStream stream;
    memset(&stream, 0, sizeof(stream));
    stream.ops = &full_rw_ops;
    stream.is_open = true;
    strncpy(stream.name, "input", sizeof(stream.name) - 1);
    
    rw_input_str = "Hello";
    rw_input_pos = 0;
    
    logo_io_set_reader(&io, &stream);
    
    char buf[16];
    int n = logo_io_read_chars(&io, buf, 3);
    TEST_ASSERT_EQUAL(3, n);
    buf[n] = '\0';
    TEST_ASSERT_EQUAL_STRING("Hel", buf);
}

void test_read_line_operation(void)
{
    LogoStream stream;
    memset(&stream, 0, sizeof(stream));
    stream.ops = &full_rw_ops;
    stream.is_open = true;
    strncpy(stream.name, "input", sizeof(stream.name) - 1);
    
    rw_input_str = "Hello World\nSecond Line";
    rw_input_pos = 0;
    
    logo_io_set_reader(&io, &stream);
    
    char buf[64];
    int n = logo_io_read_line(&io, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(11, n);
    TEST_ASSERT_EQUAL_STRING("Hello World", buf);
}

void test_key_available(void)
{
    LogoStream stream;
    memset(&stream, 0, sizeof(stream));
    stream.ops = &full_rw_ops;
    stream.is_open = true;
    strncpy(stream.name, "input", sizeof(stream.name) - 1);
    
    logo_io_set_reader(&io, &stream);
    
    rw_can_read_val = false;
    TEST_ASSERT_FALSE(logo_io_key_available(&io));
    
    rw_can_read_val = true;
    TEST_ASSERT_TRUE(logo_io_key_available(&io));
}

void test_write_line_operation(void)
{
    LogoStream stream;
    memset(&stream, 0, sizeof(stream));
    stream.ops = &full_rw_ops;
    stream.is_open = true;
    strncpy(stream.name, "output", sizeof(stream.name) - 1);
    
    rw_buffer_pos = 0;
    rw_buffer[0] = '\0';
    
    logo_io_set_writer(&io, &stream);
    logo_io_write_line(&io, "Hello");
    
    TEST_ASSERT_EQUAL_STRING("Hello\n", rw_buffer);
}

void test_write_line_null_text(void)
{
    LogoStream stream;
    memset(&stream, 0, sizeof(stream));
    stream.ops = &full_rw_ops;
    stream.is_open = true;
    strncpy(stream.name, "output", sizeof(stream.name) - 1);
    
    rw_buffer_pos = 0;
    rw_buffer[0] = '\0';
    
    logo_io_set_writer(&io, &stream);
    logo_io_write_line(&io, NULL);
    
    // Should write just a newline
    TEST_ASSERT_EQUAL_STRING("\n", rw_buffer);
}

void test_flush_operation(void)
{
    LogoStream stream;
    memset(&stream, 0, sizeof(stream));
    stream.ops = &full_rw_ops;
    stream.is_open = true;
    strncpy(stream.name, "output", sizeof(stream.name) - 1);
    
    rw_flushed = false;
    logo_io_set_writer(&io, &stream);
    logo_io_flush(&io);
    
    TEST_ASSERT_TRUE(rw_flushed);
}

void test_check_write_error(void)
{
    LogoStream stream;
    memset(&stream, 0, sizeof(stream));
    stream.ops = &full_rw_ops;
    stream.is_open = true;
    stream.write_error = false;
    strncpy(stream.name, "output", sizeof(stream.name) - 1);
    
    logo_io_set_writer(&io, &stream);
    
    // No error initially
    TEST_ASSERT_FALSE(logo_io_check_write_error(&io));
    
    // Set error
    stream.write_error = true;
    TEST_ASSERT_TRUE(logo_io_check_write_error(&io));
    
    // Error should be cleared after check
    TEST_ASSERT_FALSE(stream.write_error);
    TEST_ASSERT_FALSE(logo_io_check_write_error(&io));
}

//
// Console write tests
//

void test_console_write(void)
{
    // Console write should write to console output even when writer is redirected
    LogoStream stream;
    memset(&stream, 0, sizeof(stream));
    stream.ops = &full_rw_ops;
    stream.is_open = true;
    strncpy(stream.name, "output", sizeof(stream.name) - 1);
    
    rw_buffer_pos = 0;
    rw_buffer[0] = '\0';
    
    // Redirect writer away from console
    logo_io_set_writer(&io, &stream);
    
    // console_write should still write to the console (not the redirected writer)
    // It goes directly to console->output
    logo_io_console_write(&io, "direct");
    // The mock console output is a different stream, so rw_buffer shouldn't have it
    // (it goes to the console output stream instead)
}

void test_console_write_line(void)
{
    // Should not crash with NULL text
    logo_io_console_write_line(&io, NULL);
    logo_io_console_write_line(&io, "hello");
}

//
// Null safety tests
//

void test_null_io_safety(void)
{
    // All functions should handle NULL io gracefully
    logo_io_sleep(NULL, 10);
    TEST_ASSERT_EQUAL(0, logo_io_random(NULL));
    
    int level;
    bool charging;
    logo_io_get_battery_level(NULL, &level, &charging);
    
    // NULL level/charging should not crash
    logo_io_get_battery_level(&io, NULL, &charging);
    logo_io_get_battery_level(&io, &level, NULL);
    
    TEST_ASSERT_FALSE(logo_io_check_user_interrupt(NULL));
    TEST_ASSERT_FALSE(logo_io_check_pause_request(NULL));
    TEST_ASSERT_FALSE(logo_io_check_freeze_request(NULL));
    
    logo_io_clear_pause_request(NULL);
    logo_io_clear_freeze_request(NULL);
    
    logo_io_set_prefix(NULL, "test");
    TEST_ASSERT_EQUAL_STRING("", logo_io_get_prefix(NULL));
    
    logo_io_set_timeout(NULL, 10);
    TEST_ASSERT_EQUAL(0, logo_io_get_timeout(NULL));
    
    TEST_ASSERT_EQUAL(0, logo_io_open_count(NULL));
    TEST_ASSERT_NULL(logo_io_get_open(NULL, 0));
    
    TEST_ASSERT_EQUAL(-1, logo_io_read_char(NULL));
    TEST_ASSERT_EQUAL(-1, logo_io_read_chars(NULL, NULL, 0));
    TEST_ASSERT_EQUAL(-1, logo_io_read_line(NULL, NULL, 0));
    TEST_ASSERT_FALSE(logo_io_key_available(NULL));
    
    // These should not crash
    logo_io_write(NULL, "test");
    logo_io_write_line(NULL, "test");
    logo_io_flush(NULL);
    logo_io_check_write_error(NULL);
    logo_io_console_write(NULL, "test");
    logo_io_console_write_line(NULL, "test");
    logo_io_cleanup(NULL);
    logo_io_init(NULL, NULL, NULL, NULL);
    
    // Null params for parse_network_address
    char host[64];
    uint16_t port;
    TEST_ASSERT_FALSE(logo_io_parse_network_address(NULL, host, sizeof(host), &port));
    TEST_ASSERT_FALSE(logo_io_parse_network_address("host:80", NULL, sizeof(host), &port));
    TEST_ASSERT_FALSE(logo_io_parse_network_address("host:80", host, 0, &port));
    TEST_ASSERT_FALSE(logo_io_parse_network_address("host:80", host, sizeof(host), NULL));
    
    // NULL for open/close/find/is_open
    TEST_ASSERT_NULL(logo_io_open(NULL, "file"));
    TEST_ASSERT_NULL(logo_io_open(&io, NULL));
    logo_io_close(NULL, "file");
    logo_io_close(&io, NULL);
    TEST_ASSERT_NULL(logo_io_find_open(NULL, "file"));
    TEST_ASSERT_NULL(logo_io_find_open(&io, NULL));
    TEST_ASSERT_FALSE(logo_io_is_open(NULL, "file"));
    TEST_ASSERT_FALSE(logo_io_is_open(&io, NULL));
    
    // NULL for file/dir ops
    TEST_ASSERT_FALSE(logo_io_file_exists(NULL, "file"));
    TEST_ASSERT_FALSE(logo_io_dir_exists(NULL, "dir"));
    TEST_ASSERT_FALSE(logo_io_file_delete(NULL, "file"));
    TEST_ASSERT_FALSE(logo_io_dir_create(NULL, "dir"));
    TEST_ASSERT_FALSE(logo_io_dir_delete(NULL, "dir"));
    TEST_ASSERT_FALSE(logo_io_rename(NULL, "old", "new"));
    TEST_ASSERT_EQUAL(-1, logo_io_file_size(NULL, "file"));
    TEST_ASSERT_FALSE(logo_io_list_directory(NULL, "dir", NULL, NULL, NULL));
    
    // NULL for reader/writer name
    TEST_ASSERT_EQUAL_STRING("", logo_io_get_reader_name(NULL));
    TEST_ASSERT_EQUAL_STRING("", logo_io_get_writer_name(NULL));
    TEST_ASSERT_FALSE(logo_io_reader_is_keyboard(NULL));
    TEST_ASSERT_FALSE(logo_io_writer_is_screen(NULL));
    
    // NULL for dribble ops
    TEST_ASSERT_FALSE(logo_io_start_dribble(NULL, "file"));
    TEST_ASSERT_FALSE(logo_io_start_dribble(&io, NULL));
    logo_io_stop_dribble(NULL);
    TEST_ASSERT_FALSE(logo_io_is_dribbling(NULL));
    logo_io_dribble_input(NULL, "text");
    logo_io_dribble_input(&io, NULL);
    
    // resolve_path NULL checks
    char buffer[64];
    TEST_ASSERT_NULL(logo_io_resolve_path(NULL, "file", buffer, sizeof(buffer)));
    TEST_ASSERT_NULL(logo_io_resolve_path(&io, NULL, buffer, sizeof(buffer)));
    TEST_ASSERT_NULL(logo_io_resolve_path(&io, "file", NULL, sizeof(buffer)));
    TEST_ASSERT_NULL(logo_io_resolve_path(&io, "file", buffer, 0));
}

//
// Path normalization edge cases
//

void test_resolve_path_relative_dotdot(void)
{
    char buffer[256];
    
    // Relative path with ".." should keep ".." in result
    logo_io_set_prefix(&io, "");
    TEST_ASSERT_NOT_NULL(logo_io_resolve_path(&io, "../file.txt", buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("../file.txt", buffer);
    
    // Relative path that goes up then down
    TEST_ASSERT_NOT_NULL(logo_io_resolve_path(&io, "a/../b", buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("b", buffer);
    
    // ".." resolving to current directory
    TEST_ASSERT_NOT_NULL(logo_io_resolve_path(&io, "a/..", buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING(".", buffer);
}

void test_resolve_path_buffer_too_small(void)
{
    char buffer[5];  // Very small buffer
    
    // Pathname too long for buffer (absolute, no prefix)
    logo_io_set_prefix(&io, "");
    TEST_ASSERT_NULL(logo_io_resolve_path(&io, "/very/long/path", buffer, sizeof(buffer)));
    
    // Prefix + pathname too long for buffer
    logo_io_set_prefix(&io, "pre");
    TEST_ASSERT_NULL(logo_io_resolve_path(&io, "long", buffer, sizeof(buffer)));
}

void test_resolve_path_prefix_no_trailing_slash(void)
{
    char buffer[256];
    
    // Prefix without trailing "/" should auto-add separator
    logo_io_set_prefix(&io, "mydir");
    TEST_ASSERT_NOT_NULL(logo_io_resolve_path(&io, "file.txt", buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("mydir/file.txt", buffer);
    
    // Prefix with trailing "/" should not double up
    logo_io_set_prefix(&io, "mydir/");
    TEST_ASSERT_NOT_NULL(logo_io_resolve_path(&io, "file.txt", buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("mydir/file.txt", buffer);
}

void test_resolve_path_consecutive_slashes(void)
{
    char buffer[256];

    // Consecutive slashes should be normalized
    logo_io_set_prefix(&io, "");
    TEST_ASSERT_NOT_NULL(logo_io_resolve_path(&io, "/a///b//c", buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("/a/b/c", buffer);
}

//
// Open file edge cases
//

void test_open_already_open(void)
{
    // Open a file
    LogoStream *stream = malloc(sizeof(LogoStream));
    memset(stream, 0, sizeof(LogoStream));
    strncpy(stream->name, "dup.txt", sizeof(stream->name) - 1);
    static LogoStreamOps dup_ops = {0};
    stream->ops = &dup_ops;
    mock_open_result = stream;
    
    LogoStream *first = logo_io_open(&io, "dup.txt");
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_EQUAL(1, io.open_count);
    
    // Opening the same file again should return the existing stream
    LogoStream *second = logo_io_open(&io, "dup.txt");
    TEST_ASSERT_EQUAL_PTR(first, second);
    TEST_ASSERT_EQUAL(1, io.open_count);  // Count should NOT increase
}

void test_open_max_files(void)
{
    static LogoStreamOps max_ops = {0};
    LogoStream *streams[LOGO_MAX_OPEN_FILES];
    
    // Fill all slots
    for (int i = 0; i < LOGO_MAX_OPEN_FILES; i++)
    {
        streams[i] = malloc(sizeof(LogoStream));
        memset(streams[i], 0, sizeof(LogoStream));
        snprintf(streams[i]->name, sizeof(streams[i]->name), "file%d.txt", i);
        streams[i]->ops = &max_ops;
        mock_open_result = streams[i];
        
        char name[32];
        snprintf(name, sizeof(name), "file%d.txt", i);
        TEST_ASSERT_NOT_NULL(logo_io_open(&io, name));
    }
    
    TEST_ASSERT_EQUAL(LOGO_MAX_OPEN_FILES, io.open_count);
    
    // Trying to open one more should fail
    LogoStream extra;
    memset(&extra, 0, sizeof(extra));
    strncpy(extra.name, "extra.txt", sizeof(extra.name) - 1);
    extra.ops = &max_ops;
    mock_open_result = &extra;
    
    TEST_ASSERT_NULL(logo_io_open(&io, "extra.txt"));
}

void test_open_no_storage(void)
{
    // Remove storage to test the guard
    LogoIO no_storage_io;
    logo_io_init(&no_storage_io, mock_device_get_console(), NULL, &mock_hardware);
    
    TEST_ASSERT_NULL(logo_io_open(&no_storage_io, "file.txt"));
    
    logo_io_cleanup(&no_storage_io);
}

//
// Close resets reader/writer
//

void test_close_resets_reader_writer(void)
{
    // Open a file
    LogoStream *stream = malloc(sizeof(LogoStream));
    memset(stream, 0, sizeof(LogoStream));
    strncpy(stream->name, "active.txt", sizeof(stream->name) - 1);
    static LogoStreamOps close_rw_ops = {0};
    stream->ops = &close_rw_ops;
    mock_open_result = stream;
    
    LogoStream *opened = logo_io_open(&io, "active.txt");
    TEST_ASSERT_NOT_NULL(opened);
    
    // Set the opened file as current reader and writer
    logo_io_set_reader(&io, opened);
    logo_io_set_writer(&io, opened);
    TEST_ASSERT_FALSE(logo_io_reader_is_keyboard(&io));
    TEST_ASSERT_FALSE(logo_io_writer_is_screen(&io));
    
    // Close should reset reader/writer to console
    logo_io_close(&io, "active.txt");
    TEST_ASSERT_TRUE(logo_io_reader_is_keyboard(&io));
    TEST_ASSERT_TRUE(logo_io_writer_is_screen(&io));
}

//
// Dribble interacts with write/flush/error
//

void test_write_with_dribble(void)
{
    // Set up writer
    LogoStream writer_stream;
    memset(&writer_stream, 0, sizeof(writer_stream));
    writer_stream.ops = &full_rw_ops;
    writer_stream.is_open = true;
    strncpy(writer_stream.name, "output", sizeof(writer_stream.name) - 1);
    logo_io_set_writer(&io, &writer_stream);
    
    // Set up dribble
    LogoStream *dribble = malloc(sizeof(LogoStream));
    memset(dribble, 0, sizeof(LogoStream));
    dribble->ops = &dribble_ops;
    dribble->is_open = true;
    mock_open_result = dribble;
    TEST_ASSERT_TRUE(logo_io_start_dribble(&io, "dribble.txt"));
    
    // Write should go to both writer AND dribble
    rw_buffer_pos = 0;
    rw_buffer[0] = '\0';
    logo_io_write(&io, "hello");
    
    // rw_buffer has writer output, last_written_text has dribble output
    TEST_ASSERT_EQUAL_STRING("hello", rw_buffer);
    TEST_ASSERT_EQUAL_STRING("hello", last_written_text);
    
    logo_io_stop_dribble(&io);
}

static bool dribble_flushed_flag = false;
static void dribble_flush_fn(LogoStream *s) { dribble_flushed_flag = true; }

static LogoStreamOps dribble_flush_ops = {
    .close = mock_stream_close,
    .write = mock_stream_write,
    .flush = dribble_flush_fn,
    .set_write_pos = mock_stream_set_write_pos,
    .get_length = mock_stream_get_length,
};

void test_flush_with_dribble(void)
{
    // Set up writer with flush tracking
    LogoStream writer_stream;
    memset(&writer_stream, 0, sizeof(writer_stream));
    writer_stream.ops = &full_rw_ops;
    writer_stream.is_open = true;
    strncpy(writer_stream.name, "output", sizeof(writer_stream.name) - 1);
    logo_io_set_writer(&io, &writer_stream);
    
    // Set up dribble
    LogoStream *dribble = malloc(sizeof(LogoStream));
    memset(dribble, 0, sizeof(LogoStream));
    dribble->ops = &dribble_flush_ops;
    dribble->is_open = true;
    mock_open_result = dribble;
    TEST_ASSERT_TRUE(logo_io_start_dribble(&io, "dribble.txt"));
    
    rw_flushed = false;
    dribble_flushed_flag = false;
    logo_io_flush(&io);
    
    TEST_ASSERT_TRUE(rw_flushed);
    TEST_ASSERT_TRUE(dribble_flushed_flag);
    
    logo_io_stop_dribble(&io);
}

void test_check_write_error_with_dribble(void)
{
    // Set up writer (no error)
    LogoStream writer_stream;
    memset(&writer_stream, 0, sizeof(writer_stream));
    writer_stream.ops = &full_rw_ops;
    writer_stream.is_open = true;
    writer_stream.write_error = false;
    strncpy(writer_stream.name, "output", sizeof(writer_stream.name) - 1);
    logo_io_set_writer(&io, &writer_stream);
    
    // Set up dribble (with error)
    LogoStream *dribble = malloc(sizeof(LogoStream));
    memset(dribble, 0, sizeof(LogoStream));
    dribble->ops = &dribble_ops;
    dribble->is_open = true;
    dribble->write_error = true;
    mock_open_result = dribble;
    TEST_ASSERT_TRUE(logo_io_start_dribble(&io, "dribble.txt"));
    
    // Should detect dribble error even if writer is fine
    TEST_ASSERT_TRUE(logo_io_check_write_error(&io));
    // Error should be cleared
    TEST_ASSERT_FALSE(dribble->write_error);
    
    logo_io_stop_dribble(&io);
}

//
// Dribble start edge cases
//

void test_start_dribble_already_active(void)
{
    LogoStream *dribble = malloc(sizeof(LogoStream));
    memset(dribble, 0, sizeof(LogoStream));
    dribble->ops = &dribble_ops;
    dribble->is_open = true;
    mock_open_result = dribble;
    
    TEST_ASSERT_TRUE(logo_io_start_dribble(&io, "dribble.txt"));
    
    // Starting again while already dribbling should fail
    TEST_ASSERT_FALSE(logo_io_start_dribble(&io, "dribble2.txt"));
    
    logo_io_stop_dribble(&io);
}

void test_start_dribble_no_storage(void)
{
    LogoIO no_storage_io;
    logo_io_init(&no_storage_io, mock_device_get_console(), NULL, &mock_hardware);
    
    TEST_ASSERT_FALSE(logo_io_start_dribble(&no_storage_io, "dribble.txt"));
    
    logo_io_cleanup(&no_storage_io);
}

//
// parse_network_address buffer too small
//

void test_parse_network_address_host_buffer_small(void)
{
    char host[4];  // Too small for "localhost"
    uint16_t port;
    
    TEST_ASSERT_FALSE(logo_io_parse_network_address("localhost:80", host, sizeof(host), &port));
}

//
// list_directory
//

static int mock_dir_callback_count = 0;
static bool mock_dir_callback(const char *name, LogoEntryType type, void *user_data)
{
    mock_dir_callback_count++;
    return true;
}

void test_list_directory(void)
{
    mock_dir_callback_count = 0;
    TEST_ASSERT_TRUE(logo_io_list_directory(&io, "/", mock_dir_callback, NULL, NULL));
}

//
// Negative timeout clamped to 0
//

void test_set_negative_timeout(void)
{
    logo_io_set_timeout(&io, -5);
    TEST_ASSERT_EQUAL(0, logo_io_get_timeout(&io));
}

//
// User interrupt fires and clears
//

void test_user_interrupt_fires(void)
{
    mock_interrupt_flag = true;
    TEST_ASSERT_TRUE(logo_io_check_user_interrupt(&io));
    // Flag should be cleared by the check
    TEST_ASSERT_FALSE(mock_interrupt_flag);
}

//
// Init without console
//

void test_init_no_console(void)
{
    LogoIO no_console_io;
    logo_io_init(&no_console_io, NULL, &mock_storage, &mock_hardware);
    
    TEST_ASSERT_NULL(no_console_io.reader);
    TEST_ASSERT_NULL(no_console_io.writer);
    
    // Cleanup should handle no console
    logo_io_cleanup(&no_console_io);
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
    RUN_TEST(test_check_pause_request);
    RUN_TEST(test_clear_pause_request);
    RUN_TEST(test_check_freeze_request);
    RUN_TEST(test_clear_freeze_request);
    RUN_TEST(test_close_all);
    RUN_TEST(test_get_open_by_index);
    RUN_TEST(test_is_network_stream);
    RUN_TEST(test_dir_exists);
    RUN_TEST(test_read_chars_operation);
    RUN_TEST(test_read_line_operation);
    RUN_TEST(test_key_available);
    RUN_TEST(test_write_line_operation);
    RUN_TEST(test_write_line_null_text);
    RUN_TEST(test_flush_operation);
    RUN_TEST(test_check_write_error);
    RUN_TEST(test_console_write);
    RUN_TEST(test_console_write_line);
    RUN_TEST(test_null_io_safety);
    RUN_TEST(test_resolve_path_relative_dotdot);
    RUN_TEST(test_resolve_path_buffer_too_small);
    RUN_TEST(test_resolve_path_prefix_no_trailing_slash);
    RUN_TEST(test_resolve_path_consecutive_slashes);
    RUN_TEST(test_open_already_open);
    RUN_TEST(test_open_max_files);
    RUN_TEST(test_open_no_storage);
    RUN_TEST(test_close_resets_reader_writer);
    RUN_TEST(test_write_with_dribble);
    RUN_TEST(test_flush_with_dribble);
    RUN_TEST(test_check_write_error_with_dribble);
    RUN_TEST(test_start_dribble_already_active);
    RUN_TEST(test_start_dribble_no_storage);
    RUN_TEST(test_parse_network_address_host_buffer_small);
    RUN_TEST(test_list_directory);
    RUN_TEST(test_set_negative_timeout);
    RUN_TEST(test_user_interrupt_fires);
    RUN_TEST(test_init_no_console);
    return UNITY_END();
}
