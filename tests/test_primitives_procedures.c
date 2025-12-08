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

    return UNITY_END();
}
