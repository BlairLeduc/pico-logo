//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the TCP server device ops (network_tcp_listen / unlisten / accept)
//  and their interaction with the shared read/write/close/can-read ops. These
//  drive the mock hardware ops table directly, exactly as the httpd pump will in
//  M2. See docs/http-server-design.md §5 (M1).
//

#include "test_scaffold.h"
#include <string.h>

void setUp(void)
{
    test_scaffold_setUp();
    mock_device_init();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

// Read from a connection until it reports EOF (-1), collecting into buf.
// Returns the total number of bytes read.
static int drain(void *conn, char *buf, int cap)
{
    int total = 0;
    for (;;)
    {
        int r = mock_hardware_ops.network_tcp_read(conn, buf + total, cap - total, 0);
        if (r <= 0) break;
        total += r;
        if (total >= cap) break;
    }
    return total;
}

// ============================================================================
// listen / unlisten
// ============================================================================

void test_listen_returns_handle_and_records_port(void)
{
    void *listener = mock_hardware_ops.network_tcp_listen(80);

    TEST_ASSERT_NOT_NULL(listener);
    TEST_ASSERT_TRUE(mock_httpd_is_listening());
    TEST_ASSERT_EQUAL_UINT16(80, mock_httpd_listen_port());

    mock_hardware_ops.network_tcp_unlisten(listener);
    TEST_ASSERT_FALSE(mock_httpd_is_listening());
}

void test_second_listen_while_listening_fails(void)
{
    void *first = mock_hardware_ops.network_tcp_listen(8080);
    TEST_ASSERT_NOT_NULL(first);

    void *second = mock_hardware_ops.network_tcp_listen(9090);
    TEST_ASSERT_NULL(second);

    mock_hardware_ops.network_tcp_unlisten(first);
}

// ============================================================================
// accept
// ============================================================================

void test_accept_when_empty_returns_null(void)
{
    void *listener = mock_hardware_ops.network_tcp_listen(80);

    char ip[16] = "unset";
    void *conn = mock_hardware_ops.network_tcp_accept(listener, ip, sizeof(ip));

    TEST_ASSERT_NULL(conn);

    mock_hardware_ops.network_tcp_unlisten(listener);
}

void test_accept_returns_connection_and_remote_ip(void)
{
    void *listener = mock_hardware_ops.network_tcp_listen(80);
    const char *req = "GET / HTTP/1.1\r\n\r\n";
    mock_httpd_queue_connection_ex(req, strlen(req), "10.0.0.7", 0);

    char ip[16] = "";
    void *conn = mock_hardware_ops.network_tcp_accept(listener, ip, sizeof(ip));

    TEST_ASSERT_NOT_NULL(conn);
    TEST_ASSERT_EQUAL_STRING("10.0.0.7", ip);

    mock_hardware_ops.network_tcp_close(conn);
    mock_hardware_ops.network_tcp_unlisten(listener);
}

// ============================================================================
// listen -> accept -> read -> write -> close (the full happy path)
// ============================================================================

void test_full_connection_round_trip(void)
{
    void *listener = mock_hardware_ops.network_tcp_listen(80);
    const char *req = "GET /hi HTTP/1.1\r\nHost: x\r\n\r\n";
    mock_httpd_queue_connection(req, strlen(req));

    char ip[16];
    void *conn = mock_hardware_ops.network_tcp_accept(listener, ip, sizeof(ip));
    TEST_ASSERT_NOT_NULL(conn);

    // The request bytes are readable, then EOF.
    char got[128];
    int n = drain(conn, got, sizeof(got));
    TEST_ASSERT_EQUAL_INT((int)strlen(req), n);
    TEST_ASSERT_EQUAL_MEMORY(req, got, n);

    // The handler's response bytes are recorded on the connection.
    const char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nhi";
    int w = mock_hardware_ops.network_tcp_write(conn, resp, (int)strlen(resp));
    TEST_ASSERT_EQUAL_INT((int)strlen(resp), w);

    MockHttpdConn *c = (MockHttpdConn *)conn;
    TEST_ASSERT_EQUAL_INT((int)strlen(resp), c->response_len);
    TEST_ASSERT_EQUAL_MEMORY(resp, c->response, c->response_len);

    mock_hardware_ops.network_tcp_close(conn);
    TEST_ASSERT_FALSE(c->open);
    // Response survives the close for assertions.
    TEST_ASSERT_EQUAL_INT((int)strlen(resp), c->response_len);

    mock_hardware_ops.network_tcp_unlisten(listener);
}

void test_can_read_reflects_available_bytes(void)
{
    void *listener = mock_hardware_ops.network_tcp_listen(80);
    const char *req = "PING";
    mock_httpd_queue_connection(req, strlen(req));

    char ip[16];
    void *conn = mock_hardware_ops.network_tcp_accept(listener, ip, sizeof(ip));

    TEST_ASSERT_TRUE(mock_hardware_ops.network_tcp_can_read(conn));

    char buf[8];
    drain(conn, buf, sizeof(buf));
    TEST_ASSERT_FALSE(mock_hardware_ops.network_tcp_can_read(conn));

    mock_hardware_ops.network_tcp_close(conn);
    mock_hardware_ops.network_tcp_unlisten(listener);
}

// The pump reads incrementally; a dribbled connection returns a few bytes at a
// time so the parser's state machine is exercised.
void test_dribbled_read_delivers_in_chunks(void)
{
    void *listener = mock_hardware_ops.network_tcp_listen(80);
    const char *req = "GET / HTTP/1.1\r\n\r\n";
    mock_httpd_queue_connection_ex(req, strlen(req), "1.2.3.4", 3);  // 3 bytes/read

    char ip[16];
    void *conn = mock_hardware_ops.network_tcp_accept(listener, ip, sizeof(ip));

    // First read yields at most the dribble chunk.
    char buf[64];
    int first = mock_hardware_ops.network_tcp_read(conn, buf, sizeof(buf), 0);
    TEST_ASSERT_EQUAL_INT(3, first);

    // Draining the rest reassembles the whole request.
    int rest = drain(conn, buf + first, (int)sizeof(buf) - first);
    TEST_ASSERT_EQUAL_INT((int)strlen(req), first + rest);
    TEST_ASSERT_EQUAL_MEMORY(req, buf, first + rest);

    mock_hardware_ops.network_tcp_close(conn);
    mock_hardware_ops.network_tcp_unlisten(listener);
}

// ============================================================================
// lifetime: unlisten drops a pending (unaccepted) connection
// ============================================================================

void test_unlisten_drops_pending_connection(void)
{
    void *listener = mock_hardware_ops.network_tcp_listen(80);
    const char *req = "GET / HTTP/1.1\r\n\r\n";
    mock_httpd_queue_connection(req, strlen(req));

    // Drop the listener before accepting.
    mock_hardware_ops.network_tcp_unlisten(listener);
    TEST_ASSERT_FALSE(mock_httpd_is_listening());

    // A fresh listener sees no leftover connection.
    void *again = mock_hardware_ops.network_tcp_listen(80);
    char ip[16];
    void *conn = mock_hardware_ops.network_tcp_accept(again, ip, sizeof(ip));
    TEST_ASSERT_NULL(conn);

    mock_hardware_ops.network_tcp_unlisten(again);
}

// Two connections queued and served one after another (serial-with-backlog).
void test_two_connections_served_serially(void)
{
    void *listener = mock_hardware_ops.network_tcp_listen(80);
    mock_httpd_queue_connection("first", 5);
    mock_httpd_queue_connection("second", 6);

    char ip[16], buf[16];

    void *c1 = mock_hardware_ops.network_tcp_accept(listener, ip, sizeof(ip));
    TEST_ASSERT_NOT_NULL(c1);
    int n1 = drain(c1, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_MEMORY("first", buf, n1);
    mock_hardware_ops.network_tcp_close(c1);

    void *c2 = mock_hardware_ops.network_tcp_accept(listener, ip, sizeof(ip));
    TEST_ASSERT_NOT_NULL(c2);
    int n2 = drain(c2, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_MEMORY("second", buf, n2);
    mock_hardware_ops.network_tcp_close(c2);

    // Nothing left.
    TEST_ASSERT_NULL(mock_hardware_ops.network_tcp_accept(listener, ip, sizeof(ip)));

    mock_hardware_ops.network_tcp_unlisten(listener);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_listen_returns_handle_and_records_port);
    RUN_TEST(test_second_listen_while_listening_fails);
    RUN_TEST(test_accept_when_empty_returns_null);
    RUN_TEST(test_accept_returns_connection_and_remote_ip);
    RUN_TEST(test_full_connection_round_trip);
    RUN_TEST(test_can_read_reflects_available_bytes);
    RUN_TEST(test_dribbled_read_delivers_in_chunks);
    RUN_TEST(test_unlisten_drops_pending_connection);
    RUN_TEST(test_two_connections_served_serially);

    return UNITY_END();
}
