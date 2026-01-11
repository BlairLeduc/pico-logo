//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for WiFi primitives: wifi?, wifi.connect, wifi.disconnect,
//  wifi.ip, wifi.ssid, wifi.scan
//

#include "test_scaffold.h"

void setUp(void)
{
    test_scaffold_setUp();
    mock_device_init();  // Initialize mock device for WiFi state
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

// ============================================================================
// wifi? tests
// ============================================================================

void test_wifi_connected_returns_false_when_not_connected(void)
{
    mock_device_set_wifi_connected(false);
    
    Result r = eval_string("wifi?");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_wifi_connected_returns_true_when_connected(void)
{
    mock_device_set_wifi_connected(true);
    mock_device_set_wifi_ssid("TestNetwork");
    mock_device_set_wifi_ip("192.168.1.50");
    
    Result r = eval_string("wifi?");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_wifip_alias_works(void)
{
    mock_device_set_wifi_connected(true);
    mock_device_set_wifi_ssid("TestNetwork");
    
    Result r = eval_string("wifip");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

// ============================================================================
// wifi.connect tests
// ============================================================================

void test_wifi_connect_succeeds(void)
{
    mock_device_set_wifi_connected(false);
    mock_device_set_wifi_connect_result(0);  // Success
    
    Result r = eval_string("wifi.connect \"TestSSID \"password123");
    
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Verify state was updated
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_TRUE(state->wifi.connected);
    TEST_ASSERT_EQUAL_STRING("TestSSID", state->wifi.ssid);
}

void test_wifi_connect_fails(void)
{
    mock_device_set_wifi_connected(false);
    mock_device_set_wifi_connect_result(1);  // Failure
    
    Result r = eval_string("wifi.connect \"TestSSID \"password123");
    
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    // Verify state was not updated
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_FALSE(state->wifi.connected);
}

void test_wifi_connect_requires_two_args(void)
{
    Result r = eval_string("wifi.connect \"TestSSID");
    
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_wifi_connect_requires_words(void)
{
    Result r = eval_string("wifi.connect [TestSSID] \"password");
    
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// ============================================================================
// wifi.disconnect tests
// ============================================================================

void test_wifi_disconnect_when_connected(void)
{
    mock_device_set_wifi_connected(true);
    mock_device_set_wifi_ssid("TestNetwork");
    mock_device_set_wifi_ip("192.168.1.50");
    
    Result r = eval_string("wifi.disconnect");
    
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Verify state was updated
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_FALSE(state->wifi.connected);
}

void test_wifi_disconnect_when_not_connected(void)
{
    mock_device_set_wifi_connected(false);
    
    Result r = eval_string("wifi.disconnect");
    
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);  // Should still succeed (no-op)
}

// ============================================================================
// wifi.ip tests
// ============================================================================

void test_wifi_ip_returns_ip_when_connected(void)
{
    mock_device_set_wifi_connected(true);
    mock_device_set_wifi_ssid("TestNetwork");
    mock_device_set_wifi_ip("10.0.0.42");
    
    Result r = eval_string("wifi.ip");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("10.0.0.42", mem_word_ptr(r.value.as.node));
}

void test_wifi_ip_returns_empty_list_when_not_connected(void)
{
    mock_device_set_wifi_connected(false);
    
    Result r = eval_string("wifi.ip");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));  // Empty list
}

// ============================================================================
// wifi.ssid tests
// ============================================================================

void test_wifi_ssid_returns_ssid_when_connected(void)
{
    mock_device_set_wifi_connected(true);
    mock_device_set_wifi_ssid("MyHomeNetwork");
    mock_device_set_wifi_ip("192.168.1.1");
    
    Result r = eval_string("wifi.ssid");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("MyHomeNetwork", mem_word_ptr(r.value.as.node));
}

void test_wifi_ssid_returns_empty_list_when_not_connected(void)
{
    mock_device_set_wifi_connected(false);
    
    Result r = eval_string("wifi.ssid");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));  // Empty list
}

// ============================================================================
// wifi.scan tests
// ============================================================================

void test_wifi_scan_returns_empty_list_when_no_networks(void)
{
    mock_device_clear_wifi_scan_results();
    
    Result r = eval_string("wifi.scan");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));  // Empty list
}

void test_wifi_scan_returns_networks(void)
{
    mock_device_clear_wifi_scan_results();
    mock_device_add_wifi_scan_result("Network1", -50);
    mock_device_add_wifi_scan_result("Network2", -70);
    
    Result r = eval_string("wifi.scan");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_FALSE(mem_is_nil(r.value.as.node));  // Not empty
    
    // The result should be a list of [ssid strength] pairs
    // Check first network
    Node list = r.value.as.node;
    Node first_pair = mem_car(list);
    TEST_ASSERT_FALSE(mem_is_nil(first_pair));
    
    Node first_ssid = mem_car(first_pair);
    TEST_ASSERT_EQUAL_STRING("Network1", mem_word_ptr(first_ssid));
    
    Node first_strength = mem_car(mem_cdr(first_pair));
    TEST_ASSERT_EQUAL_STRING("-50", mem_word_ptr(first_strength));
    
    // Check second network
    Node second_pair = mem_car(mem_cdr(list));
    Node second_ssid = mem_car(second_pair);
    TEST_ASSERT_EQUAL_STRING("Network2", mem_word_ptr(second_ssid));
}

void test_wifi_scan_handles_scan_error(void)
{
    mock_device_set_wifi_scan_result(1);  // Simulate error
    
    Result r = eval_string("wifi.scan");
    
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);  // Should fail
}

// ============================================================================
// Integration tests
// ============================================================================

void test_wifi_connect_then_check_status(void)
{
    mock_device_set_wifi_connected(false);
    mock_device_set_wifi_connect_result(0);
    
    // Connect
    Result connect_result = eval_string("wifi.connect \"TestNet \"pass123");
    TEST_ASSERT_EQUAL(RESULT_NONE, connect_result.status);
    
    // Check connected
    Result status_result = eval_string("wifi?");
    TEST_ASSERT_EQUAL(RESULT_OK, status_result.status);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(status_result.value.as.node));
    
    // Check SSID
    Result ssid_result = eval_string("wifi.ssid");
    TEST_ASSERT_EQUAL(RESULT_OK, ssid_result.status);
    TEST_ASSERT_EQUAL_STRING("TestNet", mem_word_ptr(ssid_result.value.as.node));
    
    // Check IP (default IP from mock)
    Result ip_result = eval_string("wifi.ip");
    TEST_ASSERT_EQUAL(RESULT_OK, ip_result.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, ip_result.value.type);
}

void test_wifi_connect_disconnect_cycle(void)
{
    mock_device_set_wifi_connect_result(0);
    
    // Connect
    eval_string("wifi.connect \"TestNet \"pass123");
    Result connected = eval_string("wifi?");
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(connected.value.as.node));
    
    // Disconnect
    eval_string("wifi.disconnect");
    Result disconnected = eval_string("wifi?");
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(disconnected.value.as.node));
    
    // IP should be empty list now
    Result ip = eval_string("wifi.ip");
    TEST_ASSERT_EQUAL(VALUE_LIST, ip.value.type);
    TEST_ASSERT_TRUE(mem_is_nil(ip.value.as.node));
}

// ============================================================================
// Main
// ============================================================================

int main(void)
{
    UNITY_BEGIN();
    
    // wifi? tests
    RUN_TEST(test_wifi_connected_returns_false_when_not_connected);
    RUN_TEST(test_wifi_connected_returns_true_when_connected);
    RUN_TEST(test_wifip_alias_works);
    
    // wifi.connect tests
    RUN_TEST(test_wifi_connect_succeeds);
    RUN_TEST(test_wifi_connect_fails);
    RUN_TEST(test_wifi_connect_requires_two_args);
    RUN_TEST(test_wifi_connect_requires_words);
    
    // wifi.disconnect tests
    RUN_TEST(test_wifi_disconnect_when_connected);
    RUN_TEST(test_wifi_disconnect_when_not_connected);
    
    // wifi.ip tests
    RUN_TEST(test_wifi_ip_returns_ip_when_connected);
    RUN_TEST(test_wifi_ip_returns_empty_list_when_not_connected);
    
    // wifi.ssid tests
    RUN_TEST(test_wifi_ssid_returns_ssid_when_connected);
    RUN_TEST(test_wifi_ssid_returns_empty_list_when_not_connected);
    
    // wifi.scan tests
    RUN_TEST(test_wifi_scan_returns_empty_list_when_no_networks);
    RUN_TEST(test_wifi_scan_returns_networks);
    RUN_TEST(test_wifi_scan_handles_scan_error);
    
    // Integration tests
    RUN_TEST(test_wifi_connect_then_check_status);
    RUN_TEST(test_wifi_connect_disconnect_cycle);
    
    return UNITY_END();
}
