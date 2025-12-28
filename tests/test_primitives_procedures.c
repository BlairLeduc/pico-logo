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
// User-Defined Procedure Tests
//==========================================================================

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
    define_proc("printx", params, 1, "print :x");
    
    run_string("printx 42");
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
    
    reset_output();
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

void test_defined_question_alias(void)
{
    // defined? is the canonical name for definedp
    const char *params[] = {};
    define_proc("testproc", params, 0, "print 1");
    
    Result r1 = eval_string("defined? \"testproc");
    TEST_ASSERT_EQUAL(RESULT_OK, r1.status);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r1.value.as.node));
    
    Result r2 = eval_string("defined? \"undefined");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r2.value.as.node));
}

void test_primitive_question_alias(void)
{
    // primitive? is the canonical name for primitivep
    Result r1 = eval_string("primitive? \"sum");
    TEST_ASSERT_EQUAL(RESULT_OK, r1.status);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r1.value.as.node));
    
    Result r2 = eval_string("primitive? \"notaprimitive");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r2.value.as.node));
}

void test_copydef_copies_procedure(void)
{
    // Define a procedure
    Node p = mem_atom("x", 1);
    const char *params[] = {mem_word_ptr(p)};
    define_proc("double", params, 1, "output :x * 2");
    
    // Copy it to a new name
    run_string("copydef \"double \"twice");
    
    // Both should work
    Result r1 = eval_string("double 5");
    TEST_ASSERT_EQUAL(RESULT_OK, r1.status);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r1.value.as.number);
    
    Result r2 = eval_string("twice 7");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_FLOAT(14.0f, r2.value.as.number);
}

void test_copydef_error_source_not_found(void)
{
    Result r = run_string("copydef \"nonexistent \"newname");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DONT_KNOW_HOW, r.error_code);
}

void test_copydef_copies_primitive(void)
{
    // Copy a primitive to a new name
    run_string("copydef \"forward \"f");
    
    // The alias should work
    Result r = eval_string("primitive? \"f");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_copydef_error_dest_is_primitive(void)
{
    const char *params[] = {};
    define_proc("myproc", params, 0, "print 1");
    
    Result r = run_string("copydef \"myproc \"print");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_IS_PRIMITIVE, r.error_code);
}

void test_text_outputs_procedure_definition(void)
{
    // Define a procedure
    Node p = mem_atom("x", 1);
    const char *params[] = {mem_word_ptr(p)};
    define_proc("square", params, 1, "output :x * :x");
    
    Result r = eval_string("text \"square");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    // The result should be [[x] [output :x * :x]] or similar
    Node list = r.value.as.node;
    TEST_ASSERT_FALSE(mem_is_nil(list));
}

void test_text_error_not_found(void)
{
    Result r = eval_string("text \"undefined");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DONT_KNOW_HOW, r.error_code);
}

//==========================================================================
// DEFINE primitive tests
//==========================================================================

void test_define_simple_procedure(void)
{
    // define "name [[params] [body]]
    // Define a procedure with no params: define "hello [[] [print "hi]]
    Result r = run_string("define \"hello [[] [print \"hi]]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Now call it
    reset_output();
    run_string("hello");
    TEST_ASSERT_EQUAL_STRING("hi\n", output_buffer);
}

void test_define_procedure_with_params(void)
{
    // define "double [[x] [output :x * 2]]
    Result r = run_string("define \"double [[x] [output :x * 2]]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Now call it
    Result r2 = eval_string("double 5");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r2.value.as.number);
}

void test_define_procedure_multiple_params(void)
{
    // define "add3 [[a b c] [output :a + :b + :c]]
    Result r = run_string("define \"add3 [[a b c] [output :a + :b + :c]]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Now call it
    Result r2 = eval_string("add3 1 2 3");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_FLOAT(6.0f, r2.value.as.number);
}

void test_define_error_name_not_word(void)
{
    // First arg must be a word
    Result r = run_string("define [notaword] [[] [print 1]]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_define_error_def_not_list(void)
{
    // Second arg must be a list
    Result r = run_string("define \"myproc \"notalist");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_define_error_redefine_primitive(void)
{
    // Cannot redefine primitives
    Result r = run_string("define \"print [[] [print 1]]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_IS_PRIMITIVE, r.error_code);
}

void test_define_error_empty_definition_list(void)
{
    // Empty definition list
    Result r = run_string("define \"myproc []");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_TOO_FEW_ITEMS, r.error_code);
}

//==========================================================================
// Additional error path tests
//==========================================================================

void test_primitivep_error_not_word(void)
{
    Result r = eval_string("primitivep [notaword]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_definedp_error_not_word(void)
{
    Result r = eval_string("definedp [notaword]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_text_error_not_word(void)
{
    Result r = eval_string("text [notaword]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_copydef_error_source_not_word(void)
{
    Result r = run_string("copydef [notaword] \"newname");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_copydef_error_dest_not_word(void)
{
    const char *params[] = {};
    define_proc("myproc", params, 0, "print 1");
    
    Result r = run_string("copydef \"myproc [notaword]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

//==========================================================================
// TEXT primitive detailed tests
//==========================================================================

void test_text_with_params(void)
{
    // Define a procedure with parameters
    Node p1 = mem_atom("x", 1);
    Node p2 = mem_atom("y", 1);
    const char *params[] = {mem_word_ptr(p1), mem_word_ptr(p2)};
    define_proc("addxy", params, 2, "output :x + :y");
    
    Result r = eval_string("text \"addxy");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    // First element should be parameter list
    Node list = r.value.as.node;
    Node params_list = mem_car(list);
    // params_list is marked as a list type
    TEST_ASSERT_EQUAL(NODE_TYPE_LIST, NODE_GET_TYPE(params_list));
}

void test_text_no_params(void)
{
    // Define a procedure with no parameters
    const char *params[] = {};
    define_proc("noparam", params, 0, "print \"hello");
    
    Result r = eval_string("text \"noparam");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
}

//==========================================================================
// proc_define_from_text tests
//==========================================================================

void test_proc_define_from_text_simple(void)
{
    // Define using text format with newline markers: to name \n body \n end
    Result r = proc_define_from_text("to greetings \\n print \"hello \\n end");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    // Verify the procedure was defined
    TEST_ASSERT_TRUE(proc_exists("greetings"));
    
    // Run it
    reset_output();
    run_string("greetings");
    TEST_ASSERT_EQUAL_STRING("hello\n", output_buffer);
}

void test_proc_define_from_text_with_param(void)
{
    // Define a procedure with parameters and newline markers
    Result r = proc_define_from_text("to triple :n \\n output :n * 3 \\n end");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = eval_string("triple 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_FLOAT(12.0f, r2.value.as.number);
}

void test_proc_define_from_text_multiple_params(void)
{
    Result r = proc_define_from_text("to avg :a :b \\n output (:a + :b) / 2 \\n end");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = eval_string("avg 10 20");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_FLOAT(15.0f, r2.value.as.number);
}

void test_proc_define_from_text_with_brackets(void)
{
    // Test with brackets in the body and newline markers
    Result r = proc_define_from_text("to countdown :n \\n if :n > 0 [print :n countdown :n - 1] \\n end");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    reset_output();
    run_string("countdown 3");
    TEST_ASSERT_EQUAL_STRING("3\n2\n1\n", output_buffer);
}

void test_proc_define_from_text_with_comparison(void)
{
    // Test with comparison operators and newline markers
    Result r = proc_define_from_text("to bigger :a :b \\n if :a > :b [output :a] \\n output :b \\n end");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = eval_string("bigger 5 3");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, r2.value.as.number);
    
    Result r3 = eval_string("bigger 2 7");
    TEST_ASSERT_EQUAL(RESULT_OK, r3.status);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, r3.value.as.number);
}

void test_proc_define_from_text_error_not_to(void)
{
    // First token should be a word (the "to" keyword, though value isn't validated)
    // Pass a number first to trigger the error
    Result r = proc_define_from_text("123 myproc print 1 end");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_proc_define_from_text_error_no_name(void)
{
    // Missing procedure name - only "to"
    Result r = proc_define_from_text("to");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NOT_ENOUGH_INPUTS, r.error_code);
}

void test_proc_define_from_text_error_redefine_primitive(void)
{
    // Cannot redefine primitives
    Result r = proc_define_from_text("to print :x \\n output :x \\n end");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_IS_PRIMITIVE, r.error_code);
}

void test_proc_define_from_text_quoted_word(void)
{
    // Test with quoted words in body and newline markers
    Result r = proc_define_from_text("to sayhello \\n print \"hello \\n end");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    reset_output();
    run_string("sayhello");
    TEST_ASSERT_EQUAL_STRING("hello\n", output_buffer);
}

void test_proc_define_from_text_all_operators(void)
{
    // Test all arithmetic and comparison operators with newline markers
    Result r = proc_define_from_text("to mathtest :x \\n output :x + 1 - 1 * 2 / 2 \\n end");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = eval_string("mathtest 10");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    // Due to operator precedence: 10 + 1 - 1 * 2 / 2 = 10 + 1 - 1 = 10
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r2.value.as.number);
}

void test_proc_define_from_text_equals_operator(void)
{
    // Test equals operator with newline markers
    Result r = proc_define_from_text("to iseq :a :b \\n if :a = :b [output \"yes] \\n output \"no \\n end");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = eval_string("iseq 5 5");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_STRING("yes", mem_word_ptr(r2.value.as.node));
    
    Result r3 = eval_string("iseq 3 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r3.status);
    TEST_ASSERT_EQUAL_STRING("no", mem_word_ptr(r3.value.as.node));
}
void test_proc_define_from_text_end_in_list(void)
{
    // Bug: [end] in procedure body was incorrectly terminating the procedure
    // Test that [end] as a list element doesn't end the procedure
    Result r = proc_define_from_text("to checkend :x \\n if :x = [end] [output \"yes] \\n output \"no \\n end");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    // Verify the procedure works correctly
    Result r2 = eval_string("checkend [end]");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_STRING("yes", mem_word_ptr(r2.value.as.node));
    
    Result r3 = eval_string("checkend [other]");
    TEST_ASSERT_EQUAL(RESULT_OK, r3.status);
    TEST_ASSERT_EQUAL_STRING("no", mem_word_ptr(r3.value.as.node));
}

void test_proc_define_from_text_less_than_operator(void)
{
    // Test less than operator with newline markers
    Result r = proc_define_from_text("to isless :a :b \\n if :a < :b [output \"yes] \\n output \"no \\n end");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = eval_string("isless 3 5");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_STRING("yes", mem_word_ptr(r2.value.as.node));
}

void test_proc_define_from_text_with_parentheses(void)
{
    // Test with parentheses in body and newline markers
    Result r = proc_define_from_text("to sumall :a :b :c \\n output (:a + :b + :c) \\n end");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = eval_string("sumall 1 2 3");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_FLOAT(6.0f, r2.value.as.number);
}

void test_unconditional_tco_no_args(void)
{
    // Test unconditional infinite recursion with no args - should use TCO
    // This tests that `to foo pr "Hey foo end` doesn't overflow
    // We use a counter to stop after enough iterations
    run_string("make \"counter 0");
    
    Result r = proc_define_from_text("to infloop \\n make \"counter :counter + 1 \\n "
                                     "if :counter > 100 [stop] \\n "
                                     "infloop \\n end");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = run_string("infloop");
    TEST_ASSERT_EQUAL(RESULT_NONE, r2.status);  // Should complete successfully
    
    Result r3 = eval_string(":counter");
    TEST_ASSERT_EQUAL(VALUE_NUMBER, r3.value.type);
    TEST_ASSERT_EQUAL_FLOAT(101.0f, r3.value.as.number);
}

void test_unconditional_tco_with_args(void)
{
    // Test unconditional infinite recursion WITH args - the bug case
    // This is the case that crashes: `to foo2 :n pr se "Hey :n foo2 :n end`
    // We use a counter to stop after enough iterations
    run_string("make \"counter 0");
    
    Result r = proc_define_from_text("to infloop2 :n \\n make \"counter :counter + 1 \\n "
                                     "if :counter > 100 [stop] \\n "
                                     "infloop2 :n \\n end");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = run_string("infloop2 42");
    TEST_ASSERT_EQUAL(RESULT_NONE, r2.status);  // Should complete successfully with TCO
    
    Result r3 = eval_string(":counter");
    TEST_ASSERT_EQUAL(VALUE_NUMBER, r3.value.type);
    TEST_ASSERT_EQUAL_FLOAT(101.0f, r3.value.as.number);
}

void test_tco_with_args_exact_failing_case(void)
{
    // This is the EXACT failing case from the user:
    // to foo2 :n
    //   pr se "Hey :n
    //   foo2 :n
    // end
    // The key difference is the recursive call uses the argument
    // Let's verify it works with > 32 recursions (scope limit)
    run_string("make \"loopcount 0");
    
    // Define procedure that uses its argument in recursive call
    Result r = proc_define_from_text(
        "to foo2 :n \\n "
        "make \"loopcount :loopcount + 1 \\n "
        "if :loopcount > 100 [stop] \\n "
        "foo2 :n \\n "
        "end");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = run_string("foo2 5");
    TEST_ASSERT_EQUAL(RESULT_NONE, r2.status);
    
    Result r3 = eval_string(":loopcount");
    TEST_ASSERT_EQUAL(VALUE_NUMBER, r3.value.type);
    TEST_ASSERT_EQUAL_FLOAT(101.0f, r3.value.as.number);  // Should get to 101 with TCO
}

void test_tco_with_print_and_args(void)
{
    // Test the EXACT case that fails:
    // to foo2 :n
    //   pr se "Hey :n
    //   foo2 :n
    // end
    // With a counter to terminate
    run_string("make \"cnt 0");
    
    reset_output();
    
    Result r = proc_define_from_text(
        "to fooprint :n \\n "
        "make \"cnt :cnt + 1 \\n "
        "if :cnt > 50 [stop] \\n "
        "pr (se \"Hey :n) \\n "
        "fooprint :n \\n "
        "end");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = run_string("fooprint 42");
    TEST_ASSERT_EQUAL(RESULT_NONE, r2.status);  // Should complete with TCO
    
    Result r3 = eval_string(":cnt");
    TEST_ASSERT_EQUAL_FLOAT(51.0f, r3.value.as.number);  // Should reach 51
}

void test_tco_scope_depth_stability(void)
{
    // Verify that scope depth stays at 1 throughout TCO execution
    // This ensures TCO is working and not accumulating scopes
    run_string("make \"maxdepth 0");
    run_string("make \"cnt 0");
    
    Result r = proc_define_from_text(
        "to checkdepth :n \\n "
        "make \"cnt :cnt + 1 \\n "
        "if :cnt > 100 [stop] \\n "
        "checkdepth :n \\n "
        "end");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    // Track scope depth before
    int depth_before = var_scope_depth();
    
    Result r2 = run_string("checkdepth 5");
    TEST_ASSERT_EQUAL(RESULT_NONE, r2.status);
    
    // Scope depth should be back to same level after
    int depth_after = var_scope_depth();
    TEST_ASSERT_EQUAL(depth_before, depth_after);
    
    // Should have completed 101 iterations without overflow
    Result r3 = eval_string(":cnt");
    TEST_ASSERT_EQUAL_FLOAT(101.0f, r3.value.as.number);
}

int main(void)
{
    UNITY_BEGIN();

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
    RUN_TEST(test_defined_question_alias);
    RUN_TEST(test_primitive_question_alias);
    RUN_TEST(test_copydef_copies_procedure);
    RUN_TEST(test_copydef_copies_primitive);
    RUN_TEST(test_copydef_error_source_not_found);
    RUN_TEST(test_copydef_error_dest_is_primitive);
    RUN_TEST(test_text_outputs_procedure_definition);
    RUN_TEST(test_text_error_not_found);
    
    // DEFINE primitive tests
    RUN_TEST(test_define_simple_procedure);
    RUN_TEST(test_define_procedure_with_params);
    RUN_TEST(test_define_procedure_multiple_params);
    RUN_TEST(test_define_error_name_not_word);
    RUN_TEST(test_define_error_def_not_list);
    RUN_TEST(test_define_error_redefine_primitive);
    RUN_TEST(test_define_error_empty_definition_list);
    
    // Additional error path tests
    RUN_TEST(test_primitivep_error_not_word);
    RUN_TEST(test_definedp_error_not_word);
    RUN_TEST(test_text_error_not_word);
    RUN_TEST(test_copydef_error_source_not_word);
    RUN_TEST(test_copydef_error_dest_not_word);
    
    // TEXT primitive detailed tests
    RUN_TEST(test_text_with_params);
    RUN_TEST(test_text_no_params);
    
    // proc_define_from_text tests
    RUN_TEST(test_proc_define_from_text_simple);
    RUN_TEST(test_proc_define_from_text_with_param);
    RUN_TEST(test_proc_define_from_text_multiple_params);
    RUN_TEST(test_proc_define_from_text_with_brackets);
    RUN_TEST(test_proc_define_from_text_with_comparison);
    RUN_TEST(test_proc_define_from_text_error_not_to);
    RUN_TEST(test_proc_define_from_text_error_no_name);
    RUN_TEST(test_proc_define_from_text_error_redefine_primitive);
    RUN_TEST(test_proc_define_from_text_quoted_word);
    RUN_TEST(test_proc_define_from_text_all_operators);
    RUN_TEST(test_proc_define_from_text_equals_operator);
    RUN_TEST(test_proc_define_from_text_less_than_operator);
    RUN_TEST(test_proc_define_from_text_end_in_list);
    RUN_TEST(test_proc_define_from_text_with_parentheses);
    RUN_TEST(test_unconditional_tco_no_args);
    RUN_TEST(test_unconditional_tco_with_args);
    RUN_TEST(test_tco_with_args_exact_failing_case);
    RUN_TEST(test_tco_with_print_and_args);
    RUN_TEST(test_tco_scope_depth_stability);

    return UNITY_END();
}
