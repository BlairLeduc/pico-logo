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
    Result r = {0};
    r.status = RESULT_OK;
    TEST_ASSERT_EQUAL_STRING("", error_format(r));
}

void test_error_format_doesnt_like_input(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_DOESNT_LIKE_INPUT;
    r.error_proc = "sum";
    r.error_arg = "hello";
    
    TEST_ASSERT_EQUAL_STRING("sum doesn't like hello as input", error_format(r));
}

void test_error_format_doesnt_like_input_with_caller(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_DOESNT_LIKE_INPUT;
    r.error_proc = "sum";
    r.error_arg = "hello";
    r.error_caller = "myproc";
    
    TEST_ASSERT_EQUAL_STRING("sum doesn't like hello as input in myproc", error_format(r));
}

void test_error_format_didnt_output_to(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_DIDNT_OUTPUT_TO;
    r.error_proc = "print";
    r.error_caller = "myproc";
    
    TEST_ASSERT_EQUAL_STRING("print didn't output to myproc", error_format(r));
}

void test_error_format_didnt_output_to_no_caller(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_DIDNT_OUTPUT_TO;
    r.error_proc = "print";
    
    TEST_ASSERT_EQUAL_STRING("print didn't output", error_format(r));
}

void test_error_format_too_few_items(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_TOO_FEW_ITEMS;
    r.error_arg = "[1 2]";
    
    TEST_ASSERT_EQUAL_STRING("Too few items in [1 2]", error_format(r));
}

void test_error_format_single_placeholder_proc(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_NOT_PROCEDURE;
    r.error_proc = "foo";
    
    TEST_ASSERT_EQUAL_STRING("foo isn't a procedure", error_format(r));
}

void test_error_format_single_placeholder_arg(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_NOT_PROCEDURE;
    r.error_arg = "foo";
    
    TEST_ASSERT_EQUAL_STRING("foo isn't a procedure", error_format(r));
}

void test_error_format_no_placeholder(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_DISK_FULL;
    
    TEST_ASSERT_EQUAL_STRING("Disk full", error_format(r));
}

void test_error_format_doesnt_like_input_missing_fields(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_DOESNT_LIKE_INPUT;
    // Missing proc and arg
    
    // Should return the raw template with %s
    TEST_ASSERT_EQUAL_STRING("%s doesn't like %s as input", error_format(r));
}

void test_error_format_didnt_output_to_missing_proc(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_DIDNT_OUTPUT_TO;
    // Missing proc
    
    TEST_ASSERT_EQUAL_STRING("%s didn't output to %s", error_format(r));
}

void test_error_format_too_few_items_missing_arg(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_TOO_FEW_ITEMS;
    // Missing arg
    
    TEST_ASSERT_EQUAL_STRING("Too few items in %s", error_format(r));
}

void test_error_format_single_placeholder_missing_fields(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_NOT_PROCEDURE;
    // Missing proc/arg
    
    TEST_ASSERT_EQUAL_STRING("%s isn't a procedure", error_format(r));
}

void test_error_format_single_placeholder_with_caller(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_NOT_PROCEDURE;
    r.error_proc = "foo";
    r.error_caller = "myproc";
    
    TEST_ASSERT_EQUAL_STRING("foo isn't a procedure in myproc", error_format(r));
}

void test_error_format_no_placeholder_with_caller(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_DISK_FULL;
    r.error_caller = "save_data";
    
    TEST_ASSERT_EQUAL_STRING("Disk full in save_data", error_format(r));
}

void test_error_format_too_few_items_with_caller(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_TOO_FEW_ITEMS;
    r.error_arg = "[1 2]";
    r.error_caller = "my_list_proc";
    
    TEST_ASSERT_EQUAL_STRING("Too few items in [1 2] in my_list_proc", error_format(r));
}

void test_error_format_no_value_with_caller(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_NO_VALUE;
    r.error_arg = "x";
    r.error_caller = "calculate";
    
    TEST_ASSERT_EQUAL_STRING("x has no value in calculate", error_format(r));
}

void test_error_format_divide_by_zero_with_caller(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_DIVIDE_BY_ZERO;
    r.error_caller = "average";
    
    TEST_ASSERT_EQUAL_STRING("Can't divide by zero in average", error_format(r));
}

void test_error_format_cant_use_toplevel(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_CANT_USE_TOPLEVEL;
    r.error_proc = "stop";
    
    TEST_ASSERT_EQUAL_STRING("stop can't be used at toplevel", error_format(r));
}

void test_error_format_cant_use_procedure(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_CANT_USE_PROCEDURE;
    r.error_proc = "to";
    
    TEST_ASSERT_EQUAL_STRING("to can't be used in a procedure", error_format(r));
}

void test_error_format_cant_from_editor(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_CANT_FROM_EDITOR;
    r.error_proc = "edit";
    
    TEST_ASSERT_EQUAL_STRING("Can't edit from the editor", error_format(r));
}

void test_error_format_not_found(void)
{
    Result r = {0};
    r.status = RESULT_ERROR;
    r.error_code = ERR_NOT_FOUND;
    r.error_arg = "startup";
    
    TEST_ASSERT_EQUAL_STRING("startup not found", error_format(r));
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
    return UNITY_END();
}
