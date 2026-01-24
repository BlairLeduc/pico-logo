//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "test_scaffold.h"

void setUp(void)
{
    test_scaffold_setUp();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

//==========================================================================
// Core Evaluation Tests
//==========================================================================

void test_eval_number(void)
{
    Result r = eval_string("42");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_NUMBER, r.value.type);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, r.value.as.number);
}

void test_eval_negative_number(void)
{
    Result r = eval_string("-5");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(-5.0f, r.value.as.number);
}

void test_eval_infix_add(void)
{
    Result r = eval_string("3 + 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, r.value.as.number);
}

void test_eval_infix_precedence(void)
{
    Result r = eval_string("3 + 4 * 2");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(11.0f, r.value.as.number);
}

void test_eval_parentheses(void)
{
    Result r = eval_string("(3 + 4) * 2");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(14.0f, r.value.as.number);
}

void test_eval_parentheses_zero_arg_primitive_with_infix(void)
{
    // Bug: (xcor+3) should work like xcor+3
    // xcor at home is 0, so xcor+3 = 3
    Result r = eval_string("(xcor+3)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, r.value.as.number);
}

void test_eval_quoted_word(void)
{
    Result r = eval_string("\"hello");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("hello", mem_word_ptr(r.value.as.node));
}

//==========================================================================
// Print Tests (output primitives)
//==========================================================================

void test_print(void)
{
    run_string("print 42");
    TEST_ASSERT_EQUAL_STRING("42\n", output_buffer);
}

void test_print_word(void)
{
    run_string("print \"hello");
    TEST_ASSERT_EQUAL_STRING("hello\n", output_buffer);
}

void test_print_variadic_parens(void)
{
    run_string("(print 1 2 3)");
    TEST_ASSERT_EQUAL_STRING("1 2 3\n", output_buffer);
}

//==========================================================================
// Error Message Tests
//==========================================================================

void test_error_dont_know_how(void)
{
    // I don't know how to foobar
    Result r = eval_string("foobar");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DONT_KNOW_HOW, r.error_code);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("I don't know how to foobar", msg);
}

void test_error_not_enough_inputs(void)
{
    // Not enough inputs to sum
    Result r = eval_string("sum 1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NOT_ENOUGH_INPUTS, r.error_code);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("Not enough inputs to sum", msg);
}

void test_error_uses_alias_name_fd(void)
{
    // When using alias "fd" instead of "forward", error should say "fd"
    Result r = eval_string("fd \"hello");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("fd doesn't like hello as input", msg);
}

void test_error_uses_full_name_forward(void)
{
    // When using full name "forward", error should say "forward"
    Result r = eval_string("forward \"hello");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("forward doesn't like hello as input", msg);
}

void test_error_uses_alias_name_bk(void)
{
    // When using alias "bk" instead of "back", error should say "bk"
    Result r = eval_string("bk \"hello");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("bk doesn't like hello as input", msg);
}

void test_error_infix_doesnt_like(void)
{
    // + doesn't like hello as input
    Result r = eval_string("1 + \"hello");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("+ doesn't like hello as input", msg);
}

void test_error_bracket_mismatch(void)
{
    // Unmatched right bracket - use run_string since fd is a command
    Result r = run_string("fd 8]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_BRACKET_MISMATCH, r.error_code);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("] without [", msg);
}

void test_error_paren_mismatch(void)
{
    // Unmatched right parenthesis - use run_string since print is a command
    Result r = run_string("print 3)");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_PAREN_MISMATCH, r.error_code);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING(") without (", msg);
}

void test_error_in_procedure_includes_proc_name(void)
{
    // Define a procedure that causes an error (sum with non-numeric input)
    const char *params[] = {};
    define_proc("badproc", params, 0, "print sum \"hello 1");
    
    Result r = run_string("badproc");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
    TEST_ASSERT_EQUAL_STRING("badproc", r.error_caller);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("sum doesn't like hello as input in badproc", msg);
}

void test_error_in_nested_procedure_includes_innermost_proc_name(void)
{
    // Define inner procedure that causes error
    const char *params[] = {};
    define_proc("inner", params, 0, "print sum \"hello 1");
    define_proc("outer", params, 0, "inner");
    
    Result r = run_string("outer");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
    // Error should report innermost procedure where error occurred
    TEST_ASSERT_EQUAL_STRING("inner", r.error_caller);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("sum doesn't like hello as input in inner", msg);
}

void test_error_divide_by_zero_in_procedure(void)
{
    // Define a procedure that divides by zero
    const char *params[] = {};
    define_proc("divzero", params, 0, "print 1 / 0");
    
    Result r = run_string("divzero");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DIVIDE_BY_ZERO, r.error_code);
    TEST_ASSERT_EQUAL_STRING("divzero", r.error_caller);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("Can't divide by zero in divzero", msg);
}

void test_error_no_value_in_procedure(void)
{
    // Define a procedure that accesses undefined variable
    const char *params[] = {};
    define_proc("usevar", params, 0, "print :undefined");
    
    Result r = run_string("usevar");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NO_VALUE, r.error_code);
    TEST_ASSERT_EQUAL_STRING("usevar", r.error_caller);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("undefined has no value in usevar", msg);
}

//==========================================================================
// Infix Equality Tests
//==========================================================================

void test_infix_equal_words(void)
{
    // Test that = works with words
    Result r = eval_string("\"hello = \"hello");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_infix_equal_words_false(void)
{
    // Test that = returns false for different words
    Result r = eval_string("\"hello = \"world");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_infix_equal_variable_word(void)
{
    // Test the specific case from user: make "ans "f pr :ans = "f
    run_string("make \"ans \"f");
    Result r = eval_string(":ans = \"f");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_infix_equal_numbers(void)
{
    // Test that = still works with numbers
    Result r = eval_string("3 = 3");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_infix_equal_number_word(void)
{
    // Test that = works when comparing numeric word to number
    Result r = eval_string("\"3 = 3");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_infix_equal_lists(void)
{
    // Test that = works with identical lists
    Result r = eval_string("[1 2 3] = [1 2 3]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_infix_equal_lists_false(void)
{
    // Test that = returns false for different lists
    Result r = eval_string("[1 2 3] = [1 2 4]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_infix_equal_lists_different_length(void)
{
    // Test that = returns false for lists of different lengths
    Result r = eval_string("[1 2] = [1 2 3]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_infix_equal_empty_lists(void)
{
    // Test that = works with empty lists
    Result r = eval_string("[] = []");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_infix_equal_nested_lists(void)
{
    // Test that = works with nested lists
    Result r = eval_string("[[a b] [c d]] = [[a b] [c d]]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_infix_equal_nested_lists_false(void)
{
    // Test that = returns false for different nested lists
    Result r = eval_string("[[a b] [c d]] = [[a b] [c e]]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

//==========================================================================
// Backslash Escape Tests
//==========================================================================

void test_quoted_word_escape_hyphen(void)
{
    // "H\-1 should become H-1
    Result r = eval_string("\"H\\-1");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("H-1", mem_word_ptr(r.value.as.node));
}

void test_quoted_word_escape_middle(void)
{
    // "h\ee should become hee
    Result r = eval_string("\"h\\ee");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("hee", mem_word_ptr(r.value.as.node));
}

void test_print_escaped_hyphen(void)
{
    // pr "H\-1 should print H-1
    run_string("print \"H\\-1");
    TEST_ASSERT_EQUAL_STRING("H-1\n", output_buffer);
}

void test_quoted_word_escape_space(void)
{
    // "San\ Francisco should become San Francisco
    Result r = eval_string("\"San\\ Francisco");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("San Francisco", mem_word_ptr(r.value.as.node));
}

void test_quoted_word_escape_bracket(void)
{
    // "\[ should become [
    Result r = eval_string("\"\\[");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("[", mem_word_ptr(r.value.as.node));
}

void test_variable_escape_in_name(void)
{
    // make "test\-1 42, then :test\-1 should get 42
    run_string("make \"test\\-1 42");
    Result r = eval_string(":test\\-1");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_NUMBER, r.value.type);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, r.value.as.number);
}

int main(void)
{
    UNITY_BEGIN();

    // Core evaluation tests
    RUN_TEST(test_eval_number);
    RUN_TEST(test_eval_negative_number);
    RUN_TEST(test_eval_infix_add);
    RUN_TEST(test_eval_infix_precedence);
    RUN_TEST(test_eval_parentheses);
    RUN_TEST(test_eval_parentheses_zero_arg_primitive_with_infix);
    RUN_TEST(test_eval_quoted_word);

    // Print tests
    RUN_TEST(test_print);
    RUN_TEST(test_print_word);
    RUN_TEST(test_print_variadic_parens);

    // Error message tests
    RUN_TEST(test_error_dont_know_how);
    RUN_TEST(test_error_not_enough_inputs);
    RUN_TEST(test_error_uses_alias_name_fd);
    RUN_TEST(test_error_uses_full_name_forward);
    RUN_TEST(test_error_uses_alias_name_bk);
    RUN_TEST(test_error_infix_doesnt_like);
    RUN_TEST(test_error_bracket_mismatch);
    RUN_TEST(test_error_paren_mismatch);
    
    // Error messages in procedures tests
    RUN_TEST(test_error_in_procedure_includes_proc_name);
    RUN_TEST(test_error_in_nested_procedure_includes_innermost_proc_name);
    RUN_TEST(test_error_divide_by_zero_in_procedure);
    RUN_TEST(test_error_no_value_in_procedure);

    // Infix equality tests
    RUN_TEST(test_infix_equal_words);
    RUN_TEST(test_infix_equal_words_false);
    RUN_TEST(test_infix_equal_variable_word);
    RUN_TEST(test_infix_equal_numbers);
    RUN_TEST(test_infix_equal_number_word);
    RUN_TEST(test_infix_equal_lists);
    RUN_TEST(test_infix_equal_lists_false);
    RUN_TEST(test_infix_equal_lists_different_length);
    RUN_TEST(test_infix_equal_empty_lists);
    RUN_TEST(test_infix_equal_nested_lists);
    RUN_TEST(test_infix_equal_nested_lists_false);

    // Backslash escape tests
    RUN_TEST(test_quoted_word_escape_hyphen);
    RUN_TEST(test_quoted_word_escape_middle);
    RUN_TEST(test_print_escaped_hyphen);
    RUN_TEST(test_quoted_word_escape_space);
    RUN_TEST(test_quoted_word_escape_bracket);
    RUN_TEST(test_variable_escape_in_name);

    return UNITY_END();
}

