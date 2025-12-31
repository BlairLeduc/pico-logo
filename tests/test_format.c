//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "test_scaffold.h"
#include "core/format.h"
#include <stdlib.h>
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
// Buffer Context Tests
//==========================================================================

void test_format_buffer_init_sets_empty_buffer(void)
{
    char buffer[128];
    FormatBufferContext ctx;
    
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    TEST_ASSERT_EQUAL_PTR(buffer, ctx.buffer);
    TEST_ASSERT_EQUAL(sizeof(buffer), ctx.buffer_size);
    TEST_ASSERT_EQUAL(0, ctx.pos);
    TEST_ASSERT_EQUAL_STRING("", buffer);
}

void test_format_buffer_output_appends_string(void)
{
    char buffer[128];
    FormatBufferContext ctx;
    
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    TEST_ASSERT_TRUE(format_buffer_output(&ctx, "hello"));
    TEST_ASSERT_EQUAL_STRING("hello", buffer);
    TEST_ASSERT_EQUAL(5, ctx.pos);
}

void test_format_buffer_output_appends_multiple(void)
{
    char buffer[128];
    FormatBufferContext ctx;
    
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    TEST_ASSERT_TRUE(format_buffer_output(&ctx, "hello"));
    TEST_ASSERT_TRUE(format_buffer_output(&ctx, " "));
    TEST_ASSERT_TRUE(format_buffer_output(&ctx, "world"));
    TEST_ASSERT_EQUAL_STRING("hello world", buffer);
    TEST_ASSERT_EQUAL(11, ctx.pos);
}

void test_format_buffer_output_fails_on_overflow(void)
{
    char buffer[10];
    FormatBufferContext ctx;
    
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    // This should fail because "hello world" (11 chars) won't fit
    TEST_ASSERT_TRUE(format_buffer_output(&ctx, "hello"));
    TEST_ASSERT_FALSE(format_buffer_output(&ctx, " world"));  // Would exceed buffer
}

void test_format_buffer_pos_returns_position(void)
{
    char buffer[128];
    FormatBufferContext ctx;
    
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    TEST_ASSERT_EQUAL(0, format_buffer_pos(&ctx));
    format_buffer_output(&ctx, "test");
    TEST_ASSERT_EQUAL(4, format_buffer_pos(&ctx));
}

//==========================================================================
// format_body_element Tests
//==========================================================================

void test_format_body_element_word(void)
{
    char buffer[128];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Node word = mem_atom("hello", 5);
    
    TEST_ASSERT_TRUE(format_body_element(format_buffer_output, &ctx, word));
    TEST_ASSERT_EQUAL_STRING("hello", buffer);
}

void test_format_body_element_nil_produces_no_output(void)
{
    char buffer[128];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    // NODE_NIL is not considered a list by mem_is_list()
    // It represents the empty list terminator, not a printable []
    Node nil = NODE_NIL;
    
    TEST_ASSERT_TRUE(format_body_element(format_buffer_output, &ctx, nil));
    TEST_ASSERT_EQUAL_STRING("", buffer);
}

void test_format_body_element_simple_list(void)
{
    char buffer[128];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    // Create list [a b c]
    Node a = mem_atom("a", 1);
    Node b = mem_atom("b", 1);
    Node c = mem_atom("c", 1);
    Node list = mem_cons(a, mem_cons(b, mem_cons(c, NODE_NIL)));
    
    TEST_ASSERT_TRUE(format_body_element(format_buffer_output, &ctx, list));
    TEST_ASSERT_EQUAL_STRING("[a b c]", buffer);
}

void test_format_body_element_nested_list(void)
{
    char buffer[128];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    // Create list [a [b c] d]
    Node a = mem_atom("a", 1);
    Node b = mem_atom("b", 1);
    Node c = mem_atom("c", 1);
    Node d = mem_atom("d", 1);
    Node inner = mem_cons(b, mem_cons(c, NODE_NIL));
    Node list = mem_cons(a, mem_cons(inner, mem_cons(d, NODE_NIL)));
    
    TEST_ASSERT_TRUE(format_body_element(format_buffer_output, &ctx, list));
    TEST_ASSERT_EQUAL_STRING("[a [b c] d]", buffer);
}

//==========================================================================
// format_procedure_title Tests
//==========================================================================

void test_format_procedure_title_no_params(void)
{
    char buffer[256];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    const char *params[] = {};
    define_proc("myproc", params, 0, "print 1");
    
    UserProcedure *proc = proc_find("myproc");
    TEST_ASSERT_NOT_NULL(proc);
    
    TEST_ASSERT_TRUE(format_procedure_title(format_buffer_output, &ctx, proc));
    TEST_ASSERT_EQUAL_STRING("to myproc\n", buffer);
}

void test_format_procedure_title_one_param(void)
{
    char buffer[256];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Node x = mem_atom("x", 1);
    const char *params[] = {mem_word_ptr(x)};
    define_proc("double", params, 1, "output :x * 2");
    
    UserProcedure *proc = proc_find("double");
    TEST_ASSERT_NOT_NULL(proc);
    
    TEST_ASSERT_TRUE(format_procedure_title(format_buffer_output, &ctx, proc));
    TEST_ASSERT_EQUAL_STRING("to double :x\n", buffer);
}

void test_format_procedure_title_multiple_params(void)
{
    char buffer[256];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Node a = mem_atom("a", 1);
    Node b = mem_atom("b", 1);
    Node c = mem_atom("c", 1);
    const char *params[] = {mem_word_ptr(a), mem_word_ptr(b), mem_word_ptr(c)};
    define_proc("trisum", params, 3, "output :a + :b + :c");
    
    UserProcedure *proc = proc_find("trisum");
    TEST_ASSERT_NOT_NULL(proc);
    
    TEST_ASSERT_TRUE(format_procedure_title(format_buffer_output, &ctx, proc));
    TEST_ASSERT_EQUAL_STRING("to trisum :a :b :c\n", buffer);
}

//==========================================================================
// format_procedure_definition Tests
//==========================================================================

void test_format_procedure_definition_simple(void)
{
    char buffer[512];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    const char *params[] = {};
    define_proc("hello", params, 0, "print \"world");
    
    UserProcedure *proc = proc_find("hello");
    TEST_ASSERT_NOT_NULL(proc);
    
    TEST_ASSERT_TRUE(format_procedure_definition(format_buffer_output, &ctx, proc));
    TEST_ASSERT_TRUE(strstr(buffer, "to hello\n") != NULL);
    TEST_ASSERT_TRUE(strstr(buffer, "print") != NULL);
    TEST_ASSERT_TRUE(strstr(buffer, "end\n") != NULL);
}

void test_format_procedure_definition_with_params(void)
{
    char buffer[512];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Node x = mem_atom("x", 1);
    const char *params[] = {mem_word_ptr(x)};
    define_proc("double", params, 1, "output :x * 2");
    
    UserProcedure *proc = proc_find("double");
    TEST_ASSERT_NOT_NULL(proc);
    
    TEST_ASSERT_TRUE(format_procedure_definition(format_buffer_output, &ctx, proc));
    TEST_ASSERT_TRUE(strstr(buffer, "to double :x\n") != NULL);
    TEST_ASSERT_TRUE(strstr(buffer, "end\n") != NULL);
}

void test_format_procedure_definition_multiline(void)
{
    char buffer[512];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    const char *params[] = {};
    define_proc("multi", params, 0, "print 1\nprint 2\nprint 3");
    
    UserProcedure *proc = proc_find("multi");
    TEST_ASSERT_NOT_NULL(proc);
    
    TEST_ASSERT_TRUE(format_procedure_definition(format_buffer_output, &ctx, proc));
    TEST_ASSERT_TRUE(strstr(buffer, "to multi\n") != NULL);
    TEST_ASSERT_TRUE(strstr(buffer, "end\n") != NULL);
}

//==========================================================================
// format_variable Tests
//==========================================================================

void test_format_variable_number(void)
{
    char buffer[256];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Value val = value_number(42);
    
    TEST_ASSERT_TRUE(format_variable(format_buffer_output, &ctx, "x", val));
    TEST_ASSERT_EQUAL_STRING("make \"x 42\n", buffer);
}

void test_format_variable_decimal(void)
{
    char buffer[256];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Value val = value_number(3.14f);
    
    TEST_ASSERT_TRUE(format_variable(format_buffer_output, &ctx, "pi", val));
    TEST_ASSERT_TRUE(strstr(buffer, "make \"pi 3.14") != NULL);
}

void test_format_variable_word(void)
{
    char buffer[256];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Node word = mem_atom("hello", 5);
    Value val = value_word(word);
    
    TEST_ASSERT_TRUE(format_variable(format_buffer_output, &ctx, "greeting", val));
    TEST_ASSERT_EQUAL_STRING("make \"greeting \"hello\n", buffer);
}

void test_format_variable_empty_list(void)
{
    char buffer[256];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Value val = value_list(NODE_NIL);
    
    TEST_ASSERT_TRUE(format_variable(format_buffer_output, &ctx, "empty", val));
    TEST_ASSERT_EQUAL_STRING("make \"empty []\n", buffer);
}

void test_format_variable_list(void)
{
    char buffer[256];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Node a = mem_atom("a", 1);
    Node b = mem_atom("b", 1);
    Node c = mem_atom("c", 1);
    Node list = mem_cons(a, mem_cons(b, mem_cons(c, NODE_NIL)));
    Value val = value_list(list);
    
    TEST_ASSERT_TRUE(format_variable(format_buffer_output, &ctx, "letters", val));
    TEST_ASSERT_EQUAL_STRING("make \"letters [a b c]\n", buffer);
}

void test_format_variable_nested_list(void)
{
    char buffer[256];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    // Create [[1 2] [3 4]]
    Node n1 = mem_atom("1", 1);
    Node n2 = mem_atom("2", 1);
    Node n3 = mem_atom("3", 1);
    Node n4 = mem_atom("4", 1);
    Node inner1 = mem_cons(n1, mem_cons(n2, NODE_NIL));
    Node inner2 = mem_cons(n3, mem_cons(n4, NODE_NIL));
    Node list = mem_cons(inner1, mem_cons(inner2, NODE_NIL));
    Value val = value_list(list);
    
    TEST_ASSERT_TRUE(format_variable(format_buffer_output, &ctx, "matrix", val));
    TEST_ASSERT_EQUAL_STRING("make \"matrix [[1 2] [3 4]]\n", buffer);
}

//==========================================================================
// format_property Tests
//==========================================================================

void test_format_property_word_value(void)
{
    char buffer[256];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Node val = mem_atom("blue", 4);
    
    TEST_ASSERT_TRUE(format_property(format_buffer_output, &ctx, "car", "color", val));
    TEST_ASSERT_EQUAL_STRING("pprop \"car \"color \"blue\n", buffer);
}

void test_format_property_number_value(void)
{
    char buffer[256];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Node val = mem_atom("42", 2);  // Numbers stored as words
    
    TEST_ASSERT_TRUE(format_property(format_buffer_output, &ctx, "item", "count", val));
    TEST_ASSERT_EQUAL_STRING("pprop \"item \"count 42\n", buffer);
}

void test_format_property_list_value(void)
{
    char buffer[256];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Node a = mem_atom("a", 1);
    Node b = mem_atom("b", 1);
    Node list = mem_cons(a, mem_cons(b, NODE_NIL));
    
    TEST_ASSERT_TRUE(format_property(format_buffer_output, &ctx, "obj", "items", list));
    TEST_ASSERT_EQUAL_STRING("pprop \"obj \"items [a b]\n", buffer);
}

//==========================================================================
// format_property_list Tests
//==========================================================================

void test_format_property_list_single_property(void)
{
    char buffer[256];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    // Create [color blue]
    Node prop = mem_atom("color", 5);
    Node val = mem_atom("blue", 4);
    Node plist = mem_cons(prop, mem_cons(val, NODE_NIL));
    
    TEST_ASSERT_TRUE(format_property_list(format_buffer_output, &ctx, "car", plist));
    TEST_ASSERT_EQUAL_STRING("pprop \"car \"color \"blue\n", buffer);
}

void test_format_property_list_multiple_properties(void)
{
    char buffer[512];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    // Create [color blue speed 100]
    Node p1 = mem_atom("color", 5);
    Node v1 = mem_atom("blue", 4);
    Node p2 = mem_atom("speed", 5);
    Node v2 = mem_atom("100", 3);
    Node plist = mem_cons(p1, mem_cons(v1, mem_cons(p2, mem_cons(v2, NODE_NIL))));
    
    TEST_ASSERT_TRUE(format_property_list(format_buffer_output, &ctx, "car", plist));
    TEST_ASSERT_TRUE(strstr(buffer, "pprop \"car \"color \"blue\n") != NULL);
    TEST_ASSERT_TRUE(strstr(buffer, "pprop \"car \"speed 100\n") != NULL);
}

void test_format_property_list_empty(void)
{
    char buffer[256];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Node empty = NODE_NIL;
    
    TEST_ASSERT_TRUE(format_property_list(format_buffer_output, &ctx, "obj", empty));
    TEST_ASSERT_EQUAL_STRING("", buffer);  // No output for empty list
}

//==========================================================================
// Simplified Buffer API Wrapper Tests
//==========================================================================

void test_format_procedure_to_buffer_simple(void)
{
    char buffer[512];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    const char *params[] = {};
    define_proc("greet", params, 0, "print \"hello");
    
    UserProcedure *proc = proc_find("greet");
    TEST_ASSERT_NOT_NULL(proc);
    
    TEST_ASSERT_TRUE(format_procedure_to_buffer(&ctx, proc));
    TEST_ASSERT_TRUE(strstr(buffer, "to greet") != NULL);
    TEST_ASSERT_TRUE(strstr(buffer, "end") != NULL);
}

void test_format_variable_to_buffer_number(void)
{
    char buffer[128];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Value val = value_number(42);
    
    TEST_ASSERT_TRUE(format_variable_to_buffer(&ctx, "x", val));
    TEST_ASSERT_EQUAL_STRING("make \"x 42\n", buffer);
}

void test_format_variable_to_buffer_word(void)
{
    char buffer[128];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Node word = mem_atom("hello", 5);
    Value val = value_word(word);
    
    TEST_ASSERT_TRUE(format_variable_to_buffer(&ctx, "greeting", val));
    TEST_ASSERT_EQUAL_STRING("make \"greeting \"hello\n", buffer);
}

void test_format_property_to_buffer_word(void)
{
    char buffer[256];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Node val = mem_atom("blue", 4);
    
    TEST_ASSERT_TRUE(format_property_to_buffer(&ctx, "car", "color", val));
    TEST_ASSERT_EQUAL_STRING("pprop \"car \"color \"blue\n", buffer);
}

void test_format_property_list_to_buffer_single(void)
{
    char buffer[256];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Node prop = mem_atom("color", 5);
    Node val = mem_atom("red", 3);
    Node plist = mem_cons(prop, mem_cons(val, NODE_NIL));
    
    TEST_ASSERT_TRUE(format_property_list_to_buffer(&ctx, "obj", plist));
    TEST_ASSERT_EQUAL_STRING("pprop \"obj \"color \"red\n", buffer);
}

void test_format_value_to_buffer_number(void)
{
    char buffer[64];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Value val = value_number(3.14f);
    
    TEST_ASSERT_TRUE(format_value_to_buffer(&ctx, val));
    TEST_ASSERT_EQUAL_STRING("3.14", buffer);
}

void test_format_value_to_buffer_word(void)
{
    char buffer[64];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Node word = mem_atom("hello", 5);
    Value val = value_word(word);
    
    TEST_ASSERT_TRUE(format_value_to_buffer(&ctx, val));
    TEST_ASSERT_EQUAL_STRING("hello", buffer);
}

void test_format_value_to_buffer_list(void)
{
    char buffer[128];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Node a = mem_atom("a", 1);
    Node b = mem_atom("b", 1);
    Node list = mem_cons(a, mem_cons(b, NODE_NIL));
    Value val = value_list(list);
    
    TEST_ASSERT_TRUE(format_value_to_buffer(&ctx, val));
    TEST_ASSERT_EQUAL_STRING("a b", buffer);  // No outer brackets for print/type
}

void test_format_value_show_to_buffer_list(void)
{
    char buffer[128];
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Node a = mem_atom("a", 1);
    Node b = mem_atom("b", 1);
    Node list = mem_cons(a, mem_cons(b, NODE_NIL));
    Value val = value_list(list);
    
    TEST_ASSERT_TRUE(format_value_show_to_buffer(&ctx, val));
    TEST_ASSERT_EQUAL_STRING("[a b]", buffer);  // With outer brackets for show
}

void test_format_to_buffer_fails_on_overflow(void)
{
    char buffer[10];  // Too small
    FormatBufferContext ctx;
    format_buffer_init(&ctx, buffer, sizeof(buffer));
    
    Value val = value_number(42);
    
    // "make \"longname 42\n" won't fit in 10 bytes
    TEST_ASSERT_FALSE(format_variable_to_buffer(&ctx, "longname", val));
}

//==========================================================================
// Integration Tests - Custom Output Callback
//==========================================================================

// Custom callback that counts calls
static int callback_count;
static bool counting_callback(void *ctx, const char *str)
{
    (void)ctx;
    (void)str;
    callback_count++;
    return true;
}

void test_format_uses_callback(void)
{
    callback_count = 0;
    
    Node word = mem_atom("test", 4);
    format_body_element(counting_callback, NULL, word);
    
    TEST_ASSERT_GREATER_THAN(0, callback_count);
}

// Callback that always fails
static bool failing_callback(void *ctx, const char *str)
{
    (void)ctx;
    (void)str;
    return false;
}

void test_format_propagates_callback_failure(void)
{
    Node word = mem_atom("test", 4);
    
    TEST_ASSERT_FALSE(format_body_element(failing_callback, NULL, word));
}

void test_format_variable_propagates_failure(void)
{
    Value val = value_number(42);
    
    TEST_ASSERT_FALSE(format_variable(failing_callback, NULL, "x", val));
}

//==========================================================================
// Main
//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    
    // Buffer context tests
    RUN_TEST(test_format_buffer_init_sets_empty_buffer);
    RUN_TEST(test_format_buffer_output_appends_string);
    RUN_TEST(test_format_buffer_output_appends_multiple);
    RUN_TEST(test_format_buffer_output_fails_on_overflow);
    RUN_TEST(test_format_buffer_pos_returns_position);
    
    // format_body_element tests
    RUN_TEST(test_format_body_element_word);
    RUN_TEST(test_format_body_element_nil_produces_no_output);
    RUN_TEST(test_format_body_element_simple_list);
    RUN_TEST(test_format_body_element_nested_list);
    
    // format_procedure_title tests
    RUN_TEST(test_format_procedure_title_no_params);
    RUN_TEST(test_format_procedure_title_one_param);
    RUN_TEST(test_format_procedure_title_multiple_params);
    
    // format_procedure_definition tests
    RUN_TEST(test_format_procedure_definition_simple);
    RUN_TEST(test_format_procedure_definition_with_params);
    RUN_TEST(test_format_procedure_definition_multiline);
    
    // format_variable tests
    RUN_TEST(test_format_variable_number);
    RUN_TEST(test_format_variable_decimal);
    RUN_TEST(test_format_variable_word);
    RUN_TEST(test_format_variable_empty_list);
    RUN_TEST(test_format_variable_list);
    RUN_TEST(test_format_variable_nested_list);
    
    // format_property tests
    RUN_TEST(test_format_property_word_value);
    RUN_TEST(test_format_property_number_value);
    RUN_TEST(test_format_property_list_value);
    
    // format_property_list tests
    RUN_TEST(test_format_property_list_single_property);
    RUN_TEST(test_format_property_list_multiple_properties);
    RUN_TEST(test_format_property_list_empty);
    
    // Simplified buffer API wrapper tests
    RUN_TEST(test_format_procedure_to_buffer_simple);
    RUN_TEST(test_format_variable_to_buffer_number);
    RUN_TEST(test_format_variable_to_buffer_word);
    RUN_TEST(test_format_property_to_buffer_word);
    RUN_TEST(test_format_property_list_to_buffer_single);
    RUN_TEST(test_format_value_to_buffer_number);
    RUN_TEST(test_format_value_to_buffer_word);
    RUN_TEST(test_format_value_to_buffer_list);
    RUN_TEST(test_format_value_show_to_buffer_list);
    RUN_TEST(test_format_to_buffer_fails_on_overflow);
    
    // Integration tests
    RUN_TEST(test_format_uses_callback);
    RUN_TEST(test_format_propagates_callback_failure);
    RUN_TEST(test_format_variable_propagates_failure);
    
    return UNITY_END();
}
