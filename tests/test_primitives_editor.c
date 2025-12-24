//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "test_scaffold.h"
#include <stdlib.h>

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

void test_edit_no_args_uses_empty_buffer(void)
{
    mock_device_clear_editor();
    
    Result r = run_string("(edit)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
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
    RUN_TEST(test_edit_no_args_uses_empty_buffer);
    
    return UNITY_END();
}
