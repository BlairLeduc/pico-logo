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
#include "test_mock_fs.h"
#include "core/httpd.h"
#include "core/demons.h"
#include "core/error.h"
#include <string.h>

void setUp(void)
{
    test_scaffold_setUp();
    mock_device_init();
    // Wire the mock filesystem too (for http.respondfile / http.savebody),
    // keeping the device console and mock hardware from the scaffold.
    mock_fs_reset();
    logo_storage_init(&mock_storage, &mock_storage_ops);
    logo_io_init(&mock_io, &mock_console, &mock_storage, &mock_hardware);
    primitives_set_io(&mock_io);
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

// NUL-terminated copy of connection slot `index`'s recorded response, for
// substring assertions (responses are text and never contain embedded NULs).
static const char *resp_str(int index)
{
    static char buf[4096];
    int len = 0;
    const char *r = mock_httpd_conn_response(index, &len);
    if (len > (int)sizeof(buf) - 1) len = (int)sizeof(buf) - 1;
    memcpy(buf, r, (size_t)len);
    buf[len] = '\0';
    return buf;
}

// Listen, queue one request, and pump until it is pending.
static void serve(const char *req)
{
    eval_string("http.listen 80");
    mock_httpd_queue_connection(req, strlen(req));
    pump(3);
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

void test_relisten_same_port_is_noop(void)
{
    eval_string("http.listen 80");
    Result r = eval_string("http.listen 80");  // idempotent
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(httpd_listening());
    TEST_ASSERT_EQUAL_UINT16(80, mock_httpd_listen_port());
}

void test_relisten_new_port_moves_listener(void)
{
    eval_string("http.listen 80");
    Result r = eval_string("http.listen 8080");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(httpd_listening());
    TEST_ASSERT_EQUAL_UINT16(8080, mock_httpd_listen_port());
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

void test_oversize_header_block_with_terminator_gets_431(void)
{
    eval_string("http.listen 80");
    // An oversize header block that DOES include the end-of-headers terminator,
    // delivered in one read. The incremental cap check is skipped once the
    // terminator is seen, so the cap must also be enforced after it is found.
    static char req[4096];
    int n = snprintf(req, sizeof(req), "GET / HTTP/1.1\r\n");
    while (n < 2000) n += snprintf(req + n, sizeof(req) - n, "X-Pad: aaaaaaaaaa\r\n");
    n += snprintf(req + n, sizeof(req) - n, "\r\n");  // terminator present
    mock_httpd_queue_connection(req, (size_t)n);

    pump(3);
    TEST_ASSERT_TRUE(responded(0, 431));
}

void test_oversize_body_fires_unread(void)
{
    eval_string("http.listen 80");
    // Content-Length beyond the body cap: the request fires with the body
    // unread rather than auto-responding 413.
    const char *req = "POST /x HTTP/1.1\r\nContent-Length: 100000\r\n\r\n";
    mock_httpd_queue_connection(req, strlen(req));

    pump(3);
    TEST_ASSERT_TRUE(httpd_request_pending());
    // http.body errors on an unread body.
    TEST_ASSERT_EQUAL(RESULT_ERROR, eval_string("http.body").status);
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

void test_clearscreen_leaves_the_listener_running(void)
{
    // Demon (and server) lifecycle is separate from turtle graphics: a
    // program that redraws with cs must not lose its HTTP server.
    eval_string("http.listen 80");
    TEST_ASSERT_TRUE(httpd_listening());

    eval_string("clearscreen");
    TEST_ASSERT_TRUE(httpd_listening());
    TEST_ASSERT_TRUE(mock_httpd_is_listening());
}

// ============================================================================
// request accessors (M3)
// ============================================================================

void test_accessors_report_request_fields(void)
{
    serve("GET /draw/line?colour=red&size=3 HTTP/1.1\r\n"
          "Host: h\r\nX-Key: swordfish\r\n\r\n");
    TEST_ASSERT_TRUE(httpd_request_pending());

    TEST_ASSERT_EQUAL_STRING("GET", mem_word_ptr(eval_string("http.method").value.as.node));
    TEST_ASSERT_EQUAL_STRING("/draw/line", mem_word_ptr(eval_string("http.path").value.as.node));
    TEST_ASSERT_EQUAL_STRING("colour=red&size=3", mem_word_ptr(eval_string("http.query").value.as.node));
    TEST_ASSERT_EQUAL_STRING("swordfish", mem_word_ptr(eval_string("http.reqheader \"X-Key").value.as.node));
    TEST_ASSERT_EQUAL_STRING("192.168.1.55", mem_word_ptr(eval_string("http.remote").value.as.node));
    // Header lookup is case-insensitive.
    TEST_ASSERT_EQUAL_STRING("swordfish", mem_word_ptr(eval_string("http.reqheader \"x-key").value.as.node));
}

void test_path_is_percent_decoded(void)
{
    serve("GET /a%20b%2Fc HTTP/1.1\r\n\r\n");
    TEST_ASSERT_EQUAL_STRING("/a b/c", mem_word_ptr(eval_string("http.path").value.as.node));
}

void test_query_empty_when_absent(void)
{
    serve("GET /x HTTP/1.1\r\n\r\n");
    Result r = eval_string("http.query");
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("", mem_word_ptr(r.value.as.node));
}

void test_body_is_reported_for_post(void)
{
    serve("POST /x HTTP/1.1\r\nContent-Length: 11\r\n\r\nhello world");
    TEST_ASSERT_EQUAL_STRING("hello world", mem_word_ptr(eval_string("http.body").value.as.node));
}

void test_reqheader_absent_is_empty_list(void)
{
    serve("GET /x HTTP/1.1\r\nHost: h\r\n\r\n");
    Result r = eval_string("http.reqheader \"X-Missing");
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_accessors_error_when_no_request(void)
{
    eval_string("http.listen 80");  // Listening, but nothing pending.
    TEST_ASSERT_EQUAL(RESULT_ERROR, eval_string("http.method").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, eval_string("http.path").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, eval_string("http.query").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, eval_string("http.body").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, eval_string("http.remote").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, eval_string("http.reqheader \"X").status);
}

// ============================================================================
// http.respond (M3)
// ============================================================================

void test_respond_writes_status_headers_and_body(void)
{
    serve("GET / HTTP/1.1\r\n\r\n");
    Result r = eval_string("http.respond 200 \"hello");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    const char *resp = resp_str(0);
    TEST_ASSERT_EQUAL_INT(0, strncmp(resp, "HTTP/1.1 200 OK\r\n", 17));
    TEST_ASSERT_NOT_NULL(strstr(resp, "Content-Type: text/plain; charset=utf-8\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(resp, "Content-Length: 5\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(resp, "Connection: close\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(resp, "\r\n\r\nhello"));

    // Responding clears the pending request.
    TEST_ASSERT_FALSE(httpd_request_pending());
}

void test_respond_content_type_override(void)
{
    serve("GET / HTTP/1.1\r\n\r\n");
    // Backslash-escapes let a word carry the HTML angle brackets literally.
    eval_string("(http.respond 200 \"\\<h1\\>hi\\</h1\\> \"Content-Type \"text/html)");

    const char *resp = resp_str(0);
    TEST_ASSERT_NOT_NULL(strstr(resp, "Content-Type: text/html\r\n"));
    // The default text/plain is not also emitted.
    TEST_ASSERT_NULL(strstr(resp, "text/plain"));
    TEST_ASSERT_NOT_NULL(strstr(resp, "\r\n\r\n<h1>hi</h1>"));
}

void test_respond_list_body_formats_like_print(void)
{
    serve("GET / HTTP/1.1\r\n\r\n");
    eval_string("http.respond 200 [heading is 90]");

    const char *resp = resp_str(0);
    // A list body loses its outer brackets, like print.
    TEST_ASSERT_NOT_NULL(strstr(resp, "Content-Length: 13\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(resp, "\r\n\r\nheading is 90"));
}

void test_respond_custom_status(void)
{
    serve("GET /secret HTTP/1.1\r\n\r\n");
    eval_string("http.respond 403 \"no");
    TEST_ASSERT_EQUAL_INT(0, strncmp(resp_str(0), "HTTP/1.1 403 Forbidden\r\n", 24));
}

void test_respond_errors_when_no_request(void)
{
    eval_string("http.listen 80");
    Result r = eval_string("http.respond 200 \"hello");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// ============================================================================
// end-to-end: a `when [http.request?]` handler
// ============================================================================

void test_demon_handler_serves_request(void)
{
    eval_string("http.listen 80");
    eval_string("when [http.request?] [http.respond 200 http.path]");
    mock_httpd_queue_connection("GET /hello HTTP/1.1\r\n\r\n",
                                strlen("GET /hello HTTP/1.1\r\n\r\n"));

    pump(3);  // request becomes pending
    TEST_ASSERT_TRUE(httpd_request_pending());

    Result r = demons_poll();  // the demon fires and responds
    TEST_ASSERT(r.status == RESULT_OK || r.status == RESULT_NONE);

    TEST_ASSERT_FALSE(httpd_request_pending());
    TEST_ASSERT_EQUAL_INT(0, strncmp(resp_str(0), "HTTP/1.1 200 OK\r\n", 17));
    TEST_ASSERT_NOT_NULL(strstr(resp_str(0), "\r\n\r\n/hello"));
}

// A handler that calls http.get mid-request must not clobber the pending
// request: the server and client keep separate buffers.
void test_client_get_does_not_disturb_pending_request(void)
{
    // Script a client response for http.get.
    mock_device_set_tcp_connect_result(true);
    const char *client_resp =
        "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nabc";
    mock_device_set_tcp_response(client_resp, strlen(client_resp));

    serve("GET /page?x=1 HTTP/1.1\r\nX-Key: k\r\n\r\n");
    TEST_ASSERT_EQUAL_STRING("/page", mem_word_ptr(eval_string("http.path").value.as.node));

    // Aggregate from another server mid-handler.
    eval_string("make \"got http.get \"http://example.com/data");

    // The pending request's fields are intact.
    TEST_ASSERT_EQUAL_STRING("/page", mem_word_ptr(eval_string("http.path").value.as.node));
    TEST_ASSERT_EQUAL_STRING("x=1", mem_word_ptr(eval_string("http.query").value.as.node));
    TEST_ASSERT_EQUAL_STRING("k", mem_word_ptr(eval_string("http.reqheader \"X-Key").value.as.node));
}

// ============================================================================
// http.element (HTML building helper)
// ============================================================================

static const char *word_of(const char *expr)
{
    return mem_word_ptr(eval_string(expr).value.as.node);
}

void test_element_wraps_content(void)
{
    TEST_ASSERT_EQUAL_STRING("<h1>Hi</h1>", word_of("http.element \"h1 \"Hi"));
}

void test_element_list_content_formats_like_print(void)
{
    TEST_ASSERT_EQUAL_STRING("<h1>Pico Logo Turtle</h1>",
                             word_of("http.element \"h1 [Pico Logo Turtle]"));
}

void test_element_with_attributes(void)
{
    TEST_ASSERT_EQUAL_STRING("<a href=/forward>forward</a>",
                             word_of("(http.element \"a \"forward \"href \"/forward)"));
}

void test_element_nests(void)
{
    TEST_ASSERT_EQUAL_STRING("<p><a href=/left>left</a></p>",
                             word_of("(http.element \"p (http.element \"a \"left \"href \"/left))"));
}

void test_element_escaped_attribute_value(void)
{
    // A backslash lets = survive the lexer inside the value word.
    TEST_ASSERT_EQUAL_STRING("<div style=margin=0>hi</div>",
                             word_of("(http.element \"div \"hi \"style \"margin\\=0)"));
}

void test_element_rejects_odd_attributes(void)
{
    Result r = eval_string("(http.element \"a \"x \"href)");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_element_rejects_non_word_tag(void)
{
    Result r = eval_string("http.element [a] \"x");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// ============================================================================
// file transfer (M5): http.respondfile / http.savebody
// ============================================================================

void test_respondfile_streams_file_as_body(void)
{
    mock_fs_create_file("page.html", "<h1>hi</h1>");
    serve("GET /page.html HTTP/1.1\r\n\r\n");

    Result r = eval_string("http.respondfile 200 \"page.html");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    const char *resp = resp_str(0);
    TEST_ASSERT_EQUAL_INT(0, strncmp(resp, "HTTP/1.1 200 OK\r\n", 17));
    // Default content type for files, and the file bytes as the body.
    TEST_ASSERT_NOT_NULL(strstr(resp, "Content-Type: application/octet-stream\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(resp, "Content-Length: 11\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(resp, "\r\n\r\n<h1>hi</h1>"));
    TEST_ASSERT_FALSE(httpd_request_pending());
}

void test_respondfile_content_type_override(void)
{
    mock_fs_create_file("page.html", "<h1>hi</h1>");
    serve("GET /page.html HTTP/1.1\r\n\r\n");
    eval_string("(http.respondfile 200 \"page.html \"Content-Type \"text/html)");

    const char *resp = resp_str(0);
    TEST_ASSERT_NOT_NULL(strstr(resp, "Content-Type: text/html\r\n"));
    TEST_ASSERT_NULL(strstr(resp, "application/octet-stream"));
}

void test_respondfile_missing_file_errors_and_stays_pending(void)
{
    serve("GET /nope HTTP/1.1\r\n\r\n");
    Result r = eval_string("http.respondfile 200 \"nope");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    // The request is left pending so the handler can respond 404 instead.
    TEST_ASSERT_TRUE(httpd_request_pending());
    TEST_ASSERT_EQUAL(RESULT_NONE, eval_string("http.respond 404 \"gone").status);
}

void test_respondfile_rejects_traversal(void)
{
    serve("GET /x HTTP/1.1\r\n\r\n");
    Result r = eval_string("http.respondfile 200 \"..\\/secret");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_savebody_writes_buffered_body(void)
{
    serve("PUT /note HTTP/1.1\r\nContent-Length: 11\r\n\r\nhello world");
    Result r = eval_string("http.savebody \"note");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    // The request stays pending for the handler to answer.
    TEST_ASSERT_TRUE(httpd_request_pending());

    MockFile *f = mock_fs_get_file("note", false);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_INT(11, (int)f->size);
    TEST_ASSERT_EQUAL_INT(0, memcmp(f->data, "hello world", 11));
}

void test_savebody_streams_unread_binary_body(void)
{
    // A body larger than the SRAM buffer cap (4096) fires unread; savebody must
    // stream it (buffered head + drained tail) to the file, binary-safe.
    const int N = 6000;
    static char req[8192];
    int hn = snprintf(req, sizeof(req),
                      "PUT /blob HTTP/1.1\r\nContent-Length: %d\r\n\r\n", N);
    // Body: a byte pattern that includes NUL bytes.
    for (int i = 0; i < N; i++) req[hn + i] = (char)((i * 7 + 3) & 0xFF);
    mock_httpd_queue_connection(req, (size_t)(hn + N));

    eval_string("http.listen 80");
    pump(3);
    TEST_ASSERT_TRUE(httpd_request_pending());
    TEST_ASSERT_EQUAL(RESULT_ERROR, eval_string("http.body").status);  // unread

    Result r = eval_string("http.savebody \"blob");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    MockFile *f = mock_fs_get_file("blob", false);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_INT(N, (int)f->size);
    TEST_ASSERT_EQUAL_INT(0, memcmp(f->data, req + hn, N));  // byte-identical
}

void test_savebody_errors_on_truncated_body(void)
{
    // The request declares an unread-sized Content-Length but the peer closes
    // after sending only part of it. savebody must report the truncation as an
    // error rather than confirming a partial upload as saved.
    const int DECLARED = 6000;
    const int SENT = 4500;  // fewer bytes than promised
    static char req[8192];
    int hn = snprintf(req, sizeof(req),
                      "PUT /blob HTTP/1.1\r\nContent-Length: %d\r\n\r\n", DECLARED);
    for (int i = 0; i < SENT; i++) req[hn + i] = (char)((i * 7 + 3) & 0xFF);
    mock_httpd_queue_connection(req, (size_t)(hn + SENT));

    eval_string("http.listen 80");
    pump(3);
    TEST_ASSERT_TRUE(httpd_request_pending());

    Result r = eval_string("http.savebody \"blob");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    // A cut-short upload is a lost connection, not a disk fault.
    TEST_ASSERT_EQUAL(ERR_LOST_CONNECTION, result_get_error_code(r));
}

// Advance the mock clock by more than the whole response deadline on each body
// read, so a handler that streams a large upload spends far longer than
// HTTPD_RESPOND_MS servicing the request.
static void slow_read_hook(void)
{
    set_mock_ticks(mock_ticks_value + HTTPD_RESPOND_MS + 1);
}

void test_slow_handler_keeps_its_connection(void)
{
    // A `when [http.request?]` handler owns the connection while it runs. Even
    // if servicing the upload takes longer than the response deadline, the pump
    // must not fire its own auto-503 and yank the connection out from under the
    // handler (which would make http.respond see the connection closed).
    const int N = 6000;  // > body cap, so it fires unread and savebody drains it
    static char req[8192];
    int hn = snprintf(req, sizeof(req),
                      "PUT /blob HTTP/1.1\r\nContent-Length: %d\r\n\r\n", N);
    for (int i = 0; i < N; i++) req[hn + i] = (char)((i * 7 + 3) & 0xFF);
    // Small reads so the drain loops several times mid-handler.
    mock_httpd_queue_connection_ex(req, (size_t)(hn + N), "192.168.1.55", 512);

    eval_string("http.listen 80");
    eval_string("when [http.request?] [http.savebody \"blob http.respond 201 [ok]]");
    pump(3);
    TEST_ASSERT_TRUE(httpd_request_pending());

    // Only now make the body drain "slow": each read jumps the clock past the
    // whole response deadline, so the handler runs far longer than
    // HTTPD_RESPOND_MS. Firing the demon runs the action on a nested evaluator
    // whose per-instruction httpd poll would, without the re-entrancy guard,
    // auto-503 the very request it is servicing.
    mock_httpd_set_read_hook(slow_read_hook);
    demons_poll();

    // The client sees the handler's 201, not an auto-503, and the whole body
    // landed on disk.
    int len = 0;
    const char *resp = mock_httpd_conn_response(0, &len);
    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_NOT_NULL(strstr(resp, "201"));
    TEST_ASSERT_NULL(strstr(resp, "503"));
    MockFile *f = mock_fs_get_file("blob", false);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_INT(N, (int)f->size);
}

void test_savebody_rejects_traversal(void)
{
    serve("PUT /x HTTP/1.1\r\nContent-Length: 2\r\n\r\nhi");
    Result r = eval_string("http.savebody \"..\\/secret");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_listen_starts_the_server);
    RUN_TEST(test_listen_errors_when_wifi_not_connected);
    RUN_TEST(test_relisten_same_port_is_noop);
    RUN_TEST(test_relisten_new_port_moves_listener);
    RUN_TEST(test_listen_rejects_bad_port);
    RUN_TEST(test_unlisten_stops_the_server);
    RUN_TEST(test_unlisten_when_not_listening_is_noop);

    RUN_TEST(test_whole_request_in_one_read_becomes_pending);
    RUN_TEST(test_dribbled_request_becomes_pending);
    RUN_TEST(test_request_with_body_waits_for_full_body);

    RUN_TEST(test_unanswered_request_gets_503_after_deadline);
    RUN_TEST(test_oversize_header_block_gets_431);
    RUN_TEST(test_oversize_header_block_with_terminator_gets_431);
    RUN_TEST(test_oversize_body_fires_unread);
    RUN_TEST(test_chunked_request_gets_411);
    RUN_TEST(test_malformed_request_line_gets_400);
    RUN_TEST(test_stalled_request_gets_408);

    RUN_TEST(test_reset_closes_the_listener);
    RUN_TEST(test_clearscreen_leaves_the_listener_running);

    RUN_TEST(test_accessors_report_request_fields);
    RUN_TEST(test_path_is_percent_decoded);
    RUN_TEST(test_query_empty_when_absent);
    RUN_TEST(test_body_is_reported_for_post);
    RUN_TEST(test_reqheader_absent_is_empty_list);
    RUN_TEST(test_accessors_error_when_no_request);

    RUN_TEST(test_respond_writes_status_headers_and_body);
    RUN_TEST(test_respond_content_type_override);
    RUN_TEST(test_respond_list_body_formats_like_print);
    RUN_TEST(test_respond_custom_status);
    RUN_TEST(test_respond_errors_when_no_request);

    RUN_TEST(test_demon_handler_serves_request);
    RUN_TEST(test_client_get_does_not_disturb_pending_request);

    RUN_TEST(test_element_wraps_content);
    RUN_TEST(test_element_list_content_formats_like_print);
    RUN_TEST(test_element_with_attributes);
    RUN_TEST(test_element_nests);
    RUN_TEST(test_element_escaped_attribute_value);
    RUN_TEST(test_element_rejects_odd_attributes);
    RUN_TEST(test_element_rejects_non_word_tag);

    RUN_TEST(test_respondfile_streams_file_as_body);
    RUN_TEST(test_respondfile_content_type_override);
    RUN_TEST(test_respondfile_missing_file_errors_and_stays_pending);
    RUN_TEST(test_respondfile_rejects_traversal);
    RUN_TEST(test_savebody_writes_buffered_body);
    RUN_TEST(test_savebody_streams_unread_binary_body);
    RUN_TEST(test_savebody_errors_on_truncated_body);
    RUN_TEST(test_slow_handler_keeps_its_connection);
    RUN_TEST(test_savebody_rejects_traversal);

    return UNITY_END();
}
