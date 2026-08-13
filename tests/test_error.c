#include "unity.h"
#include "core/error.h"
#include "core/value.h"
#include <string.h>

void setUp(void)
{
}

void tearDown(void)
{
}

void test_error_message_returns_template(void)
{
    TEST_ASSERT_EQUAL_STRING("Number is too big", error_message(ERR_NUMBER_TOO_BIG));
    TEST_ASSERT_EQUAL_STRING("Can't divide by zero", error_message(ERR_DIVIDE_BY_ZERO));
    TEST_ASSERT_EQUAL_STRING("%s is already defined", error_message(ERR_ALREADY_DEFINED));
}

void test_error_message_returns_unknown_for_invalid_code(void)
{
    TEST_ASSERT_EQUAL_STRING("Unknown error", error_message(-1));
    TEST_ASSERT_EQUAL_STRING("Unknown error", error_message(999));
}

void test_error_format_returns_empty_for_non_error(void)
{
    Result r = result_ok(value_none());
    TEST_ASSERT_EQUAL_STRING("", error_format(r));
}

void test_error_format_doesnt_like_input(void)
{
    Result r = result_error_arg(ERR_DOESNT_LIKE_INPUT, "sum", "hello");
    
    TEST_ASSERT_EQUAL_STRING("sum doesn't like hello as input", error_format(r));
}

void test_error_format_doesnt_like_input_with_caller(void)
{
    Result r = result_error_arg(ERR_DOESNT_LIKE_INPUT, "sum", "hello");
    r = result_error_in(r, "myproc");
    
    TEST_ASSERT_EQUAL_STRING("sum doesn't like hello as input in myproc", error_format(r));
}

void test_error_format_didnt_output_to(void)
{
    Result r = result_error_arg(ERR_DIDNT_OUTPUT_TO, "print", NULL);
    r = result_error_in(r, "myproc");
    
    TEST_ASSERT_EQUAL_STRING("print didn't output to myproc", error_format(r));
}

void test_error_format_didnt_output_to_no_caller(void)
{
    Result r = result_error_arg(ERR_DIDNT_OUTPUT_TO, "print", NULL);
    
    TEST_ASSERT_EQUAL_STRING("print didn't output", error_format(r));
}

void test_error_format_too_few_items(void)
{
    Result r = result_error_arg(ERR_TOO_FEW_ITEMS, NULL, "[1 2]");
    
    TEST_ASSERT_EQUAL_STRING("Too few items in [1 2]", error_format(r));
}

void test_error_format_single_placeholder_proc(void)
{
    Result r = result_error_arg(ERR_NOT_PROCEDURE, "foo", NULL);
    
    TEST_ASSERT_EQUAL_STRING("foo isn't a procedure", error_format(r));
}

void test_error_format_single_placeholder_arg(void)
{
    Result r = result_error_arg(ERR_NOT_PROCEDURE, NULL, "foo");
    
    TEST_ASSERT_EQUAL_STRING("foo isn't a procedure", error_format(r));
}

void test_error_format_no_placeholder(void)
{
    Result r = result_error_arg(ERR_DISK_FULL, NULL, NULL);
    
    TEST_ASSERT_EQUAL_STRING("Disk full", error_format(r));
}

void test_error_format_doesnt_like_input_missing_fields(void)
{
    Result r = result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, NULL);
    // Missing proc and arg
    
    // Should strip %s placeholders
    TEST_ASSERT_EQUAL_STRING(" doesn't like  as input", error_format(r));
}

void test_error_format_didnt_output_to_missing_proc(void)
{
    Result r = result_error_arg(ERR_DIDNT_OUTPUT_TO, NULL, NULL);
    // Missing proc
    
    TEST_ASSERT_EQUAL_STRING(" didn't output to ", error_format(r));
}

void test_error_format_too_few_items_missing_arg(void)
{
    Result r = result_error_arg(ERR_TOO_FEW_ITEMS, NULL, NULL);
    // Missing arg
    
    TEST_ASSERT_EQUAL_STRING("Too few items in ", error_format(r));
}

void test_error_format_single_placeholder_missing_fields(void)
{
    Result r = result_error_arg(ERR_NOT_PROCEDURE, NULL, NULL);
    // Missing proc/arg
    
    TEST_ASSERT_EQUAL_STRING(" isn't a procedure", error_format(r));
}

void test_error_format_single_placeholder_with_caller(void)
{
    Result r = result_error_arg(ERR_NOT_PROCEDURE, "foo", NULL);
    r = result_error_in(r, "myproc");
    
    TEST_ASSERT_EQUAL_STRING("foo isn't a procedure in myproc", error_format(r));
}

void test_error_format_no_placeholder_with_caller(void)
{
    Result r = result_error_arg(ERR_DISK_FULL, NULL, NULL);
    r = result_error_in(r, "save_data");
    
    TEST_ASSERT_EQUAL_STRING("Disk full in save_data", error_format(r));
}

void test_error_format_too_few_items_with_caller(void)
{
    Result r = result_error_arg(ERR_TOO_FEW_ITEMS, NULL, "[1 2]");
    r = result_error_in(r, "my_list_proc");
    
    TEST_ASSERT_EQUAL_STRING("Too few items in [1 2] in my_list_proc", error_format(r));
}

void test_error_format_no_value_with_caller(void)
{
    Result r = result_error_arg(ERR_NO_VALUE, NULL, "x");
    r = result_error_in(r, "calculate");
    
    TEST_ASSERT_EQUAL_STRING("x has no value in calculate", error_format(r));
}

void test_error_format_divide_by_zero_with_caller(void)
{
    Result r = result_error_arg(ERR_DIVIDE_BY_ZERO, NULL, NULL);
    r = result_error_in(r, "average");
    
    TEST_ASSERT_EQUAL_STRING("Can't divide by zero in average", error_format(r));
}

void test_error_format_cant_use_toplevel(void)
{
    Result r = result_error_arg(ERR_CANT_USE_TOPLEVEL, "stop", NULL);
    
    TEST_ASSERT_EQUAL_STRING("stop can't be used at toplevel", error_format(r));
}

void test_error_format_cant_use_procedure(void)
{
    Result r = result_error_arg(ERR_CANT_USE_PROCEDURE, "to", NULL);
    
    TEST_ASSERT_EQUAL_STRING("to can't be used in a procedure", error_format(r));
}

void test_error_format_cant_from_editor(void)
{
    Result r = result_error_arg(ERR_CANT_FROM_EDITOR, "edit", NULL);
    
    TEST_ASSERT_EQUAL_STRING("Can't edit from the editor", error_format(r));
}

void test_error_format_not_found(void)
{
    Result r = result_error_arg(ERR_NOT_FOUND, NULL, "startup");
    
    TEST_ASSERT_EQUAL_STRING("startup not found", error_format(r));
}

// Test that network errors always use error_arg, even if error_proc is set
// This is important because the evaluator sets error_proc to the primitive name
void test_error_format_cant_open_network(void)
{
    // The evaluator fills the primitive name in as the error unwinds.
    Result r = result_error_arg(ERR_CANT_OPEN_NETWORK, "open", "192.168.1.100:12345");
    
    TEST_ASSERT_EQUAL_STRING("Can't open 192.168.1.100:12345", error_format(r));
}

void test_error_format_invalid_ip_port(void)
{
    // The evaluator fills the primitive name in as the error unwinds.
    Result r = result_error_arg(ERR_INVALID_IP_PORT, "open", "badhost:notaport");
    
    TEST_ASSERT_EQUAL_STRING("Invalid IP address or port badhost:notaport", error_format(r));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_error_message_returns_template);
    RUN_TEST(test_error_message_returns_unknown_for_invalid_code);
    RUN_TEST(test_error_format_returns_empty_for_non_error);
    RUN_TEST(test_error_format_doesnt_like_input);
    RUN_TEST(test_error_format_doesnt_like_input_with_caller);
    RUN_TEST(test_error_format_didnt_output_to);
    RUN_TEST(test_error_format_didnt_output_to_no_caller);
    RUN_TEST(test_error_format_too_few_items);
    RUN_TEST(test_error_format_single_placeholder_proc);
    RUN_TEST(test_error_format_single_placeholder_arg);
    RUN_TEST(test_error_format_no_placeholder);
    RUN_TEST(test_error_format_doesnt_like_input_missing_fields);
    RUN_TEST(test_error_format_didnt_output_to_missing_proc);
    RUN_TEST(test_error_format_too_few_items_missing_arg);
    RUN_TEST(test_error_format_single_placeholder_missing_fields);
    RUN_TEST(test_error_format_single_placeholder_with_caller);
    RUN_TEST(test_error_format_no_placeholder_with_caller);
    RUN_TEST(test_error_format_too_few_items_with_caller);
    RUN_TEST(test_error_format_no_value_with_caller);
    RUN_TEST(test_error_format_divide_by_zero_with_caller);
    RUN_TEST(test_error_format_cant_use_toplevel);
    RUN_TEST(test_error_format_cant_use_procedure);
    RUN_TEST(test_error_format_cant_from_editor);
    RUN_TEST(test_error_format_not_found);
    RUN_TEST(test_error_format_cant_open_network);
    RUN_TEST(test_error_format_invalid_ip_port);
    return UNITY_END();
}
