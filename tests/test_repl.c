//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "test_scaffold.h"
#include "core/repl.h"
#include "core/error.h"
#include <string.h>

void setUp(void)
{
    test_scaffold_setUp();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

//==========================================================================
// Helper function tests
//==========================================================================

void test_repl_line_starts_with_to_basic(void)
{
    TEST_ASSERT_TRUE(repl_line_starts_with_to("to square"));
    TEST_ASSERT_TRUE(repl_line_starts_with_to("TO square"));
    TEST_ASSERT_TRUE(repl_line_starts_with_to("To square"));
    TEST_ASSERT_TRUE(repl_line_starts_with_to("tO square"));
}

void test_repl_line_starts_with_to_with_whitespace(void)
{
    TEST_ASSERT_TRUE(repl_line_starts_with_to("  to square"));
    TEST_ASSERT_TRUE(repl_line_starts_with_to("\tto square"));
    TEST_ASSERT_TRUE(repl_line_starts_with_to("   \t  to myproc"));
}

void test_repl_line_starts_with_to_false_cases(void)
{
    TEST_ASSERT_FALSE(repl_line_starts_with_to("toasty"));
    TEST_ASSERT_FALSE(repl_line_starts_with_to("torpedo"));
    TEST_ASSERT_FALSE(repl_line_starts_with_to("tomorrow"));
    TEST_ASSERT_FALSE(repl_line_starts_with_to("print to"));
    TEST_ASSERT_FALSE(repl_line_starts_with_to("forward 50"));
    TEST_ASSERT_FALSE(repl_line_starts_with_to(""));
}

void test_repl_line_starts_with_to_just_to(void)
{
    // "to" alone (no procedure name) should still match
    TEST_ASSERT_TRUE(repl_line_starts_with_to("to"));
    TEST_ASSERT_TRUE(repl_line_starts_with_to("to "));
    TEST_ASSERT_TRUE(repl_line_starts_with_to("  to"));
}

void test_repl_line_is_end_basic(void)
{
    TEST_ASSERT_TRUE(repl_line_is_end("end"));
    TEST_ASSERT_TRUE(repl_line_is_end("END"));
    TEST_ASSERT_TRUE(repl_line_is_end("End"));
    TEST_ASSERT_TRUE(repl_line_is_end("eNd"));
}

void test_repl_line_is_end_with_whitespace(void)
{
    TEST_ASSERT_TRUE(repl_line_is_end("  end"));
    TEST_ASSERT_TRUE(repl_line_is_end("\tend"));
    TEST_ASSERT_TRUE(repl_line_is_end("end  "));
    TEST_ASSERT_TRUE(repl_line_is_end("  end  "));
}

void test_repl_line_is_end_false_cases(void)
{
    TEST_ASSERT_FALSE(repl_line_is_end("ending"));
    TEST_ASSERT_FALSE(repl_line_is_end("endure"));
    TEST_ASSERT_FALSE(repl_line_is_end("friend"));
    TEST_ASSERT_FALSE(repl_line_is_end("the end"));
    TEST_ASSERT_FALSE(repl_line_is_end(""));
    // Bug: end must be alone on the line, not followed by more content
    TEST_ASSERT_FALSE(repl_line_is_end("end [stop]"));
    TEST_ASSERT_FALSE(repl_line_is_end("end something"));
    TEST_ASSERT_FALSE(repl_line_is_end("  end [end]"));
    TEST_ASSERT_FALSE(repl_line_is_end("end; comment"));
}

void test_repl_extract_proc_name_basic(void)
{
    char buffer[64];
    const char *name;
    
    name = repl_extract_proc_name("to square", buffer, sizeof(buffer));
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_STRING("square", name);
    
    name = repl_extract_proc_name("TO CIRCLE", buffer, sizeof(buffer));
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_STRING("CIRCLE", name);
}

void test_repl_extract_proc_name_with_inputs(void)
{
    char buffer[64];
    const char *name;
    
    name = repl_extract_proc_name("to poly :size :angle", buffer, sizeof(buffer));
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_STRING("poly", name);
}

void test_repl_extract_proc_name_with_whitespace(void)
{
    char buffer[64];
    const char *name;
    
    name = repl_extract_proc_name("  to   myproc", buffer, sizeof(buffer));
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_STRING("myproc", name);
}

void test_repl_extract_proc_name_no_name(void)
{
    char buffer[64];
    const char *name;
    
    name = repl_extract_proc_name("to", buffer, sizeof(buffer));
    TEST_ASSERT_NULL(name);
    
    name = repl_extract_proc_name("to   ", buffer, sizeof(buffer));
    TEST_ASSERT_NULL(name);
}

void test_repl_extract_proc_name_buffer_limit(void)
{
    char buffer[5];  // Small buffer
    const char *name;
    
    // Name gets truncated to fit buffer
    name = repl_extract_proc_name("to verylongprocedurename", buffer, sizeof(buffer));
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL(4, strlen(name));  // buffer_size - 1
}

void test_repl_count_bracket_balance_basic(void)
{
    TEST_ASSERT_EQUAL(1, repl_count_bracket_balance("["));
    TEST_ASSERT_EQUAL(-1, repl_count_bracket_balance("]"));
    TEST_ASSERT_EQUAL(0, repl_count_bracket_balance("[]"));
    TEST_ASSERT_EQUAL(0, repl_count_bracket_balance(""));
}

void test_repl_count_bracket_balance_nested(void)
{
    TEST_ASSERT_EQUAL(2, repl_count_bracket_balance("[["));
    TEST_ASSERT_EQUAL(-2, repl_count_bracket_balance("]]"));
    TEST_ASSERT_EQUAL(0, repl_count_bracket_balance("[[]]"));
    TEST_ASSERT_EQUAL(0, repl_count_bracket_balance("[][]"));
}

void test_repl_count_bracket_balance_with_text(void)
{
    TEST_ASSERT_EQUAL(1, repl_count_bracket_balance("repeat 4 [fd 100 rt 90"));
    TEST_ASSERT_EQUAL(0, repl_count_bracket_balance("repeat 4 [fd 100 rt 90]"));
    TEST_ASSERT_EQUAL(2, repl_count_bracket_balance("if :x = 1 [print [hello"));
}

void test_repl_count_bracket_balance_unbalanced(void)
{
    TEST_ASSERT_EQUAL(1, repl_count_bracket_balance("[hello"));
    TEST_ASSERT_EQUAL(-1, repl_count_bracket_balance("world]"));
    TEST_ASSERT_EQUAL(1, repl_count_bracket_balance("[a [b] c"));
}

//==========================================================================
// ReplState initialization tests
//==========================================================================

void test_repl_init_basic(void)
{
    ReplState state;
    LogoIO io;
    
    repl_init(&state, &io, REPL_FLAGS_FULL, "");
    
    TEST_ASSERT_EQUAL_PTR(&io, state.io);
    TEST_ASSERT_EQUAL(REPL_FLAGS_FULL, state.flags);
    TEST_ASSERT_EQUAL_STRING("", state.proc_prefix);
    TEST_ASSERT_FALSE(state.in_procedure_def);
    TEST_ASSERT_EQUAL(0, state.proc_len);
    TEST_ASSERT_EQUAL(0, state.expr_len);
    TEST_ASSERT_EQUAL(0, state.bracket_depth);
}

void test_repl_init_with_proc_prefix(void)
{
    ReplState state;
    LogoIO io;
    
    repl_init(&state, &io, REPL_FLAGS_PAUSE, "myfunc");
    
    TEST_ASSERT_EQUAL_STRING("myfunc", state.proc_prefix);
    TEST_ASSERT_EQUAL(REPL_FLAGS_PAUSE, state.flags);
}

void test_repl_init_null_prefix(void)
{
    ReplState state;
    LogoIO io;
    
    repl_init(&state, &io, REPL_FLAGS_FULL, NULL);
    
    // NULL should be converted to empty string
    TEST_ASSERT_EQUAL_STRING("", state.proc_prefix);
}

void test_repl_flags_full(void)
{
    // REPL_FLAGS_FULL should include proc def, continuation, and exit on EOF
    TEST_ASSERT_TRUE(REPL_FLAGS_FULL & REPL_FLAG_ALLOW_PROC_DEF);
    TEST_ASSERT_TRUE(REPL_FLAGS_FULL & REPL_FLAG_ALLOW_CONTINUATION);
    TEST_ASSERT_TRUE(REPL_FLAGS_FULL & REPL_FLAG_EXIT_ON_EOF);
    TEST_ASSERT_FALSE(REPL_FLAGS_FULL & REPL_FLAG_EXIT_ON_CO);
}

void test_repl_flags_pause(void)
{
    // REPL_FLAGS_PAUSE should include all features plus exit on co
    TEST_ASSERT_TRUE(REPL_FLAGS_PAUSE & REPL_FLAG_ALLOW_PROC_DEF);
    TEST_ASSERT_TRUE(REPL_FLAGS_PAUSE & REPL_FLAG_ALLOW_CONTINUATION);
    TEST_ASSERT_TRUE(REPL_FLAGS_PAUSE & REPL_FLAG_EXIT_ON_EOF);
    TEST_ASSERT_TRUE(REPL_FLAGS_PAUSE & REPL_FLAG_EXIT_ON_CO);
}

//==========================================================================
// REPL run tests (basic evaluation)
//==========================================================================

void test_repl_run_simple_print(void)
{
    ReplState state;
    
    // Set up input that will print and then EOF
    set_mock_input("print 42\n");
    
    repl_init(&state, &mock_io, REPL_FLAGS_FULL, "");
    Result r = repl_run(&state);
    
    // Should return RESULT_NONE on EOF
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Check that the prompt was shown and output was printed
    // Output includes: "?" prompt, then "42\n"
    TEST_ASSERT_TRUE(strstr(output_buffer, "42") != NULL);
}

void test_repl_run_multiple_lines(void)
{
    ReplState state;
    
    set_mock_input("print 1\nprint 2\nprint 3\n");
    
    repl_init(&state, &mock_io, REPL_FLAGS_FULL, "");
    Result r = repl_run(&state);
    
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(output_buffer, "1") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "2") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "3") != NULL);
}

void test_repl_run_empty_lines_skipped(void)
{
    ReplState state;
    
    set_mock_input("\n\nprint 99\n\n");
    
    repl_init(&state, &mock_io, REPL_FLAGS_FULL, "");
    Result r = repl_run(&state);
    
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(output_buffer, "99") != NULL);
}

void test_repl_run_comment_only_line(void)
{
    ReplState state;

    set_mock_input("; [comment]\nprint 7\n");

    repl_init(&state, &mock_io, REPL_FLAGS_FULL, "");
    Result r = repl_run(&state);

    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(output_buffer, "7") != NULL);
}

void test_repl_run_with_proc_prefix(void)
{
    ReplState state;
    
    set_mock_input("print 1\n");
    
    repl_init(&state, &mock_io, REPL_FLAGS_FULL, "myproc");
    Result r = repl_run(&state);
    
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    // The prompt should be "myproc?" - check it's in output
    TEST_ASSERT_TRUE(strstr(output_buffer, "myproc?") != NULL);
}

void test_repl_run_throw_toplevel(void)
{
    ReplState state;
    
    set_mock_input("throw \"toplevel\n");
    
    repl_init(&state, &mock_io, REPL_FLAGS_FULL, "");
    Result r = repl_run(&state);
    
    // throw "toplevel should exit the REPL with RESULT_THROW
    TEST_ASSERT_EQUAL(RESULT_THROW, r.status);
    TEST_ASSERT_EQUAL_STRING("toplevel", r.throw_tag);
}

void test_repl_run_error_handling(void)
{
    ReplState state;
    
    // Try to call an undefined procedure
    set_mock_input("nonexistent\nprint 42\n");
    
    repl_init(&state, &mock_io, REPL_FLAGS_FULL, "");
    Result r = repl_run(&state);
    
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    // Error should be printed but REPL continues
    TEST_ASSERT_TRUE(strstr(output_buffer, "don't know how to") != NULL);
    // After error, should continue and print 42
    TEST_ASSERT_TRUE(strstr(output_buffer, "42") != NULL);
}

void test_repl_run_uncaught_throw_error(void)
{
    ReplState state;
    
    // throw something other than "toplevel
    set_mock_input("throw \"myerror\nprint 1\n");
    
    repl_init(&state, &mock_io, REPL_FLAGS_FULL, "");
    Result r = repl_run(&state);
    
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    // Should print "Can't find a catch for myerror" and continue
    TEST_ASSERT_TRUE(strstr(output_buffer, "Can't find a catch") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "1") != NULL);
}

void test_repl_run_value_without_consumer(void)
{
    ReplState state;
    
    // An expression that returns a value without being consumed
    set_mock_input("sum 1 2\n");
    
    repl_init(&state, &mock_io, REPL_FLAGS_FULL, "");
    Result r = repl_run(&state);
    
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    // Should print "I don't know what to do with 3"
    TEST_ASSERT_TRUE(strstr(output_buffer, "don't know what to do") != NULL);
}

//==========================================================================
// Procedure definition tests
//==========================================================================

void test_repl_run_define_procedure(void)
{
    ReplState state;
    
    set_mock_input("to square\nprint 42\nend\n");
    
    repl_init(&state, &mock_io, REPL_FLAGS_FULL, "");
    Result r = repl_run(&state);
    
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    // Should show "square defined"
    TEST_ASSERT_TRUE(strstr(output_buffer, "square defined") != NULL);
    
    // Procedure should be defined - try calling it
    reset_output();
    run_string("square");
    TEST_ASSERT_EQUAL_STRING("42\n", output_buffer);
}

void test_repl_run_define_procedure_prompt_changes(void)
{
    ReplState state;
    
    set_mock_input("to myproc\nprint 1\nend\n");
    
    repl_init(&state, &mock_io, REPL_FLAGS_FULL, "");
    repl_run(&state);
    
    // Should show ">" prompts during definition
    TEST_ASSERT_TRUE(strstr(output_buffer, ">") != NULL);
}

void test_repl_run_define_primitive_error(void)
{
    ReplState state;
    
    // Try to define a procedure with same name as primitive
    set_mock_input("to print\nend\n");
    
    repl_init(&state, &mock_io, REPL_FLAGS_FULL, "");
    Result r = repl_run(&state);
    
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    // Should show error about primitive
    TEST_ASSERT_TRUE(strstr(output_buffer, "primitive") != NULL);
}

void test_repl_run_proc_def_in_pause(void)
{
    ReplState state;
    
    // In pause mode, procedure definitions ARE allowed
    set_mock_input("to myproc\nprint 99\nend\n");
    
    repl_init(&state, &mock_io, REPL_FLAGS_PAUSE, "test");
    Result r = repl_run(&state);
    
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    // Should show "myproc defined"
    TEST_ASSERT_TRUE(strstr(output_buffer, "myproc defined") != NULL);
    // Should show the ">" prompt during definition (prefixed with proc name)
    TEST_ASSERT_TRUE(strstr(output_buffer, "test>") != NULL);
    
    // Procedure should be callable
    reset_output();
    run_string("myproc");
    TEST_ASSERT_EQUAL_STRING("99\n", output_buffer);
}

//==========================================================================
// Bracket continuation tests
//==========================================================================

void test_repl_run_bracket_continuation(void)
{
    ReplState state;
    
    // Split a repeat over multiple lines
    set_mock_input("repeat 2 [\nprint 1\n]\n");
    
    repl_init(&state, &mock_io, REPL_FLAGS_FULL, "");
    Result r = repl_run(&state);
    
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    // Should show "~" continuation prompt
    TEST_ASSERT_TRUE(strstr(output_buffer, "~") != NULL);
    // Should execute the repeat
    // Count occurrences of "1" in output (should be 2)
    char *p = output_buffer;
    int count = 0;
    while ((p = strstr(p, "1\n")) != NULL) { count++; p++; }
    TEST_ASSERT_EQUAL(2, count);
}

void test_repl_run_continuation_in_pause(void)
{
    ReplState state;
    
    // In pause mode, bracket continuation IS allowed
    set_mock_input("repeat 2 [\nprint 1\n]\n");
    
    repl_init(&state, &mock_io, REPL_FLAGS_PAUSE, "test");
    Result r = repl_run(&state);
    
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    // Should show "~" continuation prompt (prefixed with proc name)
    TEST_ASSERT_TRUE(strstr(output_buffer, "test~") != NULL);
    // Should execute the repeat - count occurrences of "1\n"
    char *p = output_buffer;
    int count = 0;
    while ((p = strstr(p, "1\n")) != NULL) { count++; p++; }
    TEST_ASSERT_EQUAL(2, count);
}

void test_repl_throw_toplevel_from_pause_in_procedure(void)
{
    ReplState state;
    
    // Define a procedure that pauses
    Result def = proc_define_from_text("to testproc print \"before pause print \"after end");
    TEST_ASSERT_EQUAL(RESULT_OK, def.status);
    reset_output();
    
    // Set up input: call the procedure, then throw "toplevel in pause
    set_mock_input("testproc\nthrow \"toplevel\n");
    
    repl_init(&state, &mock_io, REPL_FLAGS_FULL, "");
    Result r = repl_run(&state);
    
    // throw "toplevel should exit the REPL entirely
    TEST_ASSERT_EQUAL(RESULT_THROW, r.status);
    TEST_ASSERT_EQUAL_STRING("toplevel", r.throw_tag);
    
    // Should have printed "before" and "Pausing..."
    TEST_ASSERT_TRUE(strstr(output_buffer, "before") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "Pausing") != NULL);
    
    // Should show the pause prompt "testproc?"
    TEST_ASSERT_TRUE(strstr(output_buffer, "testproc?") != NULL);
    
    // Should NOT have printed "after" since we exited via throw
    TEST_ASSERT_NULL(strstr(output_buffer, "after\n"));
}

//==========================================================================
// Main
//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    
    // Helper function tests
    RUN_TEST(test_repl_line_starts_with_to_basic);
    RUN_TEST(test_repl_line_starts_with_to_with_whitespace);
    RUN_TEST(test_repl_line_starts_with_to_false_cases);
    RUN_TEST(test_repl_line_starts_with_to_just_to);
    RUN_TEST(test_repl_line_is_end_basic);
    RUN_TEST(test_repl_line_is_end_with_whitespace);
    RUN_TEST(test_repl_line_is_end_false_cases);
    RUN_TEST(test_repl_extract_proc_name_basic);
    RUN_TEST(test_repl_extract_proc_name_with_inputs);
    RUN_TEST(test_repl_extract_proc_name_with_whitespace);
    RUN_TEST(test_repl_extract_proc_name_no_name);
    RUN_TEST(test_repl_extract_proc_name_buffer_limit);
    RUN_TEST(test_repl_count_bracket_balance_basic);
    RUN_TEST(test_repl_count_bracket_balance_nested);
    RUN_TEST(test_repl_count_bracket_balance_with_text);
    RUN_TEST(test_repl_count_bracket_balance_unbalanced);
    
    // ReplState initialization tests
    RUN_TEST(test_repl_init_basic);
    RUN_TEST(test_repl_init_with_proc_prefix);
    RUN_TEST(test_repl_init_null_prefix);
    RUN_TEST(test_repl_flags_full);
    RUN_TEST(test_repl_flags_pause);
    
    // REPL run tests
    RUN_TEST(test_repl_run_simple_print);
    RUN_TEST(test_repl_run_multiple_lines);
    RUN_TEST(test_repl_run_empty_lines_skipped);
    RUN_TEST(test_repl_run_comment_only_line);
    RUN_TEST(test_repl_run_with_proc_prefix);
    RUN_TEST(test_repl_run_throw_toplevel);
    RUN_TEST(test_repl_run_error_handling);
    RUN_TEST(test_repl_run_uncaught_throw_error);
    RUN_TEST(test_repl_run_value_without_consumer);
    
    // Procedure definition tests
    RUN_TEST(test_repl_run_define_procedure);
    RUN_TEST(test_repl_run_define_procedure_prompt_changes);
    RUN_TEST(test_repl_run_define_primitive_error);
    RUN_TEST(test_repl_run_proc_def_in_pause);
    
    // Bracket continuation tests
    RUN_TEST(test_repl_run_bracket_continuation);
    RUN_TEST(test_repl_run_continuation_in_pause);
    
    // throw "toplevel from nested pause
    RUN_TEST(test_repl_throw_toplevel_from_pause_in_procedure);
    
    return UNITY_END();
}
