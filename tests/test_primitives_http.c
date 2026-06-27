//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for HTTP primitives: http.get, http.post, http.status, http.header
//
//  These exercise the HTTP client end-to-end against the mock TCP backend:
//  the test scripts the raw response bytes the "server" returns, and asserts
//  on the raw request the client sent plus the Logo-level result.
//

#include "test_scaffold.h"
#include <string.h>

void setUp(void)
{
    test_scaffold_setUp();
    mock_device_init();

    // Baseline: WiFi up, DNS resolves, connections succeed. Individual tests
    // override only what they care about.
    mock_device_set_wifi_connected(true);
    mock_device_set_resolve_result("93.184.216.34", true);
    mock_device_set_tcp_connect_result(true);
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

// Script the bytes the mock "server" returns for the next request.
static void script_response(const char *raw)
{
    mock_device_set_tcp_response(raw, strlen(raw));
}

// Aux region used to back the blob heap for large-body tests.
_Alignas(8) static uint8_t http_blob_region[1u << 20];

// Script a 200 response whose body is `n` bytes of 'A', then run http.get.
static Result get_body_of_size(int n)
{
    static char resp[4096];
    int off = snprintf(resp, sizeof(resp),
                       "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", n);
    memset(resp + off, 'A', (size_t)n);
    mock_device_set_tcp_response(resp, (size_t)(off + n));
    return eval_string("http.get \"http://example.com/big.txt");
}

// ============================================================================
// http.get - happy path and request framing
// ============================================================================

void test_http_get_returns_body_on_200(void)
{
    script_response(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 12\r\n"
        "\r\n"
        "Hello World!");

    Result r = eval_string("http.get \"http://example.com/menu.txt");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("Hello World!", mem_word_ptr(r.value.as.node));
}

void test_http_get_sets_status_200(void)
{
    script_response("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nhi");

    eval_string("http.get \"http://example.com/");
    Result r = eval_string("http.status");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("200", mem_word_ptr(r.value.as.node));
}

void test_http_get_request_has_get_line_and_host(void)
{
    script_response("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");

    eval_string("http.get \"http://example.com/menu.txt");

    const char *req = mock_device_get_tcp_request();
    TEST_ASSERT_NOT_NULL(strstr(req, "GET /menu.txt HTTP/1."));
    TEST_ASSERT_NOT_NULL(strstr(req, "Host: example.com"));
}

void test_http_get_root_path_when_url_has_none(void)
{
    script_response("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");

    eval_string("http.get \"http://example.com");

    const char *req = mock_device_get_tcp_request();
    TEST_ASSERT_NOT_NULL(strstr(req, "GET / HTTP/1."));
}

void test_http_get_uses_default_port_80(void)
{
    script_response("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");

    eval_string("http.get \"http://example.com/");

    TEST_ASSERT_EQUAL_UINT16(80, mock_device_get_last_tcp_port());
}

void test_http_get_uses_explicit_port(void)
{
    script_response("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");

    eval_string("http.get \"http://example.com:8080/x");

    TEST_ASSERT_EQUAL_UINT16(8080, mock_device_get_last_tcp_port());
}

void test_http_get_resolves_host_to_ip(void)
{
    mock_device_set_resolve_result("10.1.2.3", true);
    script_response("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");

    eval_string("http.get \"http://example.com/");

    TEST_ASSERT_EQUAL_STRING("10.1.2.3", mock_device_get_last_tcp_ip());
    TEST_ASSERT_EQUAL_STRING("example.com", mock_device_get_last_resolve_hostname());
}

void test_http_get_sends_custom_headers(void)
{
    script_response("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");

    eval_string("(http.get \"http://example.com/ \"Accept \"text/plain \"User-Agent \"PicoLogo)");

    const char *req = mock_device_get_tcp_request();
    TEST_ASSERT_NOT_NULL(strstr(req, "Accept: text/plain"));
    TEST_ASSERT_NOT_NULL(strstr(req, "User-Agent: PicoLogo"));
}

void test_http_get_empty_body(void)
{
    script_response("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");

    Result r = eval_string("http.get \"http://example.com/");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("", mem_word_ptr(r.value.as.node));
}

// ============================================================================
// http.get - response parsing
// ============================================================================

void test_http_get_partial_reads_reassembled(void)
{
    script_response(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 12\r\n"
        "\r\n"
        "Hello World!");
    mock_device_set_tcp_read_chunk(1);  // One byte per read

    Result r = eval_string("http.get \"http://example.com/");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("Hello World!", mem_word_ptr(r.value.as.node));
}

void test_http_get_chunked_transfer_decoding(void)
{
    script_response(
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\nHello\r\n"
        "6\r\n World\r\n"
        "0\r\n\r\n");

    Result r = eval_string("http.get \"http://example.com/");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("Hello World", mem_word_ptr(r.value.as.node));
}

void test_http_get_status_line_http_1_0(void)
{
    script_response("HTTP/1.0 200 OK\r\nContent-Length: 2\r\n\r\nhi");

    eval_string("http.get \"http://example.com/");
    Result r = eval_string("http.status");

    TEST_ASSERT_EQUAL_STRING("200", mem_word_ptr(r.value.as.node));
}

// ============================================================================
// http.get - completed-but-error responses (still RESULT_OK)
// ============================================================================

void test_http_get_404_returns_body_not_error(void)
{
    script_response(
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Length: 9\r\n"
        "\r\n"
        "Not Found");

    Result r = eval_string("http.get \"http://example.com/missing.txt");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("Not Found", mem_word_ptr(r.value.as.node));

    Result s = eval_string("http.status");
    TEST_ASSERT_EQUAL_STRING("404", mem_word_ptr(s.value.as.node));
}

// ============================================================================
// http.get - failures (RESULT_ERROR, catchable)
// ============================================================================

void test_http_get_connection_refused_errors(void)
{
    mock_device_set_tcp_connect_result(false);
    script_response("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");

    Result r = eval_string("http.get \"http://example.com/");

    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_http_get_dns_failure_errors(void)
{
    mock_device_set_resolve_result(NULL, false);

    Result r = eval_string("http.get \"http://nonexistent.invalid/");

    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_http_get_closed_midstream_errors(void)
{
    // Declares 100 bytes but the peer closes after delivering the headers.
    script_response("HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\nshort");
    mock_device_set_tcp_close_after(40);

    Result r = eval_string("http.get \"http://example.com/");

    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_http_get_requires_wifi(void)
{
    mock_device_set_wifi_connected(false);
    script_response("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");

    Result r = eval_string("http.get \"http://example.com/");

    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_http_get_large_body_becomes_blob(void)
{
    // With an aux region the transfer buffer comes from PSRAM, raising the body
    // cap well above the SRAM fallback (HTTP_MAX_BODY = 2048). A 4000-byte body
    // therefore succeeds and returns intact as a blob word.
    logo_mem_set_aux_region(http_blob_region, sizeof(http_blob_region));

    Result r = get_body_of_size(4000);

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL(4000, mem_word_len(r.value.as.node));
    TEST_ASSERT_TRUE(mem_is_blob(r.value.as.node));
}

void test_http_get_large_body_without_region_errors(void)
{
    // No aux region (reset by setUp): a body over 255 bytes cannot become a
    // word and must error rather than truncate.
    Result r = get_body_of_size(500);

    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_http_get_print_large_body(void)
{
    // End-to-end: printing a large (blob) body must not error. Mirrors the
    // interactive `pr (http.get "http://example.com)`.
    logo_mem_set_aux_region(http_blob_region, sizeof(http_blob_region));

    static char resp[4096];
    int off = snprintf(resp, sizeof(resp),
                       "HTTP/1.1 200 OK\r\nContent-Length: 500\r\n\r\n");
    memset(resp + off, 'A', 500);
    mock_device_set_tcp_response(resp, (size_t)(off + 500));

    Result r = eval_string("pr (http.get \"http://example.com)");
    TEST_ASSERT_NOT_EQUAL(RESULT_ERROR, r.status);
}

void test_http_get_rejects_https_url(void)
{
    Result r = eval_string("http.get \"https://example.com/");

    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_http_get_rejects_non_http_url(void)
{
    Result r = eval_string("http.get \"example.com/");

    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_http_get_requires_one_argument(void)
{
    Result r = eval_string("http.get");

    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_http_get_requires_word_argument(void)
{
    Result r = eval_string("http.get [http://example.com/]");

    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// ============================================================================
// http.post
// ============================================================================

void test_http_post_sends_method_and_body(void)
{
    script_response("HTTP/1.1 201 Created\r\nContent-Length: 2\r\n\r\nok");

    eval_string("http.post \"http://example.com/orders \"pierogi");

    const char *req = mock_device_get_tcp_request();
    TEST_ASSERT_NOT_NULL(strstr(req, "POST /orders HTTP/1."));
    TEST_ASSERT_NOT_NULL(strstr(req, "Content-Length: 7"));
    TEST_ASSERT_NOT_NULL(strstr(req, "pierogi"));
}

void test_http_post_list_body_joined(void)
{
    script_response("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");

    eval_string("http.post \"http://example.com/orders [pierogi fries]");

    const char *req = mock_device_get_tcp_request();
    TEST_ASSERT_NOT_NULL(strstr(req, "pierogi fries"));
}

void test_http_post_returns_response_body(void)
{
    script_response("HTTP/1.1 200 OK\r\nContent-Length: 14\r\n\r\nOrder received");

    Result r = eval_string("http.post \"http://example.com/orders \"x");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("Order received", mem_word_ptr(r.value.as.node));
}

void test_http_post_sends_custom_headers(void)
{
    script_response("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");

    eval_string("(http.post \"http://example.com/orders \"x \"Content-Type \"text/plain)");

    const char *req = mock_device_get_tcp_request();
    TEST_ASSERT_NOT_NULL(strstr(req, "Content-Type: text/plain"));
}

void test_http_post_requires_two_arguments(void)
{
    Result r = eval_string("http.post \"http://example.com/");

    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_http_get_rejects_odd_header_args(void)
{
    script_response("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");

    // A header name with no value is a malformed (odd) pair list.
    Result r = eval_string("(http.get \"http://example.com/ \"Accept)");

    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// ============================================================================
// http.status / http.header
// ============================================================================

void test_http_status_empty_before_any_request(void)
{
    Result r = eval_string("http.status");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_EQUAL(NODE_NIL, r.value.as.node);
}

void test_http_header_returns_value(void)
{
    script_response(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 2\r\n"
        "\r\nhi");

    eval_string("http.get \"http://example.com/");
    Result r = eval_string("http.header \"Content-Type");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("text/plain", mem_word_ptr(r.value.as.node));
}

void test_http_header_is_case_insensitive(void)
{
    script_response(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 2\r\n"
        "\r\nhi");

    eval_string("http.get \"http://example.com/");
    Result r = eval_string("http.header \"content-type");

    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("text/plain", mem_word_ptr(r.value.as.node));
}

void test_http_header_absent_returns_empty_list(void)
{
    script_response("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nhi");

    eval_string("http.get \"http://example.com/");
    Result r = eval_string("http.header \"X-Not-Present");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_EQUAL(NODE_NIL, r.value.as.node);
}

void test_http_metadata_replaced_by_next_request(void)
{
    script_response("HTTP/1.1 200 OK\r\nX-Tag: one\r\nContent-Length: 0\r\n\r\n");
    eval_string("http.get \"http://example.com/first");

    script_response("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
    eval_string("http.get \"http://example.com/second");

    Result s = eval_string("http.status");
    TEST_ASSERT_EQUAL_STRING("404", mem_word_ptr(s.value.as.node));

    Result h = eval_string("http.header \"X-Tag");
    TEST_ASSERT_EQUAL(VALUE_LIST, h.value.type);
    TEST_ASSERT_EQUAL(NODE_NIL, h.value.as.node);
}

// ============================================================================
// Test runner
// ============================================================================

int main(void)
{
    UNITY_BEGIN();

    // http.get - happy path and request framing
    RUN_TEST(test_http_get_returns_body_on_200);
    RUN_TEST(test_http_get_sets_status_200);
    RUN_TEST(test_http_get_request_has_get_line_and_host);
    RUN_TEST(test_http_get_root_path_when_url_has_none);
    RUN_TEST(test_http_get_uses_default_port_80);
    RUN_TEST(test_http_get_uses_explicit_port);
    RUN_TEST(test_http_get_resolves_host_to_ip);
    RUN_TEST(test_http_get_sends_custom_headers);
    RUN_TEST(test_http_get_empty_body);

    // http.get - response parsing
    RUN_TEST(test_http_get_partial_reads_reassembled);
    RUN_TEST(test_http_get_chunked_transfer_decoding);
    RUN_TEST(test_http_get_status_line_http_1_0);

    // http.get - completed-but-error responses
    RUN_TEST(test_http_get_404_returns_body_not_error);

    // http.get - failures
    RUN_TEST(test_http_get_connection_refused_errors);
    RUN_TEST(test_http_get_dns_failure_errors);
    RUN_TEST(test_http_get_closed_midstream_errors);
    RUN_TEST(test_http_get_requires_wifi);
    RUN_TEST(test_http_get_rejects_https_url);
    RUN_TEST(test_http_get_rejects_non_http_url);
    RUN_TEST(test_http_get_requires_one_argument);
    RUN_TEST(test_http_get_requires_word_argument);

    // http.post
    RUN_TEST(test_http_post_sends_method_and_body);
    RUN_TEST(test_http_post_list_body_joined);
    RUN_TEST(test_http_post_returns_response_body);
    RUN_TEST(test_http_post_sends_custom_headers);
    RUN_TEST(test_http_post_requires_two_arguments);
    RUN_TEST(test_http_get_rejects_odd_header_args);

    // http.status / http.header
    RUN_TEST(test_http_status_empty_before_any_request);
    RUN_TEST(test_http_header_returns_value);
    RUN_TEST(test_http_header_is_case_insensitive);
    RUN_TEST(test_http_header_absent_returns_empty_list);
    RUN_TEST(test_http_metadata_replaced_by_next_request);
    RUN_TEST(test_http_get_large_body_becomes_blob);
    RUN_TEST(test_http_get_large_body_without_region_errors);
    RUN_TEST(test_http_get_print_large_body);

    return UNITY_END();
}
