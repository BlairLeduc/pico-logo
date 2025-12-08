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
#include "core/procedures.h"
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
    procedures_init();
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

// Test infix subtraction inside lists - Logo evaluates infix operators when list is run
void test_infix_minus_in_list(void)
{
    // First test: basic infix minus after variable reference
    // :x - 1 should be evaluated as infix subtraction (space after -)
    run_string("make \"x 3");
    output_buffer[0] = '\0';
    output_pos = 0;
    run_string("print :x - 1");
    // Should print 2 (3 - 1)
    TEST_ASSERT_EQUAL_STRING("2\n", output_buffer);
    
    // Second test: inside a repeat list
    output_buffer[0] = '\0';
    output_pos = 0;
    run_string("repeat 2 [print sum 1 :x - 1]");
    // sum 1 (:x - 1) = sum 1 2 = 3, printed twice
    TEST_ASSERT_EQUAL_STRING("3\n3\n", output_buffer);
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

//==========================================================================
// User-Defined Procedure Tests
//==========================================================================

// Helper to define a procedure using define primitive
static void define_proc(const char *name, const char **params, int param_count, const char *body_str)
{
    // Build body list from string
    Lexer lexer;
    lexer_init(&lexer, body_str);
    
    Node body = NODE_NIL;
    Node body_tail = NODE_NIL;
    
    while (true)
    {
        Token t = lexer_next_token(&lexer);
        if (t.type == TOKEN_EOF)
            break;
        
        Node item = NODE_NIL;
        if (t.type == TOKEN_WORD || t.type == TOKEN_NUMBER)
        {
            item = mem_atom(t.start, t.length);
        }
        else if (t.type == TOKEN_QUOTED)
        {
            item = mem_atom(t.start, t.length);
        }
        else if (t.type == TOKEN_COLON)
        {
            item = mem_atom(t.start, t.length);
        }
        else if (t.type == TOKEN_PLUS)
        {
            item = mem_atom("+", 1);
        }
        else if (t.type == TOKEN_MINUS)
        {
            item = mem_atom("-", 1);
        }
        else if (t.type == TOKEN_MULTIPLY)
        {
            item = mem_atom("*", 1);
        }
        else if (t.type == TOKEN_DIVIDE)
        {
            item = mem_atom("/", 1);
        }
        else if (t.type == TOKEN_EQUALS)
        {
            item = mem_atom("=", 1);
        }
        else if (t.type == TOKEN_LESS_THAN)
        {
            item = mem_atom("<", 1);
        }
        else if (t.type == TOKEN_GREATER_THAN)
        {
            item = mem_atom(">", 1);
        }
        else if (t.type == TOKEN_LEFT_BRACKET)
        {
            // Parse nested list
            item = mem_atom("[", 1);
        }
        else if (t.type == TOKEN_RIGHT_BRACKET)
        {
            item = mem_atom("]", 1);
        }
        
        if (!mem_is_nil(item))
        {
            Node new_cons = mem_cons(item, NODE_NIL);
            if (mem_is_nil(body))
            {
                body = new_cons;
                body_tail = new_cons;
            }
            else
            {
                mem_set_cdr(body_tail, new_cons);
                body_tail = new_cons;
            }
        }
    }
    
    // Intern the name
    Node name_atom = mem_atom(name, strlen(name));
    const char *interned_name = mem_word_ptr(name_atom);
    
    proc_define(interned_name, params, param_count, body);
}

void test_simple_procedure_no_args(void)
{
    // Define a simple procedure with no arguments
    const char *params[] = {};
    define_proc("greet", params, 0, "print \"hello");
    
    run_string("greet");
    TEST_ASSERT_EQUAL_STRING("hello\n", output_buffer);
}

void test_procedure_with_one_arg(void)
{
    // Define a procedure with one argument
    const char *param = "x";
    const char *params[] = {param};
    
    // Intern the parameter name
    Node param_atom = mem_atom(param, strlen(param));
    const char *interned_param = mem_word_ptr(param_atom);
    const char *interned_params[] = {interned_param};
    
    define_proc("double", interned_params, 1, "output :x * 2");
    
    Result r = eval_string("double 5");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.value.as.number);
}

void test_procedure_with_two_args(void)
{
    // Define a procedure with two arguments
    Node p1 = mem_atom("a", 1);
    Node p2 = mem_atom("b", 1);
    const char *params[] = {mem_word_ptr(p1), mem_word_ptr(p2)};
    
    define_proc("add", params, 2, "output :a + :b");
    
    Result r = eval_string("add 3 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, r.value.as.number);
}

void test_procedure_local_scope(void)
{
    // Procedure arguments should be local
    run_string("make \"x 100");
    
    Node p = mem_atom("x", 1);
    const char *params[] = {mem_word_ptr(p)};
    define_proc("setx", params, 1, "print :x");
    
    run_string("setx 42");
    TEST_ASSERT_EQUAL_STRING("42\n", output_buffer);
    
    // Global x should be unchanged
    Result r = eval_string(":x");
    TEST_ASSERT_EQUAL_FLOAT(100.0f, r.value.as.number);
}

void test_procedure_modifies_global(void)
{
    // Procedure can modify global variables
    run_string("make \"count 0");
    
    const char *params[] = {};
    define_proc("inc", params, 0, "make \"count :count + 1");
    
    run_string("inc");
    run_string("inc");
    run_string("inc");
    
    Result r = eval_string(":count");
    TEST_ASSERT_EQUAL_FLOAT(3.0f, r.value.as.number);
}

void test_recursive_procedure(void)
{
    // Simple recursive procedure - sumto adds up numbers from 1 to n
    // Using a global accumulator
    // sumto n: make "result :result + :n if :n > 0 [sumto difference :n 1]
    Node p = mem_atom("n", 1);
    const char *params[] = {mem_word_ptr(p)};
    
    run_string("make \"result 0");
    
    // Build body: make "result :result + :n if :n > 0 [sumto difference :n 1]
    Node body = NODE_NIL;
    Node tail = NODE_NIL;
    
    // Build: make "result :result + :n
    const char *words1[] = {"make", "\"result", ":result", "+", ":n"};
    for (int i = 0; i < 5; i++) {
        Node w = mem_atom(words1[i], strlen(words1[i]));
        Node c = mem_cons(w, NODE_NIL);
        if (mem_is_nil(body)) { body = c; tail = c; }
        else { mem_set_cdr(tail, c); tail = c; }
    }
    
    // Build: if :n > 0 [sumto difference :n 1]
    const char *words2[] = {"if", ":n", ">", "0"};
    for (int i = 0; i < 4; i++) {
        Node w = mem_atom(words2[i], strlen(words2[i]));
        Node c = mem_cons(w, NODE_NIL);
        mem_set_cdr(tail, c); tail = c;
    }
    
    // Build the nested list [sumto difference :n 1]
    const char *inner_words[] = {"sumto", "difference", ":n", "1"};
    Node inner = NODE_NIL;
    Node inner_tail = NODE_NIL;
    for (int i = 0; i < 4; i++) {
        Node w = mem_atom(inner_words[i], strlen(inner_words[i]));
        Node c = mem_cons(w, NODE_NIL);
        if (mem_is_nil(inner)) { inner = c; inner_tail = c; }
        else { mem_set_cdr(inner_tail, c); inner_tail = c; }
    }
    // Add nested list to body - inner is already a NODE_TYPE_LIST from mem_cons
    Node c = mem_cons(inner, NODE_NIL);
    mem_set_cdr(tail, c);
    
    // Define the procedure
    Node name = mem_atom("sumto", 5);
    proc_define(mem_word_ptr(name), params, 1, body);
    
    run_string("sumto 5");
    
    Result r = eval_string(":result");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(15.0f, r.value.as.number);  // 1+2+3+4+5 = 15
}

void test_tail_recursive_countdown(void)
{
    // Define a tail-recursive countdown procedure
    // countdown n: if :n > 0 [print :n countdown difference :n 1]
    Node p = mem_atom("n", 1);
    const char *params[] = {mem_word_ptr(p)};
    
    // Build body: if :n > 0 [print :n countdown difference :n 1]
    Node body = NODE_NIL;
    Node tail = NODE_NIL;
    
    // if :n > 0
    const char *words[] = {"if", ":n", ">", "0"};
    for (int i = 0; i < 4; i++) {
        Node w = mem_atom(words[i], strlen(words[i]));
        Node c = mem_cons(w, NODE_NIL);
        if (mem_is_nil(body)) { body = c; tail = c; }
        else { mem_set_cdr(tail, c); tail = c; }
    }
    
    // Build nested list [print :n countdown difference :n 1]
    const char *inner_words[] = {"print", ":n", "countdown", "difference", ":n", "1"};
    Node inner = NODE_NIL;
    Node inner_tail = NODE_NIL;
    for (int i = 0; i < 6; i++) {
        Node w = mem_atom(inner_words[i], strlen(inner_words[i]));
        Node c = mem_cons(w, NODE_NIL);
        if (mem_is_nil(inner)) { inner = c; inner_tail = c; }
        else { mem_set_cdr(inner_tail, c); inner_tail = c; }
    }
    // inner is already a NODE_TYPE_LIST from mem_cons
    Node c = mem_cons(inner, NODE_NIL);
    mem_set_cdr(tail, c);
    
    Node name = mem_atom("countdown", 9);
    proc_define(mem_word_ptr(name), params, 1, body);
    
    output_buffer[0] = '\0';
    output_pos = 0;
    run_string("countdown 3");
    TEST_ASSERT_EQUAL_STRING("3\n2\n1\n", output_buffer);
}

void test_deep_tail_recursion(void)
{
    // Test deep tail recursion with a counter
    // tailcount n: if :n > 0 [tailcount difference :n 1]
    Node p = mem_atom("n", 1);
    const char *params[] = {mem_word_ptr(p)};
    
    Node body = NODE_NIL;
    Node tail = NODE_NIL;
    
    // if :n > 0
    const char *words[] = {"if", ":n", ">", "0"};
    for (int i = 0; i < 4; i++) {
        Node w = mem_atom(words[i], strlen(words[i]));
        Node c = mem_cons(w, NODE_NIL);
        if (mem_is_nil(body)) { body = c; tail = c; }
        else { mem_set_cdr(tail, c); tail = c; }
    }
    
    // [tailcount difference :n 1]
    const char *inner_words[] = {"tailcount", "difference", ":n", "1"};
    Node inner = NODE_NIL;
    Node inner_tail = NODE_NIL;
    for (int i = 0; i < 4; i++) {
        Node w = mem_atom(inner_words[i], strlen(inner_words[i]));
        Node c = mem_cons(w, NODE_NIL);
        if (mem_is_nil(inner)) { inner = c; inner_tail = c; }
        else { mem_set_cdr(inner_tail, c); inner_tail = c; }
    }
    // inner is already a NODE_TYPE_LIST from mem_cons
    Node c = mem_cons(inner, NODE_NIL);
    mem_set_cdr(tail, c);
    
    Node name = mem_atom("tailcount", 9);
    proc_define(mem_word_ptr(name), params, 1, body);
    
    // With TCO, 100 recursive calls should work (without it, would overflow 32 scope levels)
    Result r = run_string("tailcount 100");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);  // No error = TCO is working
}

void test_definedp_true(void)
{
    const char *params[] = {};
    define_proc("myproc", params, 0, "print 1");
    
    Result r = eval_string("definedp \"myproc");
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_definedp_false(void)
{
    Result r = eval_string("definedp \"notdefined");
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_primitivep_true(void)
{
    Result r = eval_string("primitivep \"print");
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_primitivep_false(void)
{
    Result r = eval_string("primitivep \"notaprimitive");
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
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
    RUN_TEST(test_infix_minus_in_list);
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

    // User-defined procedure tests
    RUN_TEST(test_simple_procedure_no_args);
    RUN_TEST(test_procedure_with_one_arg);
    RUN_TEST(test_procedure_with_two_args);
    RUN_TEST(test_procedure_local_scope);
    RUN_TEST(test_procedure_modifies_global);
    RUN_TEST(test_recursive_procedure);
    RUN_TEST(test_tail_recursive_countdown);
    RUN_TEST(test_deep_tail_recursion);
    RUN_TEST(test_definedp_true);
    RUN_TEST(test_definedp_false);
    RUN_TEST(test_primitivep_true);
    RUN_TEST(test_primitivep_false);

    return UNITY_END();
}
