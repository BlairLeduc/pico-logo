//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for exception handling primitives: catch, throw, error
//

#include "test_scaffold.h"
#include "core/error.h"
#include <string.h>

void setUp(void)
{
    test_scaffold_setUp();
    primitives_control_reset_test_state();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

//==========================================================================
// Catch/Throw Tests
//==========================================================================

void test_catch_basic(void)
{
    // Basic catch that just runs the list
    run_string("catch \"error [print \"hello]");
    TEST_ASSERT_EQUAL_STRING("hello\n", output_buffer);
}

void test_catch_throw_match(void)
{
    // Catch with matching throw
    Result r = run_string("catch \"mytag [throw \"mytag]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_catch_throw_nomatch(void)
{
    // Catch with non-matching throw should propagate
    Result r = run_string("catch \"othertag [throw \"mytag]");
    TEST_ASSERT_EQUAL(RESULT_THROW, r.status);
    TEST_ASSERT_EQUAL_STRING("mytag", r.throw_tag);
}

void test_throw_no_catch(void)
{
    // Throw without matching catch should return RESULT_THROW
    Result r = run_string("throw \"mytag");
    TEST_ASSERT_EQUAL(RESULT_THROW, r.status);
    TEST_ASSERT_EQUAL_STRING("mytag", r.throw_tag);
}

void test_throw_toplevel(void)
{
    // throw "toplevel should work
    Result r = run_string("throw \"toplevel");
    TEST_ASSERT_EQUAL(RESULT_THROW, r.status);
    TEST_ASSERT_EQUAL_STRING("toplevel", r.throw_tag);
}

void test_throw_toplevel_in_run_inside_catch(void)
{
    // throw "toplevel inside a catch should propagate to top level
    // even if there's a catch with a different tag
    run_string("define \"inner [[] [run [throw \"toplevel]]");
    run_string("define \"outer [[] [catch \"error [inner]]");

    Result r = run_string("outer");
    TEST_ASSERT_EQUAL(RESULT_THROW, r.status);
    TEST_ASSERT_EQUAL_STRING("toplevel", r.throw_tag);
}

void test_catch_error(void)
{
    // catch "error should catch errors
    // Test that an error is caught
    Result r = run_string("catch \"error [sum 1 \"notanumber]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // After catching, error primitive should return error info
    Result err = eval_string("error");
    TEST_ASSERT_EQUAL(RESULT_OK, err.status);
    TEST_ASSERT_TRUE(value_is_list(err.value));
    TEST_ASSERT_FALSE(mem_is_nil(err.value.as.node));

    // The error list should be:
    // [41 <formatted-error-message> sum []]
    // Where <formatted-error-message> is the error message with arguments filled in
    Node list = err.value.as.node;
    
    // First element: error code (41 = ERR_DOESNT_LIKE_INPUT)
    Node first = mem_car(list);
    TEST_ASSERT_TRUE(mem_is_word(first));
    float error_code;
    TEST_ASSERT_TRUE(value_to_number(value_word(first), &error_code));
    TEST_ASSERT_EQUAL_FLOAT(ERR_DOESNT_LIKE_INPUT, error_code);
    
    // Second element: formatted error message (word)
    list = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(list));
    Node second = mem_car(list);
    TEST_ASSERT_TRUE(mem_is_word(second));
    const char *message = mem_word_ptr(second);
    TEST_ASSERT_NOT_NULL(message);
    // The message is a template like "%s doesn't like %s as input"
    TEST_ASSERT_EQUAL_STRING("sum doesn't like notanumber as input", message);
    
    // Third element: primitive name ("sum")
    list = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(list));
    Node third = mem_car(list);
    TEST_ASSERT_TRUE(mem_is_word(third));
    TEST_ASSERT_EQUAL_STRING("sum", mem_word_ptr(third));
    
    // Fourth element: caller procedure (empty list since at top level)
    list = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(list));
    Node fourth = mem_car(list);
    TEST_ASSERT_TRUE(mem_is_nil(fourth));  // Empty list (NODE_NIL)
    
    // Should be end of list
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(list)));
}

void test_error_no_error(void)
{
    // error should return empty list if no error occurred
    Result r = eval_string("error");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_catch_through_calls_good(void)
{
    // Test that catch works through nested procedure calls
    run_string("define \"tc [[in] [catch \"oops [trythis :in]]]");
    run_string("define \"trythis [[n] [pr check :n pr \"good]]");
    run_string("define \"check [[num] [if :num = 0 [throw \"oops] op :num]]");

    // Run catch around outerproc
    Result r = run_string("tc 1");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "1\ngood\n"));
    
    // Clean up
    run_string("erase \"tc");
    run_string("erase \"trythis");
    run_string("erase \"check");
}

void test_catch_through_calls_catch(void)
{
    // Test that catch works through nested procedure calls
    run_string("define \"tc [[in] [catch \"oops [trythis :in]]]");
    run_string("define \"trythis [[n] [pr check :n pr \"good]]");
    run_string("define \"check [[num] [if :num = 0 [throw \"oops] op :num]]");

    // Run catch around outerproc
    Result r = run_string("tc 0");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Clean up
    run_string("erase \"tc");
    run_string("erase \"trythis");
    run_string("erase \"check");
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_catch_basic);
    RUN_TEST(test_catch_throw_match);
    RUN_TEST(test_catch_throw_nomatch);
    RUN_TEST(test_throw_no_catch);
    RUN_TEST(test_throw_toplevel);
    RUN_TEST(test_throw_toplevel_in_run_inside_catch);
    RUN_TEST(test_catch_error);
    RUN_TEST(test_error_no_error);
    RUN_TEST(test_catch_through_calls_good);
    RUN_TEST(test_catch_through_calls_catch);

    return UNITY_END();
}
