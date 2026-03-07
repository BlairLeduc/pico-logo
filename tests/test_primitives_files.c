//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for File Stream primitives: open, close, closeall,
//                                     setread, setwrite, reader, writer, allopen,
//                                     readpos, setreadpos, writepos, setwritepos,
//                                     filelen, dribble, nodribble, .timeout, .settimeout
//

#include "test_mock_fs.h"
#include <errno.h>

void setUp(void) { mock_fs_setUp(); }
void tearDown(void) { mock_fs_tearDown(); }

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

void test_open_already_open_file_error(void)
{
    mock_fs_create_file("alreadyopen.txt", "content");
    
    // First open should succeed
    Result r1 = run_string("open \"alreadyopen.txt");
    TEST_ASSERT_EQUAL(RESULT_NONE, r1.status);
    
    // Second open should fail
    Result r2 = run_string("open \"alreadyopen.txt");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r2.status);
    TEST_ASSERT_EQUAL(ERR_FILE_ALREADY_OPEN, r2.error_code);
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

void test_writepos_at_end_for_existing_file(void)
{
    // Create an existing file with content
    mock_fs_create_file("existing.txt", "hello world");
    run_string("open \"existing.txt");
    run_string("setwrite \"existing.txt");
    
    // writepos should be at end of file (11 chars)
    Result r = eval_string("writepos");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_number(r.value));
    TEST_ASSERT_EQUAL_FLOAT(11.0, r.value.as.number);
    
    // readpos should be at start
    run_string("setread \"existing.txt");
    r = eval_string("readpos");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(0.0, r.value.as.number);
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

void test_readpos_no_file_selected(void)
{
    // When reader is keyboard (no file selected), should return ERR_NO_FILE_SELECTED
    run_string("setread []");
    Result r = eval_string("readpos");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NO_FILE_SELECTED, r.error_code);
}

void test_setreadpos_no_file_selected(void)
{
    // When reader is keyboard (no file selected), should return ERR_NO_FILE_SELECTED
    run_string("setread []");
    Result r = run_string("setreadpos 2");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NO_FILE_SELECTED, r.error_code);
}

void test_writepos_no_file_selected(void)
{
    // When writer is screen (no file selected), should return ERR_NO_FILE_SELECTED
    run_string("setwrite []");
    Result r = eval_string("writepos");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NO_FILE_SELECTED, r.error_code);
}

void test_setwritepos_no_file_selected(void)
{
    // When writer is screen (no file selected), should return ERR_NO_FILE_SELECTED
    run_string("setwrite []");
    Result r = run_string("setwritepos 2");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NO_FILE_SELECTED, r.error_code);
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
// Prefix Handling Tests (for streams)
//==========================================================================

void test_open_close_with_prefix(void)
{
    // Create a directory and file in it
    mock_fs_create_dir("subdir");
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
    // Create a directory and file in it
    mock_fs_create_dir("mydir");
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

//==========================================================================
// Network timeout primitive tests
//==========================================================================

void test_timeout_returns_default(void)
{
    reset_output();
    Result r = run_string("print .timeout");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    // Default is 100 (10 seconds in tenths)
    TEST_ASSERT_EQUAL_STRING("100\n", output_buffer);
}

void test_settimeout_changes_timeout(void)
{
    reset_output();
    Result r = run_string(".settimeout 200 print .timeout");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_STRING("200\n", output_buffer);
    
    // Restore default
    run_string(".settimeout 100");
}

void test_settimeout_zero_allowed(void)
{
    reset_output();
    Result r = run_string(".settimeout 0 print .timeout");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_STRING("0\n", output_buffer);
    
    // Restore default
    run_string(".settimeout 100");
}

void test_settimeout_negative_error(void)
{
    Result r = run_string(".settimeout -1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_settimeout_non_number_error(void)
{
    Result r = run_string(".settimeout \"abc");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
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
    RUN_TEST(test_open_already_open_file_error);
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
    RUN_TEST(test_writepos_at_end_for_existing_file);
    
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
    RUN_TEST(test_readpos_no_file_selected);
    RUN_TEST(test_setreadpos_no_file_selected);
    RUN_TEST(test_writepos_no_file_selected);
    RUN_TEST(test_setwritepos_no_file_selected);
    RUN_TEST(test_setwritepos_invalid_input);
    RUN_TEST(test_setwritepos_negative);

    // Prefix handling tests (streams)
    RUN_TEST(test_open_close_with_prefix);
    RUN_TEST(test_setread_setwrite_with_prefix);

    // Network timeout tests
    RUN_TEST(test_timeout_returns_default);
    RUN_TEST(test_settimeout_changes_timeout);
    RUN_TEST(test_settimeout_zero_allowed);
    RUN_TEST(test_settimeout_negative_error);
    RUN_TEST(test_settimeout_non_number_error);

    return UNITY_END();
}
