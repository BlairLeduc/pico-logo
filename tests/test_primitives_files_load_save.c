//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for Load/Save primitives: load, save, savel, savepic, loadpic, pofile
//

#include "test_mock_fs.h"
#include <errno.h>

void setUp(void) { mock_fs_setUp(); }
void tearDown(void) { mock_fs_tearDown(); }

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
    
    // Create directory and set prefix after setUp_with_turtle
    // Note: use prefix without trailing slash - resolve_path will add separator
    mock_fs_create_dir("pics");
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
    
    // Create directory and file to load with prefix path
    mock_fs_create_dir("pics");
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

void test_load_recursive_loading_prevented(void)
{
    // Create a file that tries to load another file directly
    mock_fs_create_file("outer.logo", "load \"inner.logo\nmake \"outer_ran 1\n");
    mock_fs_create_file("inner.logo", "make \"inner_ran 1\n");
    
    Result r = run_string("load \"outer.logo");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NO_FILE_BUFFERS, r.error_code);
    
    // The inner file should not have been loaded
    TEST_ASSERT_FALSE(var_exists("inner_ran"));
    // outer_ran should also not be set since load was interrupted
    TEST_ASSERT_FALSE(var_exists("outer_ran"));
}

void test_load_startup_can_call_load(void)
{
    // Create a file that sets startup to load another file
    // This is NOT recursive - startup runs AFTER the file is fully loaded
    mock_fs_create_file("main.logo", "make \"startup [load \"extra.logo]\n");
    mock_fs_create_file("extra.logo", "make \"extra_loaded 1\n");
    
    Result r = run_string("load \"main.logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // The extra file should have been loaded via startup
    Value val;
    TEST_ASSERT_TRUE(var_get("extra_loaded", &val));
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
    // Setup workspace with a simple procedure using define
    // Note: define with list-of-lists creates multi-line procedures
    run_string("define \"testproc [[x y] [print :x] [print :y]]");
    run_string("make \"myvar [hello world]");
    
    Result r = run_string("save \"formatted.logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Check file content has proper formatting
    MockFile *file = mock_fs_get_file("formatted.logo", false);
    TEST_ASSERT_NOT_NULL(file);
    
    // Procedure should have proper formatting with indentation
    // With list-of-lists format, each line is on its own line with 2-space indent
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(file->data, "to testproc :x :y\n"),
        "Title line should be formatted correctly");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(file->data, "  print :x\n"),
        "First body line should have 2-space indent");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(file->data, "  print :y\n"),
        "Second body line should have 2-space indent");
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
// Savel Tests
//==========================================================================

void test_savel_saves_single_procedure(void)
{
    // Setup workspace with multiple procedures
    run_string("define \"proc1 [[] [print \"one]]");
    run_string("define \"proc2 [[] [print \"two]]");
    run_string("make \"myvar 456");
    
    Result r = run_string("savel \"proc1 \"partial.logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Check file content - should contain proc1 but not proc2
    MockFile *file = mock_fs_get_file("partial.logo", false);
    TEST_ASSERT_NOT_NULL(file);
    
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(file->data, "to proc1"),
        "Should contain proc1");
    TEST_ASSERT_NULL_MESSAGE(strstr(file->data, "to proc2"),
        "Should NOT contain proc2");
    // Should contain variables (all unburied)
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(file->data, "make \"myvar 456"),
        "Should contain variable");
}

void test_savel_saves_multiple_procedures(void)
{
    // Setup workspace
    run_string("define \"procA [[] [print \"a]]");
    run_string("define \"procB [[] [print \"b]]");
    run_string("define \"procC [[] [print \"c]]");
    
    Result r = run_string("savel [procA procC] \"multi.logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Check file content
    MockFile *file = mock_fs_get_file("multi.logo", false);
    TEST_ASSERT_NOT_NULL(file);
    
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(file->data, "to procA"),
        "Should contain procA");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(file->data, "to procC"),
        "Should contain procC");
    TEST_ASSERT_NULL_MESSAGE(strstr(file->data, "to procB"),
        "Should NOT contain procB");
}

void test_savel_saves_variables_and_properties(void)
{
    // Setup workspace
    run_string("define \"myproc [[] [print \"hi]]");
    run_string("make \"var1 100");
    run_string("make \"var2 200");
    run_string("pprop \"thing \"color \"red");
    
    Result r = run_string("savel \"myproc \"withprops.logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Check file content
    MockFile *file = mock_fs_get_file("withprops.logo", false);
    TEST_ASSERT_NOT_NULL(file);
    
    // Should contain procedure
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(file->data, "to myproc"),
        "Should contain myproc");
    // Should contain all variables
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(file->data, "make \"var1 100"),
        "Should contain var1");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(file->data, "make \"var2 200"),
        "Should contain var2");
    // Should contain property
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(file->data, "pprop \"thing \"color \"red"),
        "Should contain property");
}

void test_savel_unknown_procedure_error(void)
{
    Result r = run_string("savel \"nonexistent \"test.logo");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_savel_unknown_procedure_in_list_error(void)
{
    run_string("define \"exists [[] [print 1]]");
    
    Result r = run_string("savel [exists unknown] \"test.logo");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    
    // File should not be created
    MockFile *file = mock_fs_get_file("test.logo", false);
    TEST_ASSERT_NULL(file);
}

void test_savel_file_exists_error(void)
{
    mock_fs_create_file("exists.logo", "");
    run_string("define \"proc [[] [print 1]]");
    
    Result r = run_string("savel \"proc \"exists.logo");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_savel_invalid_name_input_error(void)
{
    Result r = run_string("savel 123 \"test.logo");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_savel_invalid_pathname_input_error(void)
{
    run_string("define \"proc [[] [print 1]]");
    
    Result r = run_string("savel \"proc [not a word]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_savel_with_prefix(void)
{
    run_string("define \"myproc [[] [print \"test]]");
    run_string("make \"testvar 42");
    
    mock_fs_create_dir("mydir");
    run_string("setprefix \"mydir");
    
    Result r = run_string("savel \"myproc \"saved.logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Verify the file was created at the right path
    MockFile *file = mock_fs_get_file("mydir/saved.logo", false);
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_NOT_NULL(strstr(file->data, "to myproc"));
    TEST_ASSERT_NOT_NULL(strstr(file->data, "make \"testvar 42"));
}

//==========================================================================
// Prefix Handling Tests (for load/save)
//==========================================================================

void test_load_with_prefix(void)
{
    mock_fs_create_dir("scripts");
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
    
    mock_fs_create_dir("saves");
    run_string("setprefix \"saves");
    
    Result r = run_string("save \"test.logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Verify the file was created at the right path
    MockFile *file = mock_fs_get_file("saves/test.logo", false);
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_NOT_NULL(strstr(file->data, "make \"testvar 99"));
}

void test_save_load_preserves_empty_list(void)
{
    // Bug: Empty list [] was being lost when saving and loading procedures.
    // This test verifies the fix works for the save/load roundtrip.
    
    // Define a procedure with an empty list using proc_define_from_text
    Result r = proc_define_from_text(
        "to test_empty\n"
        "  setwrite []\n"
        "end\n");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    // Save to file
    Result r2 = run_string("save \"empty_test.logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r2.status);
    
    // Verify file contains []
    MockFile *file = mock_fs_get_file("empty_test.logo", false);
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(file->data, "[]"),
        "Saved file should contain []");
    
    // Erase the procedure
    proc_erase("test_empty");
    TEST_ASSERT_NULL(proc_find("test_empty"));
    
    // Load the file
    Result r3 = run_string("load \"empty_test.logo");
    TEST_ASSERT_EQUAL(RESULT_NONE, r3.status);
    
    // Verify procedure was loaded with [] intact
    Result text_r = eval_string("text \"test_empty");
    TEST_ASSERT_EQUAL(RESULT_OK, text_r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, text_r.value.type);
    
    Node body = text_r.value.as.node;
    body = mem_cdr(body);  // Skip params
    Node first_line = mem_car(body);
    Node tokens = mem_cdr(first_line);  // Skip "setwrite"
    Node second_token = mem_car(tokens);
    
    TEST_ASSERT_TRUE_MESSAGE(mem_is_list(second_token),
        "Empty list should be preserved through save/load roundtrip");
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
    mock_fs_create_dir("subdir");
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

    // Load/Save tests
    RUN_TEST(test_load_executes_file);
    RUN_TEST(test_load_defines_procedure);
    RUN_TEST(test_load_runs_startup_from_file);
    RUN_TEST(test_load_does_not_run_preexisting_startup);
    RUN_TEST(test_load_runs_startup_when_file_overwrites);
    RUN_TEST(test_load_recursive_loading_prevented);
    RUN_TEST(test_load_startup_can_call_load);
    RUN_TEST(test_save_writes_workspace);
    RUN_TEST(test_save_format_matches_poall);
    RUN_TEST(test_save_file_exists_error);

    // Savel tests
    RUN_TEST(test_savel_saves_single_procedure);
    RUN_TEST(test_savel_saves_multiple_procedures);
    RUN_TEST(test_savel_saves_variables_and_properties);
    RUN_TEST(test_savel_unknown_procedure_error);
    RUN_TEST(test_savel_unknown_procedure_in_list_error);
    RUN_TEST(test_savel_file_exists_error);
    RUN_TEST(test_savel_invalid_name_input_error);
    RUN_TEST(test_savel_invalid_pathname_input_error);
    RUN_TEST(test_savel_with_prefix);

    // Prefix handling tests (load/save)
    RUN_TEST(test_load_with_prefix);
    RUN_TEST(test_save_with_prefix);
    RUN_TEST(test_save_load_preserves_empty_list);

    // Pofile tests
    RUN_TEST(test_pofile_prints_file_contents);
    RUN_TEST(test_pofile_empty_file);
    RUN_TEST(test_pofile_file_not_found);
    RUN_TEST(test_pofile_already_open_error);
    RUN_TEST(test_pofile_invalid_input);
    RUN_TEST(test_pofile_with_prefix);

    return UNITY_END();
}
