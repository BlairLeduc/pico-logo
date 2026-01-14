//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "test_scaffold.h"
#include "core/format.h"
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

// Helper to check frame stack during execution
static int debug_frame_depth = 0;
static const char *debug_frame_binding_names[32];
static int debug_frame_binding_count = 0;

void test_subprocedure_sees_superprocedure_inputs(void)
{
    // Simplest possible test: outer takes :x, inner reads :x
    // inner should see :x in outer's frame (dynamic scoping)
    
    run_string("define \"outer [[x] [inner]]");
    run_string("define \"inner [[] [print :x]]");
    
    // Verify both procedures exist
    Result r_outer_def = eval_string("defined? \"outer");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r_outer_def.status, "outer should be defined");
    TEST_ASSERT_TRUE_MESSAGE(r_outer_def.value.type == VALUE_WORD, "defined? should return word");
    
    Result r_inner_def = eval_string("defined? \"inner");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r_inner_def.status, "inner should be defined");
    
    // Check text of outer
    reset_output();
    run_string("show text \"outer");
    printf("outer text: %s\n", output_buffer);
    
    reset_output();
    run_string("show text \"inner");
    printf("inner text: %s\n", output_buffer);
    
    reset_output();
    Result r = run_string("outer 42");
    
    if (r.status == RESULT_ERROR) {
        char msg[256];
        snprintf(msg, sizeof(msg), "outer failed with error %d: %s %s", 
                 r.error_code, 
                 r.error_proc ? r.error_proc : "(nil)",
                 r.error_arg ? r.error_arg : "(nil)");
        TEST_FAIL_MESSAGE(msg);
    }
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_NONE, r.status, "outer should complete without error");
    TEST_ASSERT_EQUAL_STRING("42\n", output_buffer);
}

void test_subprocedure_sees_superprocedure_locals(void)
{
    // Define a helper procedure that accesses a local variable from caller's scope
    // inner2: print :y
    const char *inner_params[] = {};
    define_proc("inner2", inner_params, 0, "print :y");
    
    // Define outer2 procedure that declares local :y and calls inner2
    // outer2: local "y make "y 99 inner2
    const char *outer_params[] = {};
    define_proc("outer2", outer_params, 0, "local \"y make \"y 99 inner2");
    
    // Call outer2 - inner2 should see :y from outer2's scope
    reset_output();
    Result r = run_string("outer2");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_NONE, r.status, "outer2 should complete without error");
    TEST_ASSERT_EQUAL_STRING("99\n", output_buffer);
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
    
    // Build body line: make "result :result + :n if :n > 0 [sumto difference :n 1]
    Node body_line = NODE_NIL;
    Node tail = NODE_NIL;
    
    // Build: make "result :result + :n
    const char *words1[] = {"make", "\"result", ":result", "+", ":n"};
    for (int i = 0; i < 5; i++) {
        Node w = mem_atom(words1[i], strlen(words1[i]));
        Node c = mem_cons(w, NODE_NIL);
        if (mem_is_nil(body_line)) { body_line = c; tail = c; }
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
    // Add nested list to body_line - inner is already a NODE_TYPE_LIST from mem_cons
    Node c = mem_cons(inner, NODE_NIL);
    mem_set_cdr(tail, c);
    
    // Wrap body_line into list-of-lists: [[body_line]]
    Node line_marked = NODE_MAKE_LIST(NODE_GET_INDEX(body_line));
    Node body = mem_cons(line_marked, NODE_NIL);
    
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
    
    // Build body line: if :n > 0 [print :n countdown difference :n 1]
    Node body_line = NODE_NIL;
    Node tail = NODE_NIL;
    
    // if :n > 0
    const char *words[] = {"if", ":n", ">", "0"};
    for (int i = 0; i < 4; i++) {
        Node w = mem_atom(words[i], strlen(words[i]));
        Node c = mem_cons(w, NODE_NIL);
        if (mem_is_nil(body_line)) { body_line = c; tail = c; }
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
    
    // Wrap body_line into list-of-lists: [[body_line]]
    Node line_marked = NODE_MAKE_LIST(NODE_GET_INDEX(body_line));
    Node body = mem_cons(line_marked, NODE_NIL);
    
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
    
    Node body_line = NODE_NIL;
    Node tail = NODE_NIL;
    
    // if :n > 0
    const char *words[] = {"if", ":n", ">", "0"};
    for (int i = 0; i < 4; i++) {
        Node w = mem_atom(words[i], strlen(words[i]));
        Node c = mem_cons(w, NODE_NIL);
        if (mem_is_nil(body_line)) { body_line = c; tail = c; }
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
    
    // Wrap body_line into list-of-lists: [[body_line]]
    Node line_marked = NODE_MAKE_LIST(NODE_GET_INDEX(body_line));
    Node body = mem_cons(line_marked, NODE_NIL);
    
    Node name = mem_atom("tailcount", 9);
    proc_define(mem_word_ptr(name), params, 1, body);
    
    // With TCO, 100 recursive calls should work (without it, would overflow 32 scope levels)
    Result r = run_string("tailcount 100");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_very_deep_tail_recursion(void)
{
    // Test very deep tail recursion - 10000 calls
    // This validates that TCO with frame_reuse truly prevents stack/memory growth
    // tailcount10k n: if :n > 0 [tailcount10k difference :n 1]
    Node p = mem_atom("n", 1);
    const char *params[] = {mem_word_ptr(p)};
    
    Node body_line = NODE_NIL;
    Node tail = NODE_NIL;
    
    // if :n > 0
    const char *words[] = {"if", ":n", ">", "0"};
    for (int i = 0; i < 4; i++) {
        Node w = mem_atom(words[i], strlen(words[i]));
        Node c = mem_cons(w, NODE_NIL);
        if (mem_is_nil(body_line)) { body_line = c; tail = c; }
        else { mem_set_cdr(tail, c); tail = c; }
    }
    
    // [tailcount10k difference :n 1]
    const char *inner_words[] = {"tailcount10k", "difference", ":n", "1"};
    Node inner = NODE_NIL;
    Node inner_tail = NODE_NIL;
    for (int i = 0; i < 4; i++) {
        Node w = mem_atom(inner_words[i], strlen(inner_words[i]));
        Node c = mem_cons(w, NODE_NIL);
        if (mem_is_nil(inner)) { inner = c; inner_tail = c; }
        else { mem_set_cdr(inner_tail, c); inner_tail = c; }
    }
    Node c = mem_cons(inner, NODE_NIL);
    mem_set_cdr(tail, c);
    
    Node line_marked = NODE_MAKE_LIST(NODE_GET_INDEX(body_line));
    Node body = mem_cons(line_marked, NODE_NIL);
    
    Node name = mem_atom("tailcount10k", 12);
    proc_define(mem_word_ptr(name), params, 1, body);
    
    // With TCO and frame_reuse, 10000 recursive calls should work efficiently
    Result r = run_string("tailcount10k 10000");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_deep_non_tail_recursion_limit(void)
{
    // Test that non-tail recursion works for reasonable depths
    // Note: With CPS disabled inside primitives (like 'if'), deep non-tail
    // recursion uses C stack. The actual limit is platform-dependent.
    // This test verifies reasonable depths work correctly.
    // 
    // deeprec n: if :n > 0 [deeprec difference :n 1 print :n]
    Node p = mem_atom("n", 1);
    const char *params[] = {mem_word_ptr(p)};
    
    Node body_line = NODE_NIL;
    Node tail = NODE_NIL;
    
    // if :n > 0
    const char *words[] = {"if", ":n", ">", "0"};
    for (int i = 0; i < 4; i++) {
        Node w = mem_atom(words[i], strlen(words[i]));
        Node c = mem_cons(w, NODE_NIL);
        if (mem_is_nil(body_line)) { body_line = c; tail = c; }
        else { mem_set_cdr(tail, c); tail = c; }
    }
    
    // [deeprec difference :n 1 print :n] - recursive call followed by print (not tail position)
    const char *inner_words[] = {"deeprec", "difference", ":n", "1", "print", ":n"};
    Node inner = NODE_NIL;
    Node inner_tail = NODE_NIL;
    for (int i = 0; i < 6; i++) {
        Node w = mem_atom(inner_words[i], strlen(inner_words[i]));
        Node c = mem_cons(w, NODE_NIL);
        if (mem_is_nil(inner)) { inner = c; inner_tail = c; }
        else { mem_set_cdr(inner_tail, c); inner_tail = c; }
    }
    Node c = mem_cons(inner, NODE_NIL);
    mem_set_cdr(tail, c);
    
    Node line_marked = NODE_MAKE_LIST(NODE_GET_INDEX(body_line));
    Node body = mem_cons(line_marked, NODE_NIL);
    
    Node name = mem_atom("deeprec", 7);
    proc_define(mem_word_ptr(name), params, 1, body);
    
    // Test with a reasonable depth that should work on all platforms
    Result r = run_string("deeprec 50");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
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
    // Define using text format with real newlines: to name\nbody\nend
    Result r = proc_define_from_text("to greetings\nprint \"hello\nend");
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
    // Define a procedure with parameters and real newlines
    Result r = proc_define_from_text("to triple :n\noutput :n * 3\nend");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = eval_string("triple 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_FLOAT(12.0f, r2.value.as.number);
}

void test_proc_define_from_text_multiple_params(void)
{
    Result r = proc_define_from_text("to avg :a :b\noutput (:a + :b) / 2\nend");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = eval_string("avg 10 20");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_FLOAT(15.0f, r2.value.as.number);
}

void test_proc_define_from_text_with_brackets(void)
{
    // Test with brackets in the body and real newlines
    Result r = proc_define_from_text("to countdown :n\nif :n > 0 [print :n countdown :n - 1]\nend");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    reset_output();
    run_string("countdown 3");
    TEST_ASSERT_EQUAL_STRING("3\n2\n1\n", output_buffer);
}

void test_proc_define_from_text_with_comparison(void)
{
    // Test with comparison operators and real newlines
    Result r = proc_define_from_text("to bigger :a :b\nif :a > :b [output :a]\noutput :b\nend");
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
    Result r = proc_define_from_text("to print :x\noutput :x\nend");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_IS_PRIMITIVE, r.error_code);
}

void test_proc_define_from_text_quoted_word(void)
{
    // Test with quoted words in body and real newlines
    Result r = proc_define_from_text("to sayhello\nprint \"hello\nend");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    reset_output();
    run_string("sayhello");
    TEST_ASSERT_EQUAL_STRING("hello\n", output_buffer);
}

void test_proc_define_from_text_all_operators(void)
{
    // Test all arithmetic and comparison operators with real newlines
    Result r = proc_define_from_text("to mathtest :x\noutput :x + 1 - 1 * 2 / 2\nend");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = eval_string("mathtest 10");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    // Due to operator precedence: 10 + 1 - 1 * 2 / 2 = 10 + 1 - 1 = 10
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r2.value.as.number);
}

void test_proc_define_from_text_equals_operator(void)
{
    // Test equals operator with real newlines
    Result r = proc_define_from_text("to iseq :a :b\nif :a = :b [output \"yes]\noutput \"no\nend");
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
    Result r = proc_define_from_text("to checkend :x\nif :x = [end] [output \"yes]\noutput \"no\nend");
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
    // Test less than operator with real newlines
    Result r = proc_define_from_text("to isless :a :b\nif :a < :b [output \"yes]\noutput \"no\nend");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = eval_string("isless 3 5");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_STRING("yes", mem_word_ptr(r2.value.as.node));
}

void test_proc_define_from_text_with_parentheses(void)
{
    // Test with parentheses in body and real newlines
    Result r = proc_define_from_text("to sumall :a :b :c\noutput (:a + :b + :c)\nend");
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
    
    Result r = proc_define_from_text("to infloop\nmake \"counter :counter + 1\n"
                                     "if :counter > 100 [stop]\n"
                                     "infloop\nend");
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
    
    Result r = proc_define_from_text("to infloop2 :n\nmake \"counter :counter + 1\n"
                                     "if :counter > 100 [stop]\n"
                                     "infloop2 :n\nend");
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
        "to foo2 :n\n"
        "make \"loopcount :loopcount + 1\n"
        "if :loopcount > 100 [stop]\n"
        "foo2 :n\n"
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
        "to fooprint :n\n"
        "make \"cnt :cnt + 1\n"
        "if :cnt > 50 [stop]\n"
        "pr (se \"Hey :n)\n"
        "fooprint :n\n"
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
        "to checkdepth :n\n"
        "make \"cnt :cnt + 1\n"
        "if :cnt > 100 [stop]\n"
        "checkdepth :n\n"
        "end");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    // Track scope depth before
    int depth_before = test_scope_depth();
    
    Result r2 = run_string("checkdepth 5");
    TEST_ASSERT_EQUAL(RESULT_NONE, r2.status);
    
    // Scope depth should be back to same level after
    int depth_after = test_scope_depth();
    TEST_ASSERT_EQUAL(depth_before, depth_after);
    
    // Should have completed 101 iterations without overflow
    Result r3 = eval_string(":cnt");
    TEST_ASSERT_EQUAL_FLOAT(101.0f, r3.value.as.number);
}

//==========================================================================
// List-of-Lists Body Structure Tests
// These tests verify the new body storage format where each line is a list
//==========================================================================

void test_text_returns_list_of_lists_structure(void)
{
    // Define a simple procedure: to test print 42 end
    // Expected text output: [[] [print 42]]
    // (params list is empty, one body line)
    Result r = proc_define_from_text("to test1\nprint 42\nend");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = eval_string("text \"test1");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_TRUE(value_is_list(r2.value));
    
    Node list = r2.value.as.node;
    
    // First element should be empty params list []
    // NIL represents an empty list
    Node params = mem_car(list);
    TEST_ASSERT_TRUE(mem_is_nil(params));
    
    // Second element should be the body line [print 42]
    Node rest = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(rest));
    Node body_line1 = mem_car(rest);
    TEST_ASSERT_EQUAL(NODE_TYPE_LIST, NODE_GET_TYPE(body_line1));
}

void test_text_multiline_procedure(void)
{
    // Define: to test2 :n\nprint :n\noutput :n * 2\nend
    // Expected text output: [[n] [print :n] [output :n * 2]]
    Result r = proc_define_from_text("to test2 :n\nprint :n\noutput :n * 2\nend");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = eval_string("text \"test2");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_TRUE(value_is_list(r2.value));
    
    Node list = r2.value.as.node;
    
    // First element: params list [n]
    Node params = mem_car(list);
    TEST_ASSERT_EQUAL(NODE_TYPE_LIST, NODE_GET_TYPE(params));
    
    // Second element: [print :n]
    Node rest1 = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(rest1));
    Node line1 = mem_car(rest1);
    TEST_ASSERT_EQUAL(NODE_TYPE_LIST, NODE_GET_TYPE(line1));
    
    // Third element: [output :n * 2]
    Node rest2 = mem_cdr(rest1);
    TEST_ASSERT_FALSE(mem_is_nil(rest2));
    Node line2 = mem_car(rest2);
    TEST_ASSERT_EQUAL(NODE_TYPE_LIST, NODE_GET_TYPE(line2));
    
    // No more elements
    Node rest3 = mem_cdr(rest2);
    TEST_ASSERT_TRUE(mem_is_nil(rest3));
}

void test_define_from_list_of_lists(void)
{
    // Test that define accepts list-of-lists format
    // define "myproc [[x] [output :x * 2]]
    Result r = run_string("define \"dbltest [[x] [output :x * 2]]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Call it
    Result r2 = eval_string("dbltest 7");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_FLOAT(14.0f, r2.value.as.number);
}

void test_define_multiline_list_of_lists(void)
{
    // Test multi-line procedure via define
    // define "multitest [[] [print 1] [print 2] [print 3]]
    Result r = run_string("define \"multitest [[] [print 1] [print 2] [print 3]]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    reset_output();
    run_string("multitest");
    TEST_ASSERT_EQUAL_STRING("1\n2\n3\n", output_buffer);
}

void test_lput_adds_line_to_procedure(void)
{
    // Define a procedure, then use lput to add a line
    // to square print 1 end
    // define "square2 lput [print 2] text "square
    // square2 should print 1 then 2
    Result r = proc_define_from_text("to sqbase\nprint 1\nend");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = run_string("define \"sqextend lput [print 2] text \"sqbase");
    TEST_ASSERT_EQUAL(RESULT_NONE, r2.status);
    
    reset_output();
    run_string("sqextend");
    TEST_ASSERT_EQUAL_STRING("1\n2\n", output_buffer);
}

void test_fput_adds_line_at_start(void)
{
    // Use fput to add a line at the start (after params)
    // This is trickier: fput [newline] text "proc puts line before params
    // We need: fput params fput [newline] bf text "proc
    Result r = proc_define_from_text("to fbase\nprint 2\nend");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    // Get text, which is [[] [print 2]]
    // We want [[] [print 1] [print 2]]
    // That's: fput first text "fbase fput [print 1] bf text "fbase
    Result r2 = run_string("define \"fextend fput first text \"fbase fput [print 1] bf text \"fbase");
    TEST_ASSERT_EQUAL(RESULT_NONE, r2.status);
    
    reset_output();
    run_string("fextend");
    TEST_ASSERT_EQUAL_STRING("1\n2\n", output_buffer);
}

void test_butlast_removes_last_line(void)
{
    // Use butlast to remove the last line of a procedure
    Result r = proc_define_from_text("to blbase\nprint 1\nprint 2\nend");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Result r2 = run_string("define \"blshort butlast text \"blbase");
    TEST_ASSERT_EQUAL(RESULT_NONE, r2.status);
    
    reset_output();
    run_string("blshort");
    TEST_ASSERT_EQUAL_STRING("1\n", output_buffer);
}

void test_empty_lines_preserved(void)
{
    // Test that empty lines are stored as empty lists when using real newlines
    // to emptytest\nprint 1\n\nprint 2\nend has an empty line between print 1 and print 2
    Result r = proc_define_from_text("to emptytest\nprint 1\n\nprint 2\nend");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    // Get text - should have: [[] [print 1] [] [print 2]]
    Result r2 = eval_string("text \"emptytest");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    
    Node list = r2.value.as.node;
    // Count elements: params + 3 body lines (including empty)
    int count = 0;
    while (!mem_is_nil(list)) {
        count++;
        list = mem_cdr(list);
    }
    // Should be 4: [params] [print 1] [] [print 2]
    TEST_ASSERT_EQUAL(4, count);
    
    // Procedure should still work (empty lines are skipped during execution)
    reset_output();
    run_string("emptytest");
    TEST_ASSERT_EQUAL_STRING("1\n2\n", output_buffer);
}

void test_item_extracts_procedure_line(void)
{
    // Use item to extract a specific line from a procedure
    Result r = proc_define_from_text("to itemtest :x\nprint :x\noutput :x * 2\nend");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    // item 1 text "itemtest should be [x] (params)
    Result r1 = eval_string("item 1 text \"itemtest");
    TEST_ASSERT_EQUAL(RESULT_OK, r1.status);
    TEST_ASSERT_TRUE(value_is_list(r1.value));
    
    // item 2 text "itemtest should be [print :x]
    Result r2 = eval_string("item 2 text \"itemtest");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_TRUE(value_is_list(r2.value));
    
    // item 3 text "itemtest should be [output :x * 2]
    Result r3 = eval_string("item 3 text \"itemtest");
    TEST_ASSERT_EQUAL(RESULT_OK, r3.status);
    TEST_ASSERT_TRUE(value_is_list(r3.value));
}

void test_multiline_with_real_newlines(void)
{
    // Test that proc_define_from_text correctly handles real newlines (not \n markers)
    Result r = proc_define_from_text(
        "to realtest :n\n"
        "print :n\n"
        "output :n * 2\n"
        "end\n");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, "Definition should succeed");
    
    // Get text and check structure
    Result r2 = eval_string("text \"realtest");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_TRUE(value_is_list(r2.value));
    
    // Count lines - should be 3: [params] [print :n] [output :n * 2]
    Node list = r2.value.as.node;
    int count = 0;
    while (!mem_is_nil(list)) {
        count++;
        list = mem_cdr(list);
    }
    TEST_ASSERT_EQUAL_MESSAGE(3, count, "Should have 3 elements: params + 2 body lines");
    
    // Test that it runs correctly
    reset_output();
    Result r3 = eval_string("realtest 5");
    TEST_ASSERT_EQUAL(RESULT_OK, r3.status);
    TEST_ASSERT_EQUAL_STRING("5\n", output_buffer);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r3.value.as.number);
}

void test_empty_list_in_procedure_body(void)
{
    // Bug: Empty list [] was being removed from procedure definitions.
    // The issue was that parse_bracket_contents returned NODE_NIL for [],
    // and the condition !mem_is_nil(item) was skipping empty lists.
    
    Result r = proc_define_from_text(
        "to test1\n"
        "  setwrite []\n"
        "end\n");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, "test1 definition should succeed");
    
    // Use text to get the procedure body and verify [] is present
    Result text_r = eval_string("text \"test1");
    TEST_ASSERT_EQUAL(RESULT_OK, text_r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, text_r.value.type);
    
    // The body should be [[] [[setwrite []]]]
    // First element is params (empty), second is the line with setwrite []
    Node body = text_r.value.as.node;
    TEST_ASSERT_FALSE_MESSAGE(mem_is_nil(body), "body should not be nil");
    
    // Skip params (first element)
    body = mem_cdr(body);
    TEST_ASSERT_FALSE_MESSAGE(mem_is_nil(body), "body should have at least one line");
    
    // Get first line
    Node first_line = mem_car(body);
    TEST_ASSERT_TRUE_MESSAGE(mem_is_list(first_line), "first line should be a list");
    
    // First token should be "setwrite"
    Node tokens = first_line;
    Node first_token = mem_car(tokens);
    TEST_ASSERT_TRUE_MESSAGE(mem_is_word(first_token), "first token should be a word");
    TEST_ASSERT_EQUAL_STRING("setwrite", mem_word_ptr(first_token));
    
    // Second token should be an empty list []
    tokens = mem_cdr(tokens);
    TEST_ASSERT_FALSE_MESSAGE(mem_is_nil(tokens), "should have second token");
    Node second_token = mem_car(tokens);
    // Empty list should be marked as LIST type with nil contents
    TEST_ASSERT_TRUE_MESSAGE(mem_is_list(second_token), "second token should be a list");
    // And it should be empty - when we iterate it, there's nothing
    TEST_ASSERT_TRUE_MESSAGE(mem_is_nil(second_token) || NODE_GET_INDEX(second_token) == 0, 
                             "empty list should have nil contents");
}

void test_multiline_brackets_repeat(void)
{
    // Bug: When brackets span multiple lines in a procedure, the body
    // is stored with flat tokens [ and ] rather than nested lists.
    // This caused "] without [" errors when repcount expressions were used.
    
    // trifwr: calls a procedure inside a repeat loop with brackets spanning lines
    Result r = proc_define_from_text(
        "to trifwr :size :n\n"
        "repeat :n [\n"
        "  print :size\n"
        "  rt 360 / :n\n"
        "]\n"
        "end\n");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, "trifwr definition should succeed");
    
    // web: outer repeat calls trifwr using repcount in expression
    Result r2 = proc_define_from_text(
        "to web\n"
        "repeat 3 [ trifwr repcount * 10 2 ]\n"
        "end\n");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r2.status, "web definition should succeed");
    
    // This should work without "] without [" error
    reset_output();
    Result r3 = run_string("web");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_NONE, r3.status, "web should complete without error");
    // Expected output: trifwr is called 3 times with sizes 10, 20, 30, each printing twice
    TEST_ASSERT_EQUAL_STRING("10\n10\n20\n20\n30\n30\n", output_buffer);
}

void test_empty_list_roundtrip(void)
{
    // Bug: When a procedure with an empty list is formatted (for editor)
    // and then re-defined from the formatted text, the empty list is lost.
    // This simulates what happens when you edit a procedure and save it.
    
    // Step 1: Define a procedure with an empty list
    Result r = proc_define_from_text(
        "to test_rt\n"
        "  setwrite []\n"
        "end\n");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, "test_rt definition should succeed");
    
    // Step 2: Format the procedure to text (like the editor does)
    UserProcedure *proc = proc_find("test_rt");
    TEST_ASSERT_NOT_NULL_MESSAGE(proc, "procedure should exist");
    
    char buffer[512];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    format_procedure_definition(format_buffer_output, &ctx, proc);
    
    // Verify the formatted text contains []
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(buffer, "[]"), 
        "formatted output should contain [] - got: '%s'");
    
    // Step 3: Erase the procedure
    proc_erase("test_rt");
    TEST_ASSERT_NULL_MESSAGE(proc_find("test_rt"), "procedure should be erased");
    
    // Step 4: Re-define from the formatted text
    Result r2 = proc_define_from_text(buffer);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r2.status, "re-definition should succeed");
    
    // Step 5: Verify the empty list is still present
    Result text_r = eval_string("text \"test_rt");
    TEST_ASSERT_EQUAL(RESULT_OK, text_r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, text_r.value.type);
    
    Node body = text_r.value.as.node;
    TEST_ASSERT_FALSE_MESSAGE(mem_is_nil(body), "body should not be nil");
    
    // Skip params (first element)
    body = mem_cdr(body);
    TEST_ASSERT_FALSE_MESSAGE(mem_is_nil(body), "body should have at least one line");
    
    // Get first line
    Node first_line = mem_car(body);
    TEST_ASSERT_TRUE_MESSAGE(mem_is_list(first_line), "first line should be a list");
    
    // Get second token (should be the empty list)
    Node tokens = first_line;
    tokens = mem_cdr(tokens);  // skip "setwrite"
    TEST_ASSERT_FALSE_MESSAGE(mem_is_nil(tokens), "should have second token");
    Node second_token = mem_car(tokens);
    TEST_ASSERT_TRUE_MESSAGE(mem_is_list(second_token), 
        "second token should be an empty list - roundtrip failed!");
}

void test_empty_list_inside_brackets_roundtrip(void)
{
    // Bug reported: Empty list [] inside brackets is lost after loading/editing.
    // Example: "if not equal? reader [] [ setread [] stop ]"
    // Becomes: "if not equal? reader [] [ setread stop ]" - the [] after setread is lost!
    
    // Step 1: Define a procedure with empty list inside brackets
    Result r = proc_define_from_text(
        "to reset\n"
        "  if not equal? reader [] [ setread [] stop ]\n"
        "end\n");
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, "reset definition should succeed");
    
    // Step 2: Format the procedure to text (like the editor does)
    UserProcedure *proc = proc_find("reset");
    TEST_ASSERT_NOT_NULL_MESSAGE(proc, "procedure should exist");
    
    char buffer[512];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    format_procedure_definition(format_buffer_output, &ctx, proc);
    
    // Both empty lists should be present
    char *first_empty = strstr(buffer, "[]");
    TEST_ASSERT_NOT_NULL_MESSAGE(first_empty, "first [] should be in output");
    char *second_empty = strstr(first_empty + 2, "[]");
    TEST_ASSERT_NOT_NULL_MESSAGE(second_empty, "second [] should be in output");
    
    // Step 3: Erase and re-define from the formatted text
    proc_erase("reset");
    TEST_ASSERT_NULL_MESSAGE(proc_find("reset"), "procedure should be erased");
    
    Result r2 = proc_define_from_text(buffer);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r2.status, "re-definition should succeed");
    
    // Step 4: Format again and verify both [] are still there
    proc = proc_find("reset");
    TEST_ASSERT_NOT_NULL_MESSAGE(proc, "procedure should exist after re-define");
    
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    format_procedure_definition(format_buffer_output, &ctx, proc);
    
    first_empty = strstr(buffer, "[]");
    TEST_ASSERT_NOT_NULL_MESSAGE(first_empty, "first [] should be in output after roundtrip");
    second_empty = strstr(first_empty + 2, "[]");
    TEST_ASSERT_NOT_NULL_MESSAGE(second_empty, 
        "second [] inside brackets should be preserved after roundtrip!");
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_simple_procedure_no_args);
    RUN_TEST(test_procedure_with_one_arg);
    RUN_TEST(test_procedure_with_two_args);
    RUN_TEST(test_procedure_local_scope);
    RUN_TEST(test_subprocedure_sees_superprocedure_inputs);
    RUN_TEST(test_subprocedure_sees_superprocedure_locals);
    RUN_TEST(test_procedure_modifies_global);
    RUN_TEST(test_recursive_procedure);
    RUN_TEST(test_tail_recursive_countdown);
    RUN_TEST(test_deep_tail_recursion);
    RUN_TEST(test_very_deep_tail_recursion);
    RUN_TEST(test_deep_non_tail_recursion_limit);
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
    
    // List-of-lists body structure tests
    RUN_TEST(test_text_returns_list_of_lists_structure);
    RUN_TEST(test_text_multiline_procedure);
    RUN_TEST(test_define_from_list_of_lists);
    RUN_TEST(test_define_multiline_list_of_lists);
    RUN_TEST(test_lput_adds_line_to_procedure);
    RUN_TEST(test_fput_adds_line_at_start);
    RUN_TEST(test_butlast_removes_last_line);
    RUN_TEST(test_empty_lines_preserved);
    RUN_TEST(test_item_extracts_procedure_line);
    RUN_TEST(test_multiline_with_real_newlines);
    RUN_TEST(test_empty_list_in_procedure_body);
    RUN_TEST(test_multiline_brackets_repeat);
    RUN_TEST(test_empty_list_roundtrip);
    RUN_TEST(test_empty_list_inside_brackets_roundtrip);

    return UNITY_END();
}
