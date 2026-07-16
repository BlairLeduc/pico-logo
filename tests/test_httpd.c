//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the HTTP server pump and parser (core/httpd.c): request
//  completion, incremental/dribbled parsing, the auto-responses (400/408/411/
//  413/431/503), and the cs / error-unwind lifetime. The request accessors and
//  http.respond are exercised in M3. See docs/http-server-design.md §4 (M2).
//

#include "test_scaffold.h"
#include "core/httpd.h"
#include <string.h>

void setUp(void)
{
    test_scaffold_setUp();
    mock_device_init();
    mock_device_set_wifi_connected(true);  // http.listen requires a connection
    set_mock_ticks(0);
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

static void pump(int n)
{
    for (int i = 0; i < n; i++) httpd_poll();
}

// True when connection slot `index`'s recorded response starts with the given
// HTTP status code.
static bool responded(int index, int code)
{
    char prefix[24];
    int plen = snprintf(prefix, sizeof(prefix), "HTTP/1.1 %d", code);
    int len = 0;
    const char *r = mock_httpd_conn_response(index, &len);
    return len >= plen && memcmp(r, prefix, (size_t)plen) == 0;
}

// ============================================================================
// http.listen / http.unlisten
// ============================================================================

void test_listen_starts_the_server(void)
{
    Result r = eval_string("http.listen 80");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(httpd_listening());
    TEST_ASSERT_TRUE(mock_httpd_is_listening());
    TEST_ASSERT_EQUAL_UINT16(80, mock_httpd_listen_port());
}

void test_listen_errors_when_wifi_not_connected(void)
{
    mock_device_set_wifi_connected(false);
    Result r = eval_string("http.listen 80");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_FALSE(httpd_listening());
}

void test_listen_errors_when_already_listening(void)
{
    eval_string("http.listen 80");
    Result r = eval_string("http.listen 8080");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_listen_rejects_bad_port(void)
{
    TEST_ASSERT_EQUAL(RESULT_ERROR, eval_string("http.listen 0").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, eval_string("http.listen 70000").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, eval_string("http.listen 1.5").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, eval_string("http.listen \"eighty").status);
}

void test_unlisten_stops_the_server(void)
{
    eval_string("http.listen 80");
    Result r = eval_string("http.unlisten");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_FALSE(httpd_listening());
    TEST_ASSERT_FALSE(mock_httpd_is_listening());
}

void test_unlisten_when_not_listening_is_noop(void)
{
    Result r = eval_string("http.unlisten");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

// ============================================================================
// request completion
// ============================================================================

void test_whole_request_in_one_read_becomes_pending(void)
{
    eval_string("http.listen 80");
    TEST_ASSERT_FALSE(httpd_request_pending());

    const char *req = "GET /hello HTTP/1.1\r\nHost: x\r\n\r\n";
    mock_httpd_queue_connection(req, strlen(req));

    pump(3);  // accept, read+parse, (idle)
    TEST_ASSERT_TRUE(httpd_request_pending());

    Result r = eval_string("http.request?");
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_dribbled_request_becomes_pending(void)
{
    eval_string("http.listen 80");
    const char *req = "POST /x HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello";
    mock_httpd_queue_connection_ex(req, strlen(req), "1.2.3.4", 4);  // 4 bytes/read

    // Many polls: one accept then several 4-byte reads assemble the request.
    pump(20);
    TEST_ASSERT_TRUE(httpd_request_pending());
}

void test_request_with_body_waits_for_full_body(void)
{
    eval_string("http.listen 80");
    // Declares 5 body bytes but the connection only carries 3, then closes.
    const char *req = "POST /x HTTP/1.1\r\nContent-Length: 5\r\n\r\nhel";
    mock_httpd_queue_connection(req, strlen(req));

    pump(5);
    // Never completes; the peer closed mid-body -> auto 400, not pending.
    TEST_ASSERT_FALSE(httpd_request_pending());
    TEST_ASSERT_TRUE(responded(0, 400));
}

// ============================================================================
// auto-responses
// ============================================================================

void test_unanswered_request_gets_503_after_deadline(void)
{
    eval_string("http.listen 80");
    const char *req = "GET / HTTP/1.1\r\n\r\n";
    mock_httpd_queue_connection(req, strlen(req));

    pump(3);
    TEST_ASSERT_TRUE(httpd_request_pending());

    // No handler answers: after HTTPD_RESPOND_MS the pump auto-responds 503.
    set_mock_ticks(20000);
    pump(1);
    TEST_ASSERT_FALSE(httpd_request_pending());
    TEST_ASSERT_TRUE(responded(0, 503));
}

void test_oversize_header_block_gets_431(void)
{
    eval_string("http.listen 80");
    // A header block well over HTTPD_MAX_HEADERS with no terminator in range.
    static char req[4096];
    int n = snprintf(req, sizeof(req), "GET / HTTP/1.1\r\n");
    while (n < 3000) n += snprintf(req + n, sizeof(req) - n, "X-Pad: aaaaaaaaaa\r\n");
    mock_httpd_queue_connection(req, (size_t)n);

    pump(3);
    TEST_ASSERT_TRUE(responded(0, 431));
}

void test_oversize_body_gets_413(void)
{
    eval_string("http.listen 80");
    // Content-Length beyond the SRAM body cap (HTTPD_MAX_BODY 4096).
    const char *req = "POST /x HTTP/1.1\r\nContent-Length: 100000\r\n\r\n";
    mock_httpd_queue_connection(req, strlen(req));

    pump(3);
    TEST_ASSERT_TRUE(responded(0, 413));
    TEST_ASSERT_FALSE(httpd_request_pending());
}

void test_chunked_request_gets_411(void)
{
    eval_string("http.listen 80");
    const char *req = "POST /x HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n";
    mock_httpd_queue_connection(req, strlen(req));

    pump(3);
    TEST_ASSERT_TRUE(responded(0, 411));
}

void test_malformed_request_line_gets_400(void)
{
    eval_string("http.listen 80");
    const char *req = "NOSPACESHERE\r\n\r\n";  // no method/target/version
    mock_httpd_queue_connection(req, strlen(req));

    pump(3);
    TEST_ASSERT_TRUE(responded(0, 400));
}

void test_stalled_request_gets_408(void)
{
    eval_string("http.listen 80");
    // Partial headers, no terminator, and the client goes quiet (stall).
    const char *req = "GET / HTTP/1.1\r\n";
    mock_httpd_queue_connection_stalled(req, strlen(req));

    pump(3);  // accept, read the partial bytes, first empty read
    TEST_ASSERT_FALSE(httpd_request_pending());

    // After HTTPD_STALL_MS of no data, auto 408.
    set_mock_ticks(10000);
    pump(1);
    TEST_ASSERT_TRUE(responded(0, 408));
}

// ============================================================================
// lifetime: cs / reset close the listener
// ============================================================================

void test_reset_closes_the_listener(void)
{
    eval_string("http.listen 80");
    TEST_ASSERT_TRUE(httpd_listening());

    httpd_reset();
    TEST_ASSERT_FALSE(httpd_listening());
    TEST_ASSERT_FALSE(mock_httpd_is_listening());
}

void test_clearscreen_closes_the_listener(void)
{
    eval_string("http.listen 80");
    TEST_ASSERT_TRUE(httpd_listening());

    eval_string("clearscreen");
    TEST_ASSERT_FALSE(httpd_listening());
    TEST_ASSERT_FALSE(mock_httpd_is_listening());
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_listen_starts_the_server);
    RUN_TEST(test_listen_errors_when_wifi_not_connected);
    RUN_TEST(test_listen_errors_when_already_listening);
    RUN_TEST(test_listen_rejects_bad_port);
    RUN_TEST(test_unlisten_stops_the_server);
    RUN_TEST(test_unlisten_when_not_listening_is_noop);

    RUN_TEST(test_whole_request_in_one_read_becomes_pending);
    RUN_TEST(test_dribbled_request_becomes_pending);
    RUN_TEST(test_request_with_body_waits_for_full_body);

    RUN_TEST(test_unanswered_request_gets_503_after_deadline);
    RUN_TEST(test_oversize_header_block_gets_431);
    RUN_TEST(test_oversize_body_gets_413);
    RUN_TEST(test_chunked_request_gets_411);
    RUN_TEST(test_malformed_request_line_gets_400);
    RUN_TEST(test_stalled_request_gets_408);

    RUN_TEST(test_reset_closes_the_listener);
    RUN_TEST(test_clearscreen_closes_the_listener);

    return UNITY_END();
}
