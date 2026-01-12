//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for network primitives: network.ping, network.resolve
//

#include "test_scaffold.h"

void setUp(void)
{
    test_scaffold_setUp();
    mock_device_init();  // Initialize mock device for network state
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

// ============================================================================
// network.ping tests
// ============================================================================

void test_network_ping_returns_milliseconds_on_success(void)
{
    mock_device_set_ping_result(22.413f);  // 22.413 milliseconds
    
    Result r = eval_string("network.ping \"192.168.1.1");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("22.413", mem_word_ptr(r.value.as.node));
    
    // Verify the IP address was passed correctly
    TEST_ASSERT_EQUAL_STRING("192.168.1.1", mock_device_get_last_ping_ip());
}

void test_network_ping_returns_negative_one_on_failure(void)
{
    mock_device_set_ping_result(-1.0f);  // Failure
    
    Result r = eval_string("network.ping \"10.0.0.1");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("-1", mem_word_ptr(r.value.as.node));
    
    // Verify the IP address was passed correctly
    TEST_ASSERT_EQUAL_STRING("10.0.0.1", mock_device_get_last_ping_ip());
}

void test_network_ping_requires_one_argument(void)
{
    Result r = eval_string("network.ping");
    
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_network_ping_requires_word_argument(void)
{
    mock_device_set_ping_result(10.0f);
    
    Result r = eval_string("network.ping [192.168.1.1]");
    
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_network_ping_with_localhost(void)
{
    mock_device_set_ping_result(0.985f);  // 0.985 milliseconds (very fast for localhost)
    
    Result r = eval_string("network.ping \"127.0.0.1");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("0.985", mem_word_ptr(r.value.as.node));
    TEST_ASSERT_EQUAL_STRING("127.0.0.1", mock_device_get_last_ping_ip());
}

void test_network_ping_with_zero_latency(void)
{
    mock_device_set_ping_result(0.0f);  // 0 milliseconds (sub-millisecond response)
    
    Result r = eval_string("network.ping \"192.168.1.1");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("0", mem_word_ptr(r.value.as.node));
}

void test_network_ping_with_various_ip_formats(void)
{
    mock_device_set_ping_result(12.137f);  // 12.137 milliseconds
    
    // Test standard dotted-decimal notation
    Result r = eval_string("network.ping \"8.8.8.8");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("8.8.8.8", mock_device_get_last_ping_ip());
    TEST_ASSERT_EQUAL_STRING("12.137", mem_word_ptr(r.value.as.node));
    
    // Test another valid IP
    r = eval_string("network.ping \"255.255.255.0");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("255.255.255.0", mock_device_get_last_ping_ip());
}

void test_network_ping_with_whole_milliseconds(void)
{
    mock_device_set_ping_result(100.0f);  // Exactly 100 milliseconds
    
    Result r = eval_string("network.ping \"192.168.1.1");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    // Trailing zeros should be trimmed
    TEST_ASSERT_EQUAL_STRING("100", mem_word_ptr(r.value.as.node));
}

// ============================================================================
// network.resolve tests
// ============================================================================

void test_network_resolve_returns_ip_on_success(void)
{
    mock_device_set_resolve_result("93.184.216.34", true);
    
    Result r = eval_string("network.resolve \"www.example.com");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("93.184.216.34", mem_word_ptr(r.value.as.node));
    
    // Verify the hostname was passed correctly
    TEST_ASSERT_EQUAL_STRING("www.example.com", mock_device_get_last_resolve_hostname());
}

void test_network_resolve_returns_empty_list_on_failure(void)
{
    mock_device_set_resolve_result(NULL, false);
    
    Result r = eval_string("network.resolve \"nonexistent.invalid");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_EQUAL(NODE_NIL, r.value.as.node);  // Empty list is NIL
    
    // Verify the hostname was passed correctly
    TEST_ASSERT_EQUAL_STRING("nonexistent.invalid", mock_device_get_last_resolve_hostname());
}

void test_network_resolve_requires_one_argument(void)
{
    Result r = eval_string("network.resolve");
    
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_network_resolve_requires_word_argument(void)
{
    mock_device_set_resolve_result("8.8.8.8", true);
    
    Result r = eval_string("network.resolve [google.com]");
    
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_network_resolve_with_ip_address(void)
{
    // Some resolve implementations may accept IP addresses directly
    mock_device_set_resolve_result("8.8.8.8", true);
    
    Result r = eval_string("network.resolve \"8.8.8.8");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("8.8.8.8", mem_word_ptr(r.value.as.node));
    TEST_ASSERT_EQUAL_STRING("8.8.8.8", mock_device_get_last_resolve_hostname());
}

void test_network_resolve_with_localhost(void)
{
    mock_device_set_resolve_result("127.0.0.1", true);
    
    Result r = eval_string("network.resolve \"localhost");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("127.0.0.1", mem_word_ptr(r.value.as.node));
    TEST_ASSERT_EQUAL_STRING("localhost", mock_device_get_last_resolve_hostname());
}

// ============================================================================
// network.ntp tests
// ============================================================================

void test_ntp_returns_true_on_success(void)
{
    mock_device_set_ntp_result(true);
    
    Result r = eval_string("network.ntp");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
    
    // Verify the default server was used
    TEST_ASSERT_EQUAL_STRING("pool.ntp.org", mock_device_get_last_ntp_server());
}

void test_ntp_returns_false_on_failure(void)
{
    mock_device_set_ntp_result(false);
    
    Result r = eval_string("network.ntp");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_ntp_with_custom_server(void)
{
    mock_device_set_ntp_result(true);
    
    Result r = eval_string("(network.ntp 0 \"time.google.com)");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
    
    // Verify the custom server was used
    TEST_ASSERT_EQUAL_STRING("time.google.com", mock_device_get_last_ntp_server());
}

void test_ntp_with_custom_server_failure(void)
{
    mock_device_set_ntp_result(false);
    
    Result r = eval_string("(network.ntp 0 \"invalid.server.com)");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
    
    // Verify the server was passed correctly
    TEST_ASSERT_EQUAL_STRING("invalid.server.com", mock_device_get_last_ntp_server());
}

void test_ntp_requires_number_argument_if_provided(void)
{
    mock_device_set_ntp_result(true);
    
    Result r = eval_string("(network.ntp [5])");
    
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_ntp_with_timezone_offset(void)
{
    mock_device_set_ntp_result(true);
    
    Result r = eval_string("(network.ntp -5 \"time.google.com)");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
    
    // Verify the server and timezone were passed correctly
    TEST_ASSERT_EQUAL_STRING("time.google.com", mock_device_get_last_ntp_server());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -5.0f, mock_device_get_last_ntp_timezone());
}

void test_ntp_with_fractional_timezone(void)
{
    mock_device_set_ntp_result(true);
    
    Result r = eval_string("(network.ntp 5.5 \"pool.ntp.org)");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
    
    // Verify fractional timezone (e.g., IST is UTC+5:30)
    TEST_ASSERT_EQUAL_STRING("pool.ntp.org", mock_device_get_last_ntp_server());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.5f, mock_device_get_last_ntp_timezone());
}

void test_ntp_with_timezone_only(void)
{
    mock_device_set_ntp_result(true);
    
    // With timezone only, should use default server
    Result r = eval_string("(network.ntp -5)");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("pool.ntp.org", mock_device_get_last_ntp_server());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -5.0f, mock_device_get_last_ntp_timezone());
}

void test_ntp_no_args_default_timezone_is_zero(void)
{
    mock_device_set_ntp_result(true);
    
    // With no arguments, should default to UTC (0)
    Result r = eval_string("network.ntp");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, mock_device_get_last_ntp_timezone());
}

void test_ntp_server_requires_word(void)
{
    mock_device_set_ntp_result(true);
    
    // Server as list should fail
    Result r = eval_string("(network.ntp -5 [pool.ntp.org])");
    
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// ============================================================================
// Test runner
// ============================================================================

int main(void)
{
    UNITY_BEGIN();
    
    // network.ping tests
    RUN_TEST(test_network_ping_returns_milliseconds_on_success);
    RUN_TEST(test_network_ping_returns_negative_one_on_failure);
    RUN_TEST(test_network_ping_requires_one_argument);
    RUN_TEST(test_network_ping_requires_word_argument);
    RUN_TEST(test_network_ping_with_localhost);
    RUN_TEST(test_network_ping_with_zero_latency);
    RUN_TEST(test_network_ping_with_various_ip_formats);
    RUN_TEST(test_network_ping_with_whole_milliseconds);
    
    // network.resolve tests
    RUN_TEST(test_network_resolve_returns_ip_on_success);
    RUN_TEST(test_network_resolve_returns_empty_list_on_failure);
    RUN_TEST(test_network_resolve_requires_one_argument);
    RUN_TEST(test_network_resolve_requires_word_argument);
    RUN_TEST(test_network_resolve_with_ip_address);
    RUN_TEST(test_network_resolve_with_localhost);
    
    // ntp tests
    RUN_TEST(test_ntp_returns_true_on_success);
    RUN_TEST(test_ntp_returns_false_on_failure);
    RUN_TEST(test_ntp_with_custom_server);
    RUN_TEST(test_ntp_with_custom_server_failure);
    RUN_TEST(test_ntp_requires_number_argument_if_provided);
    RUN_TEST(test_ntp_with_timezone_offset);
    RUN_TEST(test_ntp_with_fractional_timezone);
    RUN_TEST(test_ntp_with_timezone_only);
    RUN_TEST(test_ntp_no_args_default_timezone_is_zero);
    RUN_TEST(test_ntp_server_requires_word);
    
    return UNITY_END();
}
