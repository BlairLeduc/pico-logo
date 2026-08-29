//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for Directory primitives: files, directories, catalog,
//                                   createdir, erasedir, erasefile,
//                                   filep, dirp, rename, setprefix, prefix
//

#include "test_mock_fs.h"
#include <errno.h>

void setUp(void) { mock_fs_setUp(); }
void tearDown(void) { mock_fs_tearDown(); }

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

void test_catalog_with_pathname_runs_without_error(void)
{
    // (catalog pathname) should run without error
    output_pos = 0;
    output_buffer[0] = '\0';
    Result r = run_string("(catalog \".)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    // We just verify it doesn't crash
}

void test_catalog_with_absolute_pathname(void)
{
    // Create a directory with some files to catalog
    mock_fs_reset();
    mock_fs_create_file("/testdir/file1.txt", "content1");
    mock_fs_create_file("/testdir/file2.txt", "content2");
    
    output_pos = 0;
    output_buffer[0] = '\0';
    Result r = run_string("(catalog \"/testdir)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    // Check that the output contains the files
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "file1.txt"));
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "file2.txt"));
}

void test_catalog_with_relative_pathname(void)
{
    // Create files in a subdirectory
    mock_fs_reset();
    mock_fs_create_file("/Logo/subdir/test.txt", "content");
    
    // Set prefix to /Logo/
    output_pos = 0;
    output_buffer[0] = '\0';
    run_string("setprefix \"/Logo");
    
    Result r = run_string("(catalog \"subdir)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    // Check that the output contains the file
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "test.txt"));
}

void test_catalog_with_invalid_input_error(void)
{
    // (catalog [not a word]) should error
    Result r = run_string("(catalog [not a word])");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_catalog_long_format_shows_size(void)
{
    // catalog is the long (ls -l) form: each file line carries its byte size
    mock_fs_reset();
    mock_fs_create_file("/testdir/data.txt", "12345"); // 5 bytes

    output_pos = 0;
    output_buffer[0] = '\0';
    Result r = run_string("(catalog \"/testdir)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "data.txt"));
    // The size column shows the 5-byte length
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "5  "));
}

void test_catalog_long_format_marks_directories(void)
{
    // Directories have a blank size column and a trailing slash
    mock_fs_reset();
    mock_fs_create_dir("/testdir/sub");

    output_pos = 0;
    output_buffer[0] = '\0';
    Result r = run_string("(catalog \"/testdir)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "sub/"));
    // No size marker shown for directories - the size column is blank
    TEST_ASSERT_NULL(strstr(output_buffer, "<DIR>"));
}

void test_cat_runs_without_error(void)
{
    // cat (columnar, ls-style) should run without error
    output_pos = 0;
    output_buffer[0] = '\0';
    Result r = run_string("cat");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_cat_lists_files(void)
{
    mock_fs_reset();
    mock_fs_create_file("/testdir/alpha.txt", "a");
    mock_fs_create_file("/testdir/beta.txt", "bb");

    output_pos = 0;
    output_buffer[0] = '\0';
    Result r = run_string("(cat \"/testdir)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "alpha.txt"));
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "beta.txt"));
}

void test_cat_with_invalid_input_error(void)
{
    // (cat [not a word]) should error
    Result r = run_string("(cat [not a word])");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
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

void test_free_reports_blocks(void)
{
    // The mock filesystem reports 100 free blocks of 512 bytes each.
    Result r = eval_string("free");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    // Expect the list [100 512].
    Node list = value_to_node(r.value);
    TEST_ASSERT_EQUAL_STRING("100", mem_word_ptr(mem_car(list)));
    TEST_ASSERT_EQUAL_STRING("512", mem_word_ptr(mem_car(mem_cdr(list))));
}

void test_free_with_pathname(void)
{
    Result r = eval_string("(free \"/anywhere)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    Node list = value_to_node(r.value);
    TEST_ASSERT_EQUAL_STRING("100", mem_word_ptr(mem_car(list)));
}

// B64: `free` on the internal volume reported "There is no SD card". Any
// free-space failure was blamed on a missing card, even for a path that never
// touches the SD slot.
void test_free_on_internal_volume_reports_disk_trouble(void)
{
    mock_fs_free_blocks_ok = false;

    Result r = eval_string("free");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DISK_TROUBLE, result_get_error_code(r));
}

// A missing card is still named as such when the path is the SD mount.
void test_free_on_absent_sd_reports_no_sd_card(void)
{
    mock_fs_free_blocks_ok = false;
    mock_fs_sd_present = false;

    Result r = eval_string("(free \"/sd)");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NO_SD_CARD, result_get_error_code(r));
}

// B64: `fscheck` exists so a broken filesystem can name its own error code --
// `free` runs the same walk but swallows the reason.
void test_fscheck_reports_a_clean_filesystem(void)
{
    reset_output();

    Result r = run_string("fscheck");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "Internal filesystem: OK"));
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "blocks read 7"));
    // A clean walk does not pay for the file scan.
    TEST_ASSERT_NULL(strstr(output_buffer, "every file reads back"));
}

void test_fscheck_prints_the_filesystems_own_error_code(void)
{
    mock_fs_check_code = -84; // LFS_ERR_CORRUPT
    reset_output();

    Result r = run_string("fscheck");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "Internal filesystem: DAMAGED"));
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "error       -84"));
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "stopped on  block 132"));
}

// B65: the walk stopping on a null block pointer must be legible. Reported as
// the raw 4294967295 it came back from the board as "4.29496e9", which single
// precision cannot hold exactly and no one can read.
void test_fscheck_names_a_null_block_pointer_instead_of_printing_it(void)
{
    mock_fs_check_code = -84;
    mock_fs_check_last_block = -1;
    reset_output();

    run_string("fscheck");
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "stopped on  a null block pointer"));
    TEST_ASSERT_NULL(strstr(output_buffer, "4.29496"));
    TEST_ASSERT_NULL(strstr(output_buffer, "4294967295"));
}

// A failed walk always runs the file scan: naming the bad file is the point, so
// the reader should not have to ask twice.
void test_fscheck_names_the_unreadable_file_without_erasing_it(void)
{
    mock_fs_check_code = -84;
    mock_fs_bad_file = "/sketches/rocks";
    mock_fs_create_file("sketches/rocks", "data");
    reset_output();

    run_string("fscheck");
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "unreadable  /sketches/rocks"));
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "412 bytes"));
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "erase"));
    // Reporting only: the named file is still there.
    TEST_ASSERT_TRUE(mock_storage_file_exists("sketches/rocks"));
}

// The walk never reads a file's data blocks, so a volume that walks clean can
// still hold an unreadable file. That is what the opt-in scan is for.
void test_fscheck_scan_checks_files_on_a_volume_that_walks_clean(void)
{
    mock_fs_bad_file = "/startup";
    reset_output();

    Result r = run_string("(fscheck \"scan)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "Internal filesystem: OK"));
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "unreadable  /startup"));
}

// `fsck` is the abbreviation, and must be the same primitive rather than a
// second copy that can drift.
void test_fsck_is_an_alias_for_fscheck(void)
{
    mock_fs_check_code = -84;
    reset_output();

    Result r = run_string("fsck");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "Internal filesystem: DAMAGED"));
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "error       -84"));
}

void test_fscheck_rejects_an_unknown_keyword(void)
{
    Result r = run_string("(fscheck \"repair)");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));
}

void test_setprefix_and_prefix(void)
{
    // Create the directory first
    mock_fs_create_dir("my/path");
    // Using \/ to escape the forward slash - should result in unescaped "my/path"
    Result r = run_string("setprefix \"my\\/path");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    Result r2 = eval_string("prefix");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    // Prefix should have trailing slash
    TEST_ASSERT_EQUAL_STRING("my/path/", mem_word_ptr(r2.value.as.node));
}

void test_sp_alias_sets_prefix(void)
{
    // sp is an alias for setprefix
    mock_fs_create_dir("my/path");
    Result r = run_string("sp \"my\\/path");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    Result r2 = eval_string("prefix");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_STRING("my/path/", mem_word_ptr(r2.value.as.node));
}

void test_setprefix_nonexistent_directory(void)
{
    // Try to set prefix to a directory that doesn't exist
    Result r = run_string("setprefix \"nonexistent");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_SUBDIR_NOT_FOUND, result_get_error_code(r));
}

void test_setprefix_root_directory(void)
{
    // Root directory "/" should always succeed without checking
    Result r = run_string("setprefix \"/");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    Result r2 = eval_string("prefix");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_STRING("/", mem_word_ptr(r2.value.as.node));
}

void test_setprefix_relative_with_root_prefix(void)
{
    // Set prefix to root first
    run_string("setprefix \"/");
    
    // Create a directory at /Logo (which is "Logo" relative to root prefix "/")
    mock_fs_create_dir("/Logo");
    
    // Now setprefix to "Logo" should resolve to "/Logo" and succeed
    // The prefix should be set to the resolved path "/Logo/"
    Result r = run_string("setprefix \"Logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    Result r2 = eval_string("prefix");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_STRING("/Logo/", mem_word_ptr(r2.value.as.node));
}

void test_setprefix_absolute_path(void)
{
    // Create an absolute path directory
    mock_fs_create_dir("/Logo");
    
    // Absolute paths should work regardless of current prefix
    Result r = run_string("setprefix \"/Logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    Result r2 = eval_string("prefix");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_STRING("/Logo/", mem_word_ptr(r2.value.as.node));
}

void test_setprefix_relative_with_trailing_slash_prefix(void)
{
    // Create directory structure: /Logo/apple
    mock_fs_create_dir("/Logo");
    mock_fs_create_dir("/Logo/apple");
    
    // Set prefix to "/Logo/" by setting it programmatically
    LogoIO *io = primitives_get_io();
    TEST_ASSERT_NOT_NULL(io);
    strcpy(io->prefix, "/Logo/");
    
    // Now setprefix to "apple" should resolve to "/Logo/apple" and succeed
    // The prefix should be set to the resolved path "/Logo/apple/"
    Result r2 = run_string("setprefix \"apple");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_NONE, r2.status, "setprefix to apple should succeed");
    
    Result r3 = eval_string("prefix");
    TEST_ASSERT_EQUAL(RESULT_OK, r3.status);
    TEST_ASSERT_EQUAL_STRING("/Logo/apple/", mem_word_ptr(r3.value.as.node));
}

void test_setprefix_parent_directory(void)
{
    // Create directory structure: /Logo/apple
    mock_fs_create_dir("/Logo");
    mock_fs_create_dir("/Logo/apple");
    
    // Set prefix to "/Logo/apple/"
    LogoIO *io = primitives_get_io();
    TEST_ASSERT_NOT_NULL(io);
    strcpy(io->prefix, "/Logo/apple/");
    
    // setprefix ".." should go up to /Logo
    Result r = run_string("setprefix \"..");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_NONE, r.status, "setprefix to .. should succeed");
    
    Result r2 = eval_string("prefix");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_STRING("/Logo/", mem_word_ptr(r2.value.as.node));
}

void test_setprefix_parent_directory_with_subdir(void)
{
    // Create directory structure: /Logo/apple and /Logo/banana
    mock_fs_create_dir("/Logo");
    mock_fs_create_dir("/Logo/apple");
    mock_fs_create_dir("/Logo/banana");
    
    // Set prefix to "/Logo/apple/"
    LogoIO *io = primitives_get_io();
    TEST_ASSERT_NOT_NULL(io);
    strcpy(io->prefix, "/Logo/apple/");
    
    // setprefix "..\\/banana" should go up to /Logo then into banana
    Result r = run_string("setprefix \"..\\/banana");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_NONE, r.status, "setprefix to ../banana should succeed");
    
    Result r2 = eval_string("prefix");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_STRING("/Logo/banana/", mem_word_ptr(r2.value.as.node));
}

void test_setprefix_parent_at_root(void)
{
    // Create directory structure: /Logo
    mock_fs_create_dir("/Logo");
    
    // Set prefix to "/Logo/"
    LogoIO *io = primitives_get_io();
    TEST_ASSERT_NOT_NULL(io);
    strcpy(io->prefix, "/Logo/");
    
    // setprefix ".." should go to root /
    Result r = run_string("setprefix \"..");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_NONE, r.status, "setprefix to .. from /Logo should go to root");
    
    Result r2 = eval_string("prefix");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_STRING("/", mem_word_ptr(r2.value.as.node));
}

void test_setprefix_multiple_parent_dirs(void)
{
    // Create directory structure: /Logo/apple/banana
    mock_fs_create_dir("/Logo");
    mock_fs_create_dir("/Logo/apple");
    mock_fs_create_dir("/Logo/apple/banana");
    
    // Set prefix to "/Logo/apple/banana/"
    LogoIO *io = primitives_get_io();
    TEST_ASSERT_NOT_NULL(io);
    strcpy(io->prefix, "/Logo/apple/banana/");
    
    // setprefix "..\\/..\\/.." should go up three levels to root
    Result r = run_string("setprefix \"..\\/..\\/..");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_NONE, r.status, "setprefix with multiple .. should succeed");
    
    Result r2 = eval_string("prefix");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_STRING("/", mem_word_ptr(r2.value.as.node));
}

//==========================================================================
// Main
//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    
    // Directory listing tests
    RUN_TEST(test_files_returns_list);
    RUN_TEST(test_files_with_extension_returns_list);
    RUN_TEST(test_files_with_star_returns_all);
    RUN_TEST(test_files_invalid_input_error);
    RUN_TEST(test_directories_returns_list);
    RUN_TEST(test_catalog_runs_without_error);
    RUN_TEST(test_catalog_with_pathname_runs_without_error);
    RUN_TEST(test_catalog_with_absolute_pathname);
    RUN_TEST(test_catalog_with_relative_pathname);
    RUN_TEST(test_catalog_with_invalid_input_error);
    RUN_TEST(test_catalog_long_format_shows_size);
    RUN_TEST(test_catalog_long_format_marks_directories);
    RUN_TEST(test_cat_runs_without_error);
    RUN_TEST(test_cat_lists_files);
    RUN_TEST(test_cat_with_invalid_input_error);

    // Directory Management tests
    RUN_TEST(test_createdir);
    RUN_TEST(test_erasedir);
    RUN_TEST(test_erasefile);
    RUN_TEST(test_filep_true);
    RUN_TEST(test_filep_false);
    RUN_TEST(test_dirp_true);
    RUN_TEST(test_dirp_false);
    RUN_TEST(test_rename_file);
    RUN_TEST(test_free_reports_blocks);
    RUN_TEST(test_free_with_pathname);
    RUN_TEST(test_free_on_internal_volume_reports_disk_trouble);
    RUN_TEST(test_free_on_absent_sd_reports_no_sd_card);
    RUN_TEST(test_fscheck_reports_a_clean_filesystem);
    RUN_TEST(test_fscheck_prints_the_filesystems_own_error_code);
    RUN_TEST(test_fscheck_names_a_null_block_pointer_instead_of_printing_it);
    RUN_TEST(test_fscheck_names_the_unreadable_file_without_erasing_it);
    RUN_TEST(test_fscheck_scan_checks_files_on_a_volume_that_walks_clean);
    RUN_TEST(test_fsck_is_an_alias_for_fscheck);
    RUN_TEST(test_fscheck_rejects_an_unknown_keyword);
    RUN_TEST(test_setprefix_and_prefix);
    RUN_TEST(test_sp_alias_sets_prefix);
    RUN_TEST(test_setprefix_nonexistent_directory);
    RUN_TEST(test_setprefix_root_directory);
    RUN_TEST(test_setprefix_relative_with_root_prefix);
    RUN_TEST(test_setprefix_absolute_path);
    RUN_TEST(test_setprefix_relative_with_trailing_slash_prefix);
    RUN_TEST(test_setprefix_parent_directory);
    RUN_TEST(test_setprefix_parent_directory_with_subdir);
    RUN_TEST(test_setprefix_parent_at_root);
    RUN_TEST(test_setprefix_multiple_parent_dirs);

    return UNITY_END();
}
