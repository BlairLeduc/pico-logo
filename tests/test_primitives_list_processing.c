//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for list processing primitives: apply, foreach, map, filter, find, reduce, crossmap
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
// apply tests
//==========================================================================

void test_apply_with_primitive_name(void)
{
    // apply "sum [1 2 3 4] - sum takes 2 args by default, but with parens can take more
    Result r = eval_string("apply \"sum [3 4]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, r.value.as.number);
}

void test_apply_with_user_procedure(void)
{
    // Define a procedure using define_proc helper
    const char *params[] = {"x"};
    define_proc("double", params, 1, "output :x * 2");
    
    Result r = eval_string("apply \"double [5]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.value.as.number);
}

void test_apply_with_lambda(void)
{
    // apply [[x] :x + 1] [5]
    Result r = eval_string("apply [[x] :x + 1] [5]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(6.0f, r.value.as.number);
}

void test_apply_with_multi_param_lambda(void)
{
    // apply [[a b c] :a + :b + :c] [1 2 3]
    Result r = eval_string("apply [[a b c] :a + :b + :c] [1 2 3]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(6.0f, r.value.as.number);
}

void test_apply_with_procedure_text(void)
{
    // apply [[x] [output :x * 3]] [4]
    Result r = eval_string("apply [[x] [output :x * 3]] [4]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(12.0f, r.value.as.number);
}

void test_apply_with_word_primitive(void)
{
    // apply "word [hello world] - list elements are words without quotes
    Result r = eval_string("apply \"word [hello world]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("helloworld", mem_word_ptr(r.value.as.node));
}

void test_apply_unknown_procedure(void)
{
    Result r = eval_string("apply \"nonexistent [1 2]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DONT_KNOW_HOW, r.error_code);
}

//==========================================================================
// foreach tests
//==========================================================================

void test_foreach_basic(void)
{
    // foreach [1 2 3] [[x] print :x]
    run_string("foreach [1 2 3] [[x] print :x]");
    TEST_ASSERT_EQUAL_STRING("1\n2\n3\n", output_buffer);
}

void test_foreach_with_named_procedure(void)
{
    // Define a procedure using define_proc helper
    const char *params[] = {"x"};
    define_proc("showit", params, 1, "print :x");
    
    run_string("foreach [a b c] \"showit");
    TEST_ASSERT_EQUAL_STRING("a\nb\nc\n", output_buffer);
}

void test_foreach_multi_list(void)
{
    // (foreach [1 2 3] [a b c] [[x y] print word :x :y])
    run_string("(foreach [1 2 3] [a b c] [[x y] print word :x :y])");
    TEST_ASSERT_EQUAL_STRING("1a\n2b\n3c\n", output_buffer);
}

void test_foreach_empty_list(void)
{
    // foreach [] [[x] print :x] - should do nothing
    run_string("foreach [] [[x] print :x]");
    TEST_ASSERT_EQUAL_STRING("", output_buffer);
}

void test_foreach_with_word(void)
{
    // foreach "abc [[c] print :c]
    run_string("foreach \"abc [[c] print :c]");
    TEST_ASSERT_EQUAL_STRING("a\nb\nc\n", output_buffer);
}

//==========================================================================
// map tests
//==========================================================================

void test_map_basic(void)
{
    // map [[x] :x * 2] [1 2 3]
    Result r = eval_string("map [[x] :x * 2] [1 2 3]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    // Check result is [2 4 6]
    Node list = r.value.as.node;
    TEST_ASSERT_FALSE(mem_is_nil(list));
    
    Node first = mem_car(list);
    TEST_ASSERT_TRUE(mem_is_word(first));
    TEST_ASSERT_EQUAL_STRING("2", mem_word_ptr(first));
    
    list = mem_cdr(list);
    Node second = mem_car(list);
    TEST_ASSERT_EQUAL_STRING("4", mem_word_ptr(second));
    
    list = mem_cdr(list);
    Node third = mem_car(list);
    TEST_ASSERT_EQUAL_STRING("6", mem_word_ptr(third));
    
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(list)));
}

void test_map_with_primitive(void)
{
    // (map "word [a b c] [1 2 3]) -> [a1 b2 c3]
    Result r = eval_string("(map \"word [a b c] [1 2 3])");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    Node list = r.value.as.node;
    Node first = mem_car(list);
    TEST_ASSERT_EQUAL_STRING("a1", mem_word_ptr(first));
    
    list = mem_cdr(list);
    Node second = mem_car(list);
    TEST_ASSERT_EQUAL_STRING("b2", mem_word_ptr(second));
    
    list = mem_cdr(list);
    Node third = mem_car(list);
    TEST_ASSERT_EQUAL_STRING("c3", mem_word_ptr(third));
}

void test_map_empty_list(void)
{
    Result r = eval_string("map [[x] :x * 2] []");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_map_with_user_procedure(void)
{
    // Define a procedure using define_proc helper
    const char *params[] = {"n"};
    define_proc("square", params, 1, "output :n * :n");
    
    Result r = eval_string("map \"square [1 2 3 4]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("1", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("4", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("9", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("16", mem_word_ptr(mem_car(list)));
}

//==========================================================================
// filter tests
//==========================================================================

void test_filter_basic(void)
{
    // filter [[x] :x > 2] [1 2 3 4 5]
    Result r = eval_string("filter [[x] :x > 2] [1 2 3 4 5]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    // Should be [3 4 5]
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("3", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("4", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("5", mem_word_ptr(mem_car(list)));
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(list)));
}

void test_filter_none_match(void)
{
    Result r = eval_string("filter [[x] :x > 100] [1 2 3]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_filter_all_match(void)
{
    Result r = eval_string("filter [[x] :x > 0] [1 2 3]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    // Should be [1 2 3]
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("1", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("2", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("3", mem_word_ptr(mem_car(list)));
}

void test_filter_with_user_procedure(void)
{
    // Define a procedure using define_proc helper
    const char *params[] = {"n"};
    define_proc("even?", params, 1, "output 0 = remainder :n 2");
    
    Result r = eval_string("filter \"even? [1 2 3 4 5 6]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    // Should be [2 4 6]
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("2", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("4", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("6", mem_word_ptr(mem_car(list)));
}

//==========================================================================
// find tests
//==========================================================================

void test_find_basic(void)
{
    // find [[x] 0 = remainder :x 2] [1 3 4 5 6]
    Result r = eval_string("find [[x] 0 = remainder :x 2] [1 3 4 5 6]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("4", mem_word_ptr(r.value.as.node));
}

void test_find_not_found(void)
{
    Result r = eval_string("find [[x] :x > 100] [1 2 3]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_find_first_element(void)
{
    Result r = eval_string("find [[x] :x > 0] [1 2 3]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("1", mem_word_ptr(r.value.as.node));
}

//==========================================================================
// reduce tests
//==========================================================================

void test_reduce_sum(void)
{
    // reduce [[a b] :a + :b] [1 2 3 4]
    Result r = eval_string("reduce [[a b] :a + :b] [1 2 3 4]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.value.as.number);
}

void test_reduce_word_concatenation(void)
{
    // reduce [[x y] word :x :y] [a b c d e]
    Result r = eval_string("reduce [[x y] word :x :y] [a b c d e]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("abcde", mem_word_ptr(r.value.as.node));
}

void test_reduce_single_element(void)
{
    Result r = eval_string("reduce [[a b] :a + :b] [42]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("42", mem_word_ptr(r.value.as.node));
}

void test_reduce_with_primitive(void)
{
    Result r = eval_string("reduce \"sum [1 2 3 4]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r.value.as.number);
}

void test_reduce_empty_list_error(void)
{
    Result r = eval_string("reduce [[a b] :a + :b] []");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

//==========================================================================
// crossmap tests
//==========================================================================

void test_crossmap_basic(void)
{
    // crossmap [[x y] :x + :y] [[1 2] [10 20 30]]
    Result r = eval_string("crossmap [[x y] :x + :y] [[1 2] [10 20 30]]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    // Expected: [11 21 31 12 22 32]
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("11", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("21", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("31", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("12", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("22", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("32", mem_word_ptr(mem_car(list)));
}

void test_crossmap_with_word_primitive(void)
{
    // (crossmap "word [a b c] [1 2 3 4])
    Result r = eval_string("(crossmap \"word [a b c] [1 2 3 4])");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    // Expected: [a1 a2 a3 a4 b1 b2 b3 b4 c1 c2 c3 c4]
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("a1", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("a2", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("a3", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("a4", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("b1", mem_word_ptr(mem_car(list)));
    // ... rest follows same pattern
}

void test_crossmap_empty_list(void)
{
    Result r = eval_string("crossmap [[x y] :x + :y] [[] [1 2]]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

//==========================================================================
// Lambda variable scoping tests
//==========================================================================

void test_lambda_doesnt_clobber_variables(void)
{
    // Set a variable, use it in lambda, make sure it's preserved
    run_string("make \"x 100");
    run_string("foreach [1 2 3] [[x] print :x]");
    
    Result r = eval_string(":x");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, r.value.as.number);
}

void test_nested_lambda_scoping(void)
{
    // Nested lambdas should preserve outer variables
    run_string("make \"y 50");
    Result r = eval_string("map [[x] :x + :y] [1 2 3]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    // Should be [51 52 53]
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("51", mem_word_ptr(mem_car(list)));
}

//==========================================================================
// Word input tests
//==========================================================================

void test_reduce_with_word(void)
{
    // reduce [[a b] word :b :a] "hello -> olleh
    Result r = eval_string("reduce [[a b] word :b :a] \"hello");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("olleh", mem_word_ptr(r.value.as.node));
}

void test_map_with_word(void)
{
    // map [[x] uppercase :x] "hello -> "HELLO (word input returns word output)
    Result r = eval_string("map [[x] uppercase :x] \"hello");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("HELLO", mem_word_ptr(r.value.as.node));
}

void test_map_with_number(void)
{
    // map [[x] :x] 123 -> "123" (number treated as word, returns word)
    Result r = eval_string("map [[x] :x] 123");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("123", mem_word_ptr(r.value.as.node));
}

//==========================================================================
// map.se tests
//==========================================================================

void test_map_se_basic(void)
{
    // map.se [[x] list :x :x] [1 2 3] -> [1 1 2 2 3 3]
    Result r = eval_string("map.se [[x] list :x :x] [1 2 3]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("1", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("1", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("2", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("2", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("3", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("3", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_TRUE(mem_is_nil(list));
}

void test_map_se_with_empty_result(void)
{
    // Empty list results contribute nothing
    // When procedure outputs empty list [], nothing is added to result
    // map.se [[x] if :x > 2 [(list :x)] [[]]] [1 2 3 4] -> [3 4]
    Result r = eval_string("map.se [[x] (if :x > 2 [(list :x)] [[]])] [1 2 3 4]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("3", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("4", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_TRUE(mem_is_nil(list));
}

void test_map_se_with_word_result(void)
{
    // Words are added as single elements
    // map.se [[x] :x] [a b c] -> [a b c]
    Result r = eval_string("map.se [[x] :x] [a b c]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("a", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("b", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("c", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_TRUE(mem_is_nil(list));
}

void test_map_se_empty_list(void)
{
    Result r = eval_string("map.se [[x] list :x] []");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_map_se_with_word_data(void)
{
    // map.se [[x] list :x :x] "ab -> [a a b b]
    Result r = eval_string("map.se [[x] list :x :x] \"ab");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("a", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("a", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("b", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("b", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_TRUE(mem_is_nil(list));
}

void test_map_se_multi_list(void)
{
    // (map.se [[a b] list :a :b] [1 2] [x y]) -> [1 x 2 y]
    Result r = eval_string("(map.se [[a b] list :a :b] [1 2] [x y])");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("1", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("x", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("2", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("y", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_TRUE(mem_is_nil(list));
}

void test_filter_with_word(void)
{
    // Filter vowels from a word - output should be a word since input is a word
    Result r = eval_string("filter [[x] member? :x \"aeiou] \"hello");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    
    // Should be "eo"
    TEST_ASSERT_EQUAL_STRING("eo", mem_word_ptr(r.value.as.node));
}

void test_find_with_word(void)
{
    // Find first vowel in a word
    Result r = eval_string("find [[x] member? :x \"aeiou] \"hello");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("e", mem_word_ptr(r.value.as.node));
}

void test_find_with_word_not_found(void)
{
    // No vowels in "xyz"
    Result r = eval_string("find [[x] member? :x \"aeiou] \"xyz");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_crossmap_with_word_data(void)
{
    // crossmap [[x y] word :x :y] "ab [1 2] -> [a1 a2 b1 b2]
    Result r = eval_string("(crossmap [[x y] word :x :y] \"ab [1 2])");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("a1", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("a2", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("b1", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("b2", mem_word_ptr(mem_car(list)));
}

void test_crossmap_listlist_with_word(void)
{
    // crossmap with a word in the listlist - use a variable to inject the word
    run_string("make \"chars \"ab");
    Result r = eval_string("crossmap [[x y] word :x :y] (list :chars [1 2])");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("a1", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("a2", mem_word_ptr(mem_car(list)));
}

//==========================================================================
// Main
//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    
    // apply tests
    RUN_TEST(test_apply_with_primitive_name);
    RUN_TEST(test_apply_with_user_procedure);
    RUN_TEST(test_apply_with_lambda);
    RUN_TEST(test_apply_with_multi_param_lambda);
    RUN_TEST(test_apply_with_procedure_text);
    RUN_TEST(test_apply_with_word_primitive);
    RUN_TEST(test_apply_unknown_procedure);
    
    // foreach tests
    RUN_TEST(test_foreach_basic);
    RUN_TEST(test_foreach_with_named_procedure);
    RUN_TEST(test_foreach_multi_list);
    RUN_TEST(test_foreach_empty_list);
    RUN_TEST(test_foreach_with_word);
    
    // map tests
    RUN_TEST(test_map_basic);
    RUN_TEST(test_map_with_primitive);
    RUN_TEST(test_map_empty_list);
    RUN_TEST(test_map_with_user_procedure);
    RUN_TEST(test_map_with_word);
    RUN_TEST(test_map_with_number);
    
    // map.se tests
    RUN_TEST(test_map_se_basic);
    RUN_TEST(test_map_se_with_empty_result);
    RUN_TEST(test_map_se_with_word_result);
    RUN_TEST(test_map_se_empty_list);
    RUN_TEST(test_map_se_with_word_data);
    RUN_TEST(test_map_se_multi_list);
    
    // filter tests
    RUN_TEST(test_filter_basic);
    RUN_TEST(test_filter_none_match);
    RUN_TEST(test_filter_all_match);
    RUN_TEST(test_filter_with_user_procedure);
    RUN_TEST(test_filter_with_word);
    
    // find tests
    RUN_TEST(test_find_basic);
    RUN_TEST(test_find_not_found);
    RUN_TEST(test_find_first_element);
    RUN_TEST(test_find_with_word);
    RUN_TEST(test_find_with_word_not_found);
    
    // reduce tests
    RUN_TEST(test_reduce_sum);
    RUN_TEST(test_reduce_word_concatenation);
    RUN_TEST(test_reduce_single_element);
    RUN_TEST(test_reduce_with_primitive);
    RUN_TEST(test_reduce_empty_list_error);
    RUN_TEST(test_reduce_with_word);
    
    // crossmap tests
    RUN_TEST(test_crossmap_basic);
    RUN_TEST(test_crossmap_with_word_primitive);
    RUN_TEST(test_crossmap_empty_list);
    RUN_TEST(test_crossmap_with_word_data);
    RUN_TEST(test_crossmap_listlist_with_word);
    
    // Lambda scoping tests
    RUN_TEST(test_lambda_doesnt_clobber_variables);
    RUN_TEST(test_nested_lambda_scoping);
    
    return UNITY_END();
}
