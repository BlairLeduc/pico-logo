//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for network primitives: network.ping
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
    
    return UNITY_END();
}
