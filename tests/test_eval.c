//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "unity.h"
#include "core/memory.h"
#include "core/lexer.h"
#include "core/eval.h"
#include "core/error.h"
#include "core/primitives.h"
#include "core/variables.h"
#include "devices/device.h"
#include <string.h>

static char output_buffer[1024];
static int output_pos;

// Mock device for capturing print output
static void mock_write(void *context, const char *text)
{
    (void)context;
    size_t len = strlen(text);
    if (output_pos + (int)len < (int)sizeof(output_buffer))
    {
        memcpy(output_buffer + output_pos, text, len);
        output_pos += len;
    }
    output_buffer[output_pos] = '\0';
}

static int mock_read_line(void *context, char *buffer, size_t buffer_size)
{
    (void)context;
    (void)buffer;
    (void)buffer_size;
    return -1;
}

static void mock_flush(void *context)
{
    (void)context;
}

static LogoDeviceOps mock_ops = {
    .read_line = mock_read_line,
    .write = mock_write,
    .flush = mock_flush,
    .fullscreen = NULL,
    .splitscreen = NULL,
    .textscreen = NULL};

static LogoDevice mock_device;

// Declare the function to set device in primitives_output.c
extern void primitives_set_device(LogoDevice *device);

void setUp(void)
{
    mem_init();
    primitives_init();
    variables_init();
    output_buffer[0] = '\0';
    output_pos = 0;

    // Set up mock device
    logo_device_init(&mock_device, &mock_ops, NULL);
    primitives_set_device(&mock_device);
}

void tearDown(void)
{
}

// Helper to evaluate and return result
static Result eval_string(const char *input)
{
    Lexer lexer;
    Evaluator eval;
    lexer_init(&lexer, input);
    eval_init(&eval, &lexer);
    return eval_expression(&eval);
}

// Helper to run instructions
static Result run_string(const char *input)
{
    Lexer lexer;
    Evaluator eval;
    lexer_init(&lexer, input);
    eval_init(&eval, &lexer);

    Result r = result_none();
    while (!eval_at_end(&eval))
    {
        r = eval_instruction(&eval);
        if (r.status == RESULT_ERROR)
            break;
    }
    return r;
}

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

void test_eval_sum(void)
{
    Result r = eval_string("sum 3 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, r.value.as.number);
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

void test_eval_quoted_word(void)
{
    Result r = eval_string("\"hello");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("hello", mem_word_ptr(r.value.as.node));
}

void test_make_and_thing(void)
{
    run_string("make \"x 42");
    Result r = eval_string("thing \"x");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, r.value.as.number);
}

void test_dots_variable(void)
{
    run_string("make \"y 100");
    Result r = eval_string(":y");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, r.value.as.number);
}

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

void test_repeat(void)
{
    run_string("repeat 3 [print 1]");
    TEST_ASSERT_EQUAL_STRING("1\n1\n1\n", output_buffer);
}

void test_run_list(void)
{
    run_string("make \"x [print 42]");
    run_string("run :x");
    TEST_ASSERT_EQUAL_STRING("42\n", output_buffer);
}

void test_stop(void)
{
    // stop should return RESULT_STOP
    Result r = eval_string("stop");
    TEST_ASSERT_EQUAL(RESULT_STOP, r.status);
}

void test_output(void)
{
    Result r = eval_string("output 99");
    TEST_ASSERT_EQUAL(RESULT_OUTPUT, r.status);
    TEST_ASSERT_EQUAL_FLOAT(99.0f, r.value.as.number);
}

void test_difference(void)
{
    Result r = eval_string("difference 10 3");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, r.value.as.number);
}

void test_product(void)
{
    Result r = eval_string("product 3 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(12.0f, r.value.as.number);
}

void test_quotient(void)
{
    Result r = eval_string("quotient 20 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, r.value.as.number);
}

void test_divide_by_zero(void)
{
    Result r = eval_string("quotient 10 0");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(13, r.error_code); // ERR_DIVIDE_BY_ZERO
}

void test_empty_list(void)
{
    Result r = eval_string("[]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_list_with_words(void)
{
    Result r = eval_string("[hello world]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_FALSE(mem_is_nil(r.value.as.node));
}

void test_sum_variadic_parens(void)
{
    // (sum 1 2 3 4 5) should add all arguments
    Result r = eval_string("(sum 1 2 3 4 5)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(15.0f, r.value.as.number);
}

void test_product_variadic_parens(void)
{
    // (product 2 3 4) should multiply all arguments
    Result r = eval_string("(product 2 3 4)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(24.0f, r.value.as.number);
}

void test_sum_single_arg_parens(void)
{
    // (sum 5) with just one arg
    Result r = eval_string("(sum 5)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, r.value.as.number);
}

void test_print_variadic_parens(void)
{
    run_string("(print 1 2 3)");
    TEST_ASSERT_EQUAL_STRING("1 2 3\n", output_buffer);
}

//==========================================================================
// Error Message Tests
//==========================================================================

void test_error_sum_doesnt_like(void)
{
    // sum doesn't like hello as input
    Result r = eval_string("sum 1 \"hello");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
    TEST_ASSERT_EQUAL_STRING("sum", r.error_proc);
    TEST_ASSERT_EQUAL_STRING("hello", r.error_arg);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("sum doesn't like hello as input", msg);
}

void test_error_no_value(void)
{
    // x has no value
    Result r = eval_string(":undefined_var");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NO_VALUE, r.error_code);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("undefined_var has no value", msg);
}

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

void test_error_divide_by_zero_msg(void)
{
    // Can't divide by zero
    Result r = eval_string("quotient 5 0");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DIVIDE_BY_ZERO, r.error_code);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("Can't divide by zero", msg);
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

//==========================================================================
// Words and Lists Tests
//==========================================================================

void test_first_number(void)
{
    // print first 12.345 outputs "1"
    run_string("print first 12.345");
    TEST_ASSERT_EQUAL_STRING("1\n", output_buffer);
}

void test_first_word(void)
{
    Result r = eval_string("first \"HOUSE");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("H", mem_word_ptr(r.value.as.node));
}

void test_first_list(void)
{
    Result r = eval_string("first [apple banana cherry]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("apple", mem_word_ptr(r.value.as.node));
}

void test_last_word(void)
{
    Result r = eval_string("last \"HOUSE");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("E", mem_word_ptr(r.value.as.node));
}

void test_butfirst_word(void)
{
    Result r = eval_string("bf \"HOUSE");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("OUSE", mem_word_ptr(r.value.as.node));
}

void test_butlast_word(void)
{
    Result r = eval_string("bl \"HOUSE");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("HOUS", mem_word_ptr(r.value.as.node));
}

void test_count_word(void)
{
    Result r = eval_string("count \"HOUSE");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, r.value.as.number);
}

void test_count_list(void)
{
    Result r = eval_string("count [a b c d]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, r.value.as.number);
}

void test_emptyp_empty_list(void)
{
    Result r = eval_string("emptyp []");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_emptyp_nonempty_list(void)
{
    Result r = eval_string("emptyp [a]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

//==========================================================================
// Variable Scoping Tests
//==========================================================================

void test_global_variable(void)
{
    // Basic global variable test
    run_string("make \"x 42");
    Result r = eval_string(":x");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, r.value.as.number);
}

void test_local_declaration(void)
{
    // At top level, local behaves like creating an unbound variable
    // We can then make it and access it
    run_string("local \"myvar");
    run_string("make \"myvar 100");
    Result r = eval_string(":myvar");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, r.value.as.number);
}

void test_scope_push_pop(void)
{
    // Test that scope management works
    TEST_ASSERT_EQUAL(0, var_scope_depth());
    var_push_scope();
    TEST_ASSERT_EQUAL(1, var_scope_depth());
    var_push_scope();
    TEST_ASSERT_EQUAL(2, var_scope_depth());
    var_pop_scope();
    TEST_ASSERT_EQUAL(1, var_scope_depth());
    var_pop_scope();
    TEST_ASSERT_EQUAL(0, var_scope_depth());
}

void test_local_variable_shadowing(void)
{
    // Create global variable
    run_string("make \"sound \"crash");
    Result r1 = eval_string(":sound");
    TEST_ASSERT_EQUAL(RESULT_OK, r1.status);
    TEST_ASSERT_EQUAL_STRING("crash", mem_word_ptr(r1.value.as.node));
    
    // Push a scope and create local with same name
    var_push_scope();
    var_set_local("sound", value_word(mem_atom("woof", 4)));
    
    // Local should shadow global
    Result r2 = eval_string(":sound");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_STRING("woof", mem_word_ptr(r2.value.as.node));
    
    // Pop scope
    var_pop_scope();
    
    // Global should be restored
    Result r3 = eval_string(":sound");
    TEST_ASSERT_EQUAL(RESULT_OK, r3.status);
    TEST_ASSERT_EQUAL_STRING("crash", mem_word_ptr(r3.value.as.node));
}

void test_local_variable_not_visible_after_scope(void)
{
    // Push scope and create a local-only variable
    var_push_scope();
    var_set_local("tempvar", value_number(999));
    
    // Should be accessible
    Result r1 = eval_string(":tempvar");
    TEST_ASSERT_EQUAL(RESULT_OK, r1.status);
    TEST_ASSERT_EQUAL_FLOAT(999.0f, r1.value.as.number);
    
    // Pop scope
    var_pop_scope();
    
    // Should no longer be accessible
    Result r2 = eval_string(":tempvar");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r2.status);
    TEST_ASSERT_EQUAL(ERR_NO_VALUE, r2.error_code);
}

void test_make_updates_local_in_scope(void)
{
    // Create global
    run_string("make \"x 10");
    
    // Push scope and create local
    var_push_scope();
    var_set_local("x", value_number(20));
    
    // Make should update the local, not global
    run_string("make \"x 30");
    Result r1 = eval_string(":x");
    TEST_ASSERT_EQUAL(RESULT_OK, r1.status);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, r1.value.as.number);
    
    // Pop scope
    var_pop_scope();
    
    // Global should still be 10
    Result r2 = eval_string(":x");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r2.value.as.number);
}

void test_make_creates_global_when_no_local(void)
{
    // Push scope
    var_push_scope();
    
    // Make creates global since no local declared
    run_string("make \"newglobal 42");
    
    // Pop scope
    var_pop_scope();
    
    // Should still be accessible as global
    Result r = eval_string(":newglobal");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, r.value.as.number);
}

void test_local_with_list(void)
{
    // local with list of names
    run_string("local [a b c]");
    run_string("make \"a 1");
    run_string("make \"b 2");
    run_string("make \"c 3");
    
    Result ra = eval_string(":a");
    TEST_ASSERT_EQUAL(RESULT_OK, ra.status);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, ra.value.as.number);
    
    Result rb = eval_string(":b");
    TEST_ASSERT_EQUAL(RESULT_OK, rb.status);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, rb.value.as.number);
    
    Result rc = eval_string(":c");
    TEST_ASSERT_EQUAL(RESULT_OK, rc.status);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, rc.value.as.number);
}

void test_name_primitive(void)
{
    // name is make with reversed arguments
    run_string("name \"pigeon \"bird");
    Result r = eval_string(":bird");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("pigeon", mem_word_ptr(r.value.as.node));
}

void test_namep_true(void)
{
    run_string("make \"testvar 123");
    Result r = eval_string("namep \"testvar");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_namep_false(void)
{
    Result r = eval_string("namep \"nonexistent");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_nested_scopes(void)
{
    // Test multiple levels of nesting
    run_string("make \"level \"global");
    
    var_push_scope();
    var_set_local("level", value_word(mem_atom("scope1", 6)));
    Result r1 = eval_string(":level");
    TEST_ASSERT_EQUAL_STRING("scope1", mem_word_ptr(r1.value.as.node));
    
    var_push_scope();
    var_set_local("level", value_word(mem_atom("scope2", 6)));
    Result r2 = eval_string(":level");
    TEST_ASSERT_EQUAL_STRING("scope2", mem_word_ptr(r2.value.as.node));
    
    var_pop_scope();
    Result r3 = eval_string(":level");
    TEST_ASSERT_EQUAL_STRING("scope1", mem_word_ptr(r3.value.as.node));
    
    var_pop_scope();
    Result r4 = eval_string(":level");
    TEST_ASSERT_EQUAL_STRING("global", mem_word_ptr(r4.value.as.node));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_eval_number);
    RUN_TEST(test_eval_negative_number);
    RUN_TEST(test_eval_sum);
    RUN_TEST(test_eval_infix_add);
    RUN_TEST(test_eval_infix_precedence);
    RUN_TEST(test_eval_parentheses);
    RUN_TEST(test_eval_quoted_word);
    RUN_TEST(test_make_and_thing);
    RUN_TEST(test_dots_variable);
    RUN_TEST(test_print);
    RUN_TEST(test_print_word);
    RUN_TEST(test_repeat);
    RUN_TEST(test_run_list);
    RUN_TEST(test_stop);
    RUN_TEST(test_output);
    RUN_TEST(test_difference);
    RUN_TEST(test_product);
    RUN_TEST(test_quotient);
    RUN_TEST(test_divide_by_zero);
    RUN_TEST(test_empty_list);
    RUN_TEST(test_list_with_words);
    RUN_TEST(test_sum_variadic_parens);
    RUN_TEST(test_product_variadic_parens);
    RUN_TEST(test_sum_single_arg_parens);
    RUN_TEST(test_print_variadic_parens);

    // Error message tests
    RUN_TEST(test_error_sum_doesnt_like);
    RUN_TEST(test_error_no_value);
    RUN_TEST(test_error_dont_know_how);
    RUN_TEST(test_error_not_enough_inputs);
    RUN_TEST(test_error_divide_by_zero_msg);
    RUN_TEST(test_error_infix_doesnt_like);

    // Words and lists tests
    RUN_TEST(test_first_number);
    RUN_TEST(test_first_word);
    RUN_TEST(test_first_list);
    RUN_TEST(test_last_word);
    RUN_TEST(test_butfirst_word);
    RUN_TEST(test_butlast_word);
    RUN_TEST(test_count_word);
    RUN_TEST(test_count_list);
    RUN_TEST(test_emptyp_empty_list);
    RUN_TEST(test_emptyp_nonempty_list);

    // Variable scoping tests
    RUN_TEST(test_global_variable);
    RUN_TEST(test_local_declaration);
    RUN_TEST(test_scope_push_pop);
    RUN_TEST(test_local_variable_shadowing);
    RUN_TEST(test_local_variable_not_visible_after_scope);
    RUN_TEST(test_make_updates_local_in_scope);
    RUN_TEST(test_make_creates_global_when_no_local);
    RUN_TEST(test_local_with_list);
    RUN_TEST(test_name_primitive);
    RUN_TEST(test_namep_true);
    RUN_TEST(test_namep_false);
    RUN_TEST(test_nested_scopes);

    return UNITY_END();
}
