//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "test_scaffold.h"
#include <stdlib.h>
#include <string.h>

void setUp(void)
{
    // Use device setup to get mock editor support
    test_scaffold_setUp_with_device();
}

void tearDown(void)
{
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
    
    return UNITY_END();
}
