//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for WiFi primitives: wifi?, wifi.connect, wifi.start, wifi.status,
//  wifi.disconnect, wifi.ip, wifi.mac, wifi.ssid, wifi.scan
//

#include "test_scaffold.h"
#include "core/demons.h"

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
// tls? tests
// ============================================================================

void test_tls_supported_returns_true_when_device_has_tls(void)
{
    // The mock wires network_tls_connect, standing in for a PSRAM board.
    Result r = eval_string("tls?");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_tls_supported_returns_false_without_tls_transport(void)
{
    // A radio-but-no-PSRAM board (e.g. Pico 2 W) has no TLS transport.
    mock_hardware_ops.network_tls_connect = NULL;

    Result r = eval_string("tls?");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_tlsp_alias_works(void)
{
    mock_hardware_ops.network_tls_connect = NULL;

    Result r = eval_string("tlsp");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
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

void test_wifi_connect_errors_message_when_no_wifi_hardware(void)
{
    // A board with no radio (e.g. Pico 2) has no wifi_connect op; the error must
    // name the command.
    mock_hardware_ops.wifi_connect = NULL;

    Result r = eval_string("wifi.connect \"TestSSID \"password123");

    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL_STRING("I can't run wifi.connect on this device",
                             error_format(r));
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
// wifi.mac tests
// ============================================================================

void test_wifi_mac_returns_mac_address(void)
{
    uint8_t mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34};
    mock_device_set_wifi_mac(mac);
    
    Result r = eval_string("wifi.mac");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("DE:AD:BE:EF:12:34", mem_word_ptr(r.value.as.node));
}

void test_wifi_mac_returns_empty_list_when_unavailable(void)
{
    // Don't set a MAC - mac_available defaults to false
    
    Result r = eval_string("wifi.mac");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_wifi_mac_returns_all_zeros(void)
{
    uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
    mock_device_set_wifi_mac(mac);
    
    Result r = eval_string("wifi.mac");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("00:00:00:00:00:00", mem_word_ptr(r.value.as.node));
}

void test_wifi_mac_returns_all_255(void)
{
    uint8_t mac[6] = {255, 255, 255, 255, 255, 255};
    mock_device_set_wifi_mac(mac);
    
    Result r = eval_string("wifi.mac");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("FF:FF:FF:FF:FF:FF", mem_word_ptr(r.value.as.node));
}

void test_wifi_mac_returns_empty_list_when_no_wifi_hardware(void)
{
    // Simulate a device with no WiFi hardware. The scaffold restores the ops
    // table each setUp, so nulling an op here does not leak into later tests.
    mock_hardware_ops.wifi_get_mac = NULL;

    Result r = eval_string("wifi.mac");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
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
// hostname / sethostname tests
// ============================================================================

void test_hostname_default_is_picologo(void)
{
    Result r = eval_string("wifi.hostname");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("picologo", mem_word_ptr(r.value.as.node));
}

void test_sethostname_sets_name_and_reads_back(void)
{
    Result set = eval_string("wifi.sethostname \"picocalc");
    TEST_ASSERT_EQUAL(RESULT_NONE, set.status);

    Result r = eval_string("wifi.hostname");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("picocalc", mem_word_ptr(r.value.as.node));
}

void test_sethostname_pushes_name_to_device(void)
{
    eval_string("wifi.sethostname \"myturtle");

    // The device op received the new name (for netif/mDNS).
    TEST_ASSERT_EQUAL_STRING("myturtle", mock_device_get_hostname());
}

void test_sethostname_accepts_digits_and_interior_hyphens(void)
{
    Result r = eval_string("wifi.sethostname \"pico-logo-2");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_STRING("pico-logo-2", mock_device_get_hostname());
}

void test_sethostname_rejects_empty(void)
{
    Result r = eval_string("wifi.sethostname \"");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    // Name unchanged, device not touched.
    Result h = eval_string("wifi.hostname");
    TEST_ASSERT_EQUAL_STRING("picologo", mem_word_ptr(h.value.as.node));
}

void test_sethostname_rejects_dot(void)
{
    // The `.local` suffix is added by mDNS; a dotted name is invalid.
    Result r = eval_string("wifi.sethostname \"pico.local");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_sethostname_rejects_leading_hyphen(void)
{
    Result r = eval_string("wifi.sethostname \"-pico");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_sethostname_rejects_trailing_hyphen(void)
{
    Result r = eval_string("wifi.sethostname \"pico-");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_sethostname_rejects_too_long(void)
{
    // 33 characters, one over HOSTNAME_MAX (32).
    Result r = eval_string("wifi.sethostname \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_sethostname_accepts_exactly_max(void)
{
    // 32 characters, exactly HOSTNAME_MAX.
    Result r = eval_string("wifi.sethostname \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_sethostname_rejects_list(void)
{
    Result r = eval_string("wifi.sethostname [pico]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
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
// wifi.start / wifi.status tests
// ============================================================================

void test_wifi_status_is_off_before_any_attempt(void)
{
    Result r = eval_string("wifi.status");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("off", mem_word_ptr(r.value.as.node));
}

void test_wifi_start_returns_without_connecting(void)
{
    // The whole point: control comes back with the link still down, so a
    // startup file reaches the prompt instead of stalling for 30 seconds.
    Result r = eval_string("wifi.start \"TestSSID \"password123");

    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_FALSE(state->wifi.connected);
    TEST_ASSERT_EQUAL(WIFI_STATE_CONNECTING, state->wifi.status);
}

void test_wifi_status_reports_connecting_then_connected(void)
{
    eval_string("wifi.start \"TestSSID \"password123");

    Result r = eval_string("wifi.status");
    TEST_ASSERT_EQUAL_STRING("connecting", mem_word_ptr(r.value.as.node));

    mock_device_set_wifi_status(WIFI_STATE_CONNECTED);

    r = eval_string("wifi.status");
    TEST_ASSERT_EQUAL_STRING("connected", mem_word_ptr(r.value.as.node));

    // wifi? tracks the same link.
    r = eval_string("wifi?");
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_wifi_status_reports_failed_attempt(void)
{
    eval_string("wifi.start \"TestSSID \"badpassword");
    mock_device_set_wifi_status(WIFI_STATE_FAILED);

    Result r = eval_string("wifi.status");
    TEST_ASSERT_EQUAL_STRING("failed", mem_word_ptr(r.value.as.node));

    // A failed attempt must not read as connected.
    r = eval_string("wifi?");
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_wifi_start_errors_when_attempt_cannot_be_started(void)
{
    mock_device_set_wifi_start_result(false);

    Result r = eval_string("wifi.start \"TestSSID \"password123");

    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_wifi_start_errors_when_no_wifi_hardware(void)
{
    // A board with no radio (e.g. Pico 2) has no wifi_start op.
    mock_hardware_ops.wifi_start = NULL;

    Result r = eval_string("wifi.start \"TestSSID \"password123");

    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL_STRING("I can't run wifi.start on this device",
                             error_format(r));
}

void test_wifi_status_is_off_without_status_op(void)
{
    mock_hardware_ops.wifi_status = NULL;

    Result r = eval_string("wifi.status");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("off", mem_word_ptr(r.value.as.node));
}

void test_wifi_start_requires_two_words(void)
{
    Result r = eval_string("wifi.start \"TestSSID");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);

    r = eval_string("wifi.start [a list] \"password123");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// The motivating end-to-end case: start the radio without blocking, and let a
// demon do the follow-up work once the link is actually up.
void test_demon_fires_when_async_connect_completes(void)
{
    eval_string("make \"synced \"no");
    eval_string("wifi.start \"TestSSID \"password123");
    eval_string("when [wifi?] [make \"synced \"yes]");

    // Still associating: the demon must not fire yet.
    demons_poll();
    Result r = eval_string("thing \"synced");
    TEST_ASSERT_EQUAL_STRING("no", mem_word_ptr(r.value.as.node));

    // Link comes up in the background.
    mock_device_set_wifi_status(WIFI_STATE_CONNECTED);
    demons_poll();

    r = eval_string("thing \"synced");
    TEST_ASSERT_EQUAL_STRING("yes", mem_word_ptr(r.value.as.node));
}

void test_demon_can_observe_a_failed_connect(void)
{
    eval_string("make \"gaveup \"no");
    eval_string("wifi.start \"TestSSID \"badpassword");
    eval_string("when [equal? wifi.status \"failed] [make \"gaveup \"yes]");

    demons_poll();
    Result r = eval_string("thing \"gaveup");
    TEST_ASSERT_EQUAL_STRING("no", mem_word_ptr(r.value.as.node));

    mock_device_set_wifi_status(WIFI_STATE_FAILED);
    demons_poll();

    r = eval_string("thing \"gaveup");
    TEST_ASSERT_EQUAL_STRING("yes", mem_word_ptr(r.value.as.node));
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

    RUN_TEST(test_tls_supported_returns_true_when_device_has_tls);
    RUN_TEST(test_tls_supported_returns_false_without_tls_transport);
    RUN_TEST(test_tlsp_alias_works);
    
    // wifi.connect tests
    RUN_TEST(test_wifi_connect_succeeds);
    RUN_TEST(test_wifi_connect_fails);
    RUN_TEST(test_wifi_connect_errors_message_when_no_wifi_hardware);
    RUN_TEST(test_wifi_connect_requires_two_args);
    RUN_TEST(test_wifi_connect_requires_words);
    
    // wifi.start / wifi.status tests
    RUN_TEST(test_wifi_status_is_off_before_any_attempt);
    RUN_TEST(test_wifi_start_returns_without_connecting);
    RUN_TEST(test_wifi_status_reports_connecting_then_connected);
    RUN_TEST(test_wifi_status_reports_failed_attempt);
    RUN_TEST(test_wifi_start_errors_when_attempt_cannot_be_started);
    RUN_TEST(test_wifi_start_errors_when_no_wifi_hardware);
    RUN_TEST(test_wifi_status_is_off_without_status_op);
    RUN_TEST(test_wifi_start_requires_two_words);
    RUN_TEST(test_demon_fires_when_async_connect_completes);
    RUN_TEST(test_demon_can_observe_a_failed_connect);

    // wifi.disconnect tests
    RUN_TEST(test_wifi_disconnect_when_connected);
    RUN_TEST(test_wifi_disconnect_when_not_connected);
    
    // wifi.ip tests
    RUN_TEST(test_wifi_ip_returns_ip_when_connected);
    RUN_TEST(test_wifi_ip_returns_empty_list_when_not_connected);
    
    // wifi.mac tests
    RUN_TEST(test_wifi_mac_returns_mac_address);
    RUN_TEST(test_wifi_mac_returns_empty_list_when_unavailable);
    RUN_TEST(test_wifi_mac_returns_all_zeros);
    RUN_TEST(test_wifi_mac_returns_all_255);
    RUN_TEST(test_wifi_mac_returns_empty_list_when_no_wifi_hardware);
    
    // wifi.ssid tests
    RUN_TEST(test_wifi_ssid_returns_ssid_when_connected);
    RUN_TEST(test_wifi_ssid_returns_empty_list_when_not_connected);
    
    // wifi.scan tests
    RUN_TEST(test_wifi_scan_returns_empty_list_when_no_networks);
    RUN_TEST(test_wifi_scan_returns_networks);
    RUN_TEST(test_wifi_scan_handles_scan_error);
    
    // hostname / sethostname tests
    RUN_TEST(test_hostname_default_is_picologo);
    RUN_TEST(test_sethostname_sets_name_and_reads_back);
    RUN_TEST(test_sethostname_pushes_name_to_device);
    RUN_TEST(test_sethostname_accepts_digits_and_interior_hyphens);
    RUN_TEST(test_sethostname_rejects_empty);
    RUN_TEST(test_sethostname_rejects_dot);
    RUN_TEST(test_sethostname_rejects_leading_hyphen);
    RUN_TEST(test_sethostname_rejects_trailing_hyphen);
    RUN_TEST(test_sethostname_rejects_too_long);
    RUN_TEST(test_sethostname_accepts_exactly_max);
    RUN_TEST(test_sethostname_rejects_list);

    // Integration tests
    RUN_TEST(test_wifi_connect_then_check_status);
    RUN_TEST(test_wifi_connect_disconnect_cycle);

    return UNITY_END();
}
