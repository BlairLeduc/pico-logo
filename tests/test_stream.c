//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Unit tests for the stream wrapper functions (devices/stream.c).
//

#include "unity.h"
#include "devices/stream.h"
#include <string.h>

//
// Mock stream context for tracking write operations
//

typedef struct MockWriteContext
{
    char buffer[256];
    size_t write_pos;
    int write_count;
} MockWriteContext;

static MockWriteContext mock_context;

static void mock_write(LogoStream *stream, const char *text)
{
    MockWriteContext *ctx = (MockWriteContext *)stream->context;
    if (text)
    {
        size_t len = strlen(text);
        if (ctx->write_pos + len < sizeof(ctx->buffer))
        {
            strcpy(ctx->buffer + ctx->write_pos, text);
            ctx->write_pos += len;
        }
    }
    ctx->write_count++;
}

static const LogoStreamOps mock_write_ops = {
    .read_char = NULL,
    .read_chars = NULL,
    .read_line = NULL,
    .can_read = NULL,
    .write = mock_write,
    .flush = NULL,
    .get_read_pos = NULL,
    .set_read_pos = NULL,
    .get_write_pos = NULL,
    .set_write_pos = NULL,
    .get_length = NULL,
    .close = NULL
};

static LogoStream test_stream;

void setUp(void)
{
    memset(&mock_context, 0, sizeof(mock_context));
    memset(&test_stream, 0, sizeof(test_stream));
    logo_stream_init(&test_stream, LOGO_STREAM_FILE, &mock_write_ops, &mock_context, "test");
}

void tearDown(void)
{
    // Nothing to tear down
}

//============================================================================
// logo_stream_write_line Tests
//============================================================================

void test_write_line_with_text(void)
{
    logo_stream_write_line(&test_stream, "hello");
    
    TEST_ASSERT_EQUAL_STRING("hello\n", mock_context.buffer);
    TEST_ASSERT_EQUAL(2, mock_context.write_count);  // text + newline
}

void test_write_line_with_null_text(void)
{
    logo_stream_write_line(&test_stream, NULL);
    
    TEST_ASSERT_EQUAL_STRING("\n", mock_context.buffer);
    TEST_ASSERT_EQUAL(1, mock_context.write_count);  // only newline
}

void test_write_line_with_empty_text(void)
{
    logo_stream_write_line(&test_stream, "");
    
    TEST_ASSERT_EQUAL_STRING("\n", mock_context.buffer);
    TEST_ASSERT_EQUAL(2, mock_context.write_count);  // empty text + newline
}

void test_write_line_with_null_stream(void)
{
    // Should not crash
    logo_stream_write_line(NULL, "hello");
    
    // Buffer should be unchanged
    TEST_ASSERT_EQUAL_STRING("", mock_context.buffer);
    TEST_ASSERT_EQUAL(0, mock_context.write_count);
}

void test_write_line_with_closed_stream(void)
{
    test_stream.is_open = false;
    
    logo_stream_write_line(&test_stream, "hello");
    
    // Should not write
    TEST_ASSERT_EQUAL_STRING("", mock_context.buffer);
    TEST_ASSERT_EQUAL(0, mock_context.write_count);
}

void test_write_line_multiple_calls(void)
{
    logo_stream_write_line(&test_stream, "line1");
    logo_stream_write_line(&test_stream, "line2");
    
    TEST_ASSERT_EQUAL_STRING("line1\nline2\n", mock_context.buffer);
    TEST_ASSERT_EQUAL(4, mock_context.write_count);
}

//============================================================================
// logo_stream_clear_write_error Tests
//============================================================================

void test_clear_write_error_clears_flag(void)
{
    test_stream.write_error = true;
    TEST_ASSERT_TRUE(logo_stream_has_write_error(&test_stream));
    
    logo_stream_clear_write_error(&test_stream);
    
    TEST_ASSERT_FALSE(logo_stream_has_write_error(&test_stream));
}

void test_clear_write_error_on_clean_stream(void)
{
    TEST_ASSERT_FALSE(logo_stream_has_write_error(&test_stream));
    
    // Should not crash when clearing already-clear error
    logo_stream_clear_write_error(&test_stream);
    
    TEST_ASSERT_FALSE(logo_stream_has_write_error(&test_stream));
}

void test_clear_write_error_with_null_stream(void)
{
    // Should not crash
    logo_stream_clear_write_error(NULL);
}

void test_has_write_error_with_null_stream(void)
{
    // Should return false for null stream
    TEST_ASSERT_FALSE(logo_stream_has_write_error(NULL));
}

void test_write_error_flag_persistence(void)
{
    // Set error, write, verify error persists
    test_stream.write_error = true;
    
    logo_stream_write_line(&test_stream, "test");
    
    // Error flag should still be set (write doesn't clear it)
    TEST_ASSERT_TRUE(logo_stream_has_write_error(&test_stream));
    
    // Only clear_write_error clears it
    logo_stream_clear_write_error(&test_stream);
    TEST_ASSERT_FALSE(logo_stream_has_write_error(&test_stream));
}

//============================================================================
// Main
//============================================================================

int main(void)
{
    UNITY_BEGIN();
    
    // logo_stream_write_line tests
    RUN_TEST(test_write_line_with_text);
    RUN_TEST(test_write_line_with_null_text);
    RUN_TEST(test_write_line_with_empty_text);
    RUN_TEST(test_write_line_with_null_stream);
    RUN_TEST(test_write_line_with_closed_stream);
    RUN_TEST(test_write_line_multiple_calls);
    
    // logo_stream_clear_write_error tests
    RUN_TEST(test_clear_write_error_clears_flag);
    RUN_TEST(test_clear_write_error_on_clean_stream);
    RUN_TEST(test_clear_write_error_with_null_stream);
    RUN_TEST(test_has_write_error_with_null_stream);
    RUN_TEST(test_write_error_flag_persistence);
    
    return UNITY_END();
}
