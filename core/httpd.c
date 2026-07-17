//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  HTTP server pump and request parser. See docs/http-server-design.md §4/§6.
//
//  One connection at a time, fully buffered, strictly serial. Each poll does at
//  most one non-blocking accept or one bounded read; parsing is an incremental
//  state machine (request line -> headers -> Content-Length body). The pump
//  never runs Logo: it only accepts, reads, parses, and auto-responds. A
//  completed request is exposed to Logo through http.request? and answered with
//  http.respond; the pump auto-responds to malformed / stalled / oversized /
//  unanswered requests so a connection is never left hanging.
//
//  Memory: the server keeps its own request buffer, deliberately not shared
//  with the HTTP client's transfer buffer, allocated lazily on the first
//  http.listen. Same tiering as the client: an aux/PSRAM region when available
//  (large body cap), else a one-time SRAM heap fallback (small body cap).
//

#include "httpd.h"
#include "primitives.h"
#include "memory.h"
#include "limits.h"
#include "demons.h"
#include "format.h"
#include "value.h"
#include "devices/io.h"
#include "devices/stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//==========================================================================
// Request buffer (lazily allocated, tiered like the client)
//==========================================================================

#define HTTPD_IO_OVERHEAD (HTTPD_MAX_HEADERS + 8)
#define HTTPD_IO_SRAM_CAP (HTTPD_IO_OVERHEAD + HTTPD_MAX_BODY)

static char *g_buf = NULL;        // Received request bytes (headers + body)
static size_t g_buf_cap = 0;      // Capacity of g_buf
static size_t g_body_max = 0;     // Max body bytes the active buffer can hold
static char *g_buf_heap = NULL;   // Process-lifetime SRAM fallback, reused

// Choose the request buffer: PSRAM region if available (large body cap), else
// the cached SRAM heap fallback. Re-selectable after httpd_init() clears g_buf.
static void httpd_buf_init(void)
{
    if (g_buf != NULL)
    {
        return;
    }
    size_t psram_cap = HTTPD_IO_OVERHEAD + (size_t)HTTPD_MAX_BODY_PSRAM;
    char *p = (char *)mem_region_alloc(psram_cap);
    if (p != NULL)
    {
        g_buf = p;
        g_buf_cap = psram_cap;
    }
    else
    {
        if (g_buf_heap == NULL)
        {
            g_buf_heap = (char *)malloc(HTTPD_IO_SRAM_CAP);
        }
        g_buf = g_buf_heap;
        g_buf_cap = (g_buf_heap != NULL) ? HTTPD_IO_SRAM_CAP : 0;
    }
    g_body_max = (g_buf_cap > HTTPD_IO_OVERHEAD) ? (g_buf_cap - HTTPD_IO_OVERHEAD) : 0;
}

//==========================================================================
// Pump / parser state
//==========================================================================

static void *g_listener = NULL;   // Device listener handle, NULL if not listening
static uint16_t g_port = 0;       // Port g_listener is bound to
static void *g_conn = NULL;       // Current accepted connection, NULL if idle
static bool g_pending = false;    // Request fully parsed, awaiting a response

static int g_recv_len = 0;        // Bytes accumulated in g_buf
static int g_header_end = -1;     // Index of CRLFCRLF, -1 until found

// Parsed request fields (valid while g_pending, until http.respond / close).
static char g_method[HTTPD_METHOD_MAX];
static char g_path[HTTPD_PATH_MAX];  // Percent-decoded, query excluded
static int g_query_off = 0, g_query_len = 0;  // Raw query string, offsets into g_buf
static int g_hdr_off = 0, g_hdr_len = 0;      // Header lines region, offsets into g_buf
static int g_body_off = 0;                    // Body start, offset into g_buf
static int g_content_length = 0;              // Declared Content-Length (0 if none)
static bool g_body_unread = false;            // Body too large to buffer; fired unread
static char g_remote[16];

// Timing. Deltas are accumulated only across active (unfrozen) polls, so the
// stall/response deadlines pause while frozen.
static uint32_t g_last_ms = 0;
static bool g_have_last = false;
static uint32_t g_stall_ms = 0;    // Since last byte received (parsing)
static uint32_t g_pending_ms = 0;  // Since the request completed (awaiting reply)

// Poll budget baseline.
static uint32_t g_budget_ms = 0;
static bool g_have_budget = false;

//==========================================================================
// Small helpers
//==========================================================================

static LogoHardwareOps *httpd_ops(void)
{
    LogoIO *io = primitives_get_io();
    if (!io || !io->hardware || !io->hardware->ops)
    {
        return NULL;
    }
    return io->hardware->ops;
}

// Reset only the per-connection parse state (keeps the listener).
static void reset_conn_state(void)
{
    g_conn = NULL;
    g_pending = false;
    g_recv_len = 0;
    g_header_end = -1;
    g_method[0] = '\0';
    g_path[0] = '\0';
    g_query_off = g_query_len = 0;
    g_hdr_off = g_hdr_len = 0;
    g_body_off = 0;
    g_content_length = 0;
    g_body_unread = false;
    g_remote[0] = '\0';
    g_stall_ms = 0;
    g_pending_ms = 0;
}

// Write all `len` bytes to the current connection, looping over short writes.
static void write_all(const char *data, int len)
{
    LogoHardwareOps *ops = httpd_ops();
    if (!ops || !ops->network_tcp_write || !g_conn)
    {
        return;
    }
    int sent = 0;
    while (sent < len)
    {
        int w = ops->network_tcp_write(g_conn, data + sent, len - sent);
        if (w <= 0)
        {
            break;  // Peer gone; the close below discards the rest.
        }
        sent += w;
    }
}

// Close the current connection (if any) and clear the parse state.
static void close_conn(void)
{
    LogoHardwareOps *ops = httpd_ops();
    if (g_conn && ops && ops->network_tcp_close)
    {
        ops->network_tcp_close(g_conn);
    }
    reset_conn_state();
}

// Send a minimal `code reason` response with a short text body, then close.
// Used for every pump-generated auto-response.
static void send_status(int code, const char *reason)
{
    char msg[64];
    int body_len = snprintf(msg, sizeof(msg), "%d %s\n", code, reason);
    if (body_len < 0) body_len = 0;

    char head[192];
    int n = snprintf(head, sizeof(head),
                     "HTTP/1.1 %d %s\r\n"
                     "Content-Type: text/plain; charset=utf-8\r\n"
                     "Content-Length: %d\r\n"
                     "Connection: close\r\n\r\n",
                     code, reason, body_len);
    if (n > 0)
    {
        write_all(head, n);
        write_all(msg, body_len);
    }
    close_conn();
}

//==========================================================================
// Header scanning (region-based; g_buf is not NUL-terminated)
//==========================================================================

// ASCII case-insensitive compare of exactly n bytes.
static bool ci_eq(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
        if (ca != cb) return false;
    }
    return true;
}

// Find header `name` within the header-lines region [off, off+len). On success
// writes the trimmed value span (pointer into g_buf) and returns true.
static bool header_find(int off, int len, const char *name,
                        const char **val, int *vlen)
{
    size_t name_len = strlen(name);
    int i = off;
    int end = off + len;
    while (i < end)
    {
        // Line runs until its CRLF, or the region end when the last header line's
        // CRLF coincides with the end-of-headers terminator (just past `end`).
        int ls = i;
        while (i < end && !(g_buf[i] == '\r' && i + 1 < end && g_buf[i + 1] == '\n')) i++;
        int le = i;  // one past the last content byte of this line
        // Advance i past the CRLF for the next iteration.
        if (i + 1 < end && g_buf[i] == '\r' && g_buf[i + 1] == '\n') i += 2;
        else i = end;

        const char *colon = memchr(g_buf + ls, ':', (size_t)(le - ls));
        if (!colon) continue;
        size_t hn = (size_t)(colon - (g_buf + ls));
        if (hn != name_len || !ci_eq(g_buf + ls, name, name_len)) continue;

        const char *v = colon + 1;
        const char *ve = g_buf + le;
        while (v < ve && (*v == ' ' || *v == '\t')) v++;
        while (ve > v && (ve[-1] == ' ' || ve[-1] == '\t')) ve--;
        *val = v;
        *vlen = (int)(ve - v);
        return true;
    }
    return false;
}

//==========================================================================
// Request-line parsing
//==========================================================================

// Percent-decode `src` (length slen) into dst (capacity dcap, NUL-terminated).
// Returns false if the result would not fit.
static bool percent_decode(const char *src, int slen, char *dst, int dcap)
{
    int o = 0;
    for (int i = 0; i < slen; i++)
    {
        if (o + 1 >= dcap) return false;
        char c = src[i];
        if (c == '%' && i + 2 < slen)
        {
            char h = src[i + 1], l = src[i + 2];
            int hv = (h >= '0' && h <= '9') ? h - '0'
                   : (h >= 'a' && h <= 'f') ? h - 'a' + 10
                   : (h >= 'A' && h <= 'F') ? h - 'A' + 10 : -1;
            int lv = (l >= '0' && l <= '9') ? l - '0'
                   : (l >= 'a' && l <= 'f') ? l - 'a' + 10
                   : (l >= 'A' && l <= 'F') ? l - 'A' + 10 : -1;
            if (hv >= 0 && lv >= 0)
            {
                dst[o++] = (char)((hv << 4) | lv);
                i += 2;
                continue;
            }
        }
        dst[o++] = c;
    }
    dst[o] = '\0';
    return true;
}

// Parse the request line and record the header-lines region. Returns 0 on
// success, or an HTTP status code to auto-respond with on failure.
static int parse_request_line(void)
{
    // Request line ends at the first CRLF (which exists: header_end was found).
    int rl_end = 0;
    while (rl_end + 1 < g_recv_len &&
           !(g_buf[rl_end] == '\r' && g_buf[rl_end + 1] == '\n'))
    {
        rl_end++;
    }
    const char *line = g_buf;
    int line_len = rl_end;

    // Method: up to the first space.
    const char *sp1 = memchr(line, ' ', (size_t)line_len);
    if (!sp1) return 400;
    int mlen = (int)(sp1 - line);
    if (mlen == 0 || mlen >= HTTPD_METHOD_MAX) return 400;
    memcpy(g_method, line, (size_t)mlen);
    g_method[mlen] = '\0';

    // Target: up to the next space (which must exist -> a version follows).
    const char *tstart = sp1 + 1;
    int rem = line_len - (int)(tstart - line);
    const char *sp2 = memchr(tstart, ' ', (size_t)rem);
    if (!sp2) return 400;
    int tlen = (int)(sp2 - tstart);
    if (tlen == 0 || tstart[0] != '/') return 400;

    // Version: require "HTTP/" after the second space.
    const char *ver = sp2 + 1;
    int vlen = line_len - (int)(ver - line);
    if (vlen < 5 || strncmp(ver, "HTTP/", 5) != 0) return 400;

    // Split the target into path and (raw) query at the first '?'.
    const char *q = memchr(tstart, '?', (size_t)tlen);
    int path_len = q ? (int)(q - tstart) : tlen;
    if (q)
    {
        g_query_off = (int)((q + 1) - g_buf);
        g_query_len = tlen - path_len - 1;
    }
    else
    {
        g_query_off = 0;
        g_query_len = 0;
    }

    if (!percent_decode(tstart, path_len, g_path, HTTPD_PATH_MAX))
    {
        return 414;  // URI too long for the path buffer.
    }

    // Header lines occupy [rl_end + 2, g_header_end).
    g_hdr_off = rl_end + 2;
    g_hdr_len = g_header_end - g_hdr_off;
    if (g_hdr_len < 0) g_hdr_len = 0;
    return 0;
}

// Inspect Transfer-Encoding / Content-Length. Returns 0 and sets the body
// region on success, or an HTTP status code to auto-respond with.
static int parse_framing(void)
{
    const char *val;
    int vl;

    // Chunked request bodies are rejected (411): keeps the state machine small.
    if (header_find(g_hdr_off, g_hdr_len, "Transfer-Encoding", &val, &vl))
    {
        for (int i = 0; i + 7 <= vl; i++)
        {
            if (ci_eq(val + i, "chunked", 7)) return 411;
        }
    }

    g_body_off = g_header_end + 4;
    g_content_length = 0;
    g_body_unread = false;

    if (header_find(g_hdr_off, g_hdr_len, "Content-Length", &val, &vl))
    {
        long cl = 0;
        bool any = false;
        for (int i = 0; i < vl; i++)
        {
            char c = val[i];
            if (c == ' ' || c == '\t') continue;
            if (c < '0' || c > '9') return 400;
            cl = cl * 10 + (c - '0');
            any = true;
            if (cl > 0x7fffffffL) return 400;  // absurd length
        }
        if (!any) return 400;
        g_content_length = (int)cl;
        // Too large to buffer: fire the request with the body left in the
        // socket. http.body will error; http.savebody streams it to a file.
        if (cl > (long)g_body_max) g_body_unread = true;
    }
    return 0;
}

//==========================================================================
// Pump
//==========================================================================

// The request is fully received: expose it to Logo.
static void mark_complete(void)
{
    g_pending = true;
    g_pending_ms = 0;
}

// Advance parsing after new bytes have arrived. May complete the request or
// auto-respond+close on a malformed / oversized request.
static void parse_progress(void)
{
    if (g_header_end < 0)
    {
        // Search for the end-of-headers terminator.
        for (int i = 0; i + 3 < g_recv_len; i++)
        {
            if (g_buf[i] == '\r' && g_buf[i + 1] == '\n' &&
                g_buf[i + 2] == '\r' && g_buf[i + 3] == '\n')
            {
                g_header_end = i;
                break;
            }
        }
        if (g_header_end < 0)
        {
            if (g_recv_len > HTTPD_MAX_HEADERS)
            {
                send_status(431, "Request Header Fields Too Large");
            }
            return;  // Need more header bytes.
        }

        int code = parse_request_line();
        if (code == 0) code = parse_framing();
        if (code != 0)
        {
            const char *reason =
                code == 411 ? "Length Required" :
                code == 414 ? "URI Too Long" : "Bad Request";
            send_status(code, reason);
            return;
        }
    }

    // Body: an oversized body fires unread (streamed later by http.savebody);
    // otherwise wait until the whole Content-Length has arrived.
    if (!g_body_unread && g_content_length > 0)
    {
        int have = g_recv_len - g_body_off;
        if (have < g_content_length) return;  // Need more body bytes.
    }
    mark_complete();
}

void httpd_poll(void)
{
    // Advance the timing baseline every poll (including frozen ones) so frozen
    // intervals are excluded from the stall/response deadlines.
    LogoIO *io = primitives_get_io();
    uint32_t now = io ? logo_io_ticks_ms(io) : 0;
    uint32_t delta = g_have_last ? (now - g_last_ms) : 0;
    g_last_ms = now;
    g_have_last = true;

    if (!g_listener) return;
    if (demons_frozen()) return;

    LogoHardwareOps *ops = httpd_ops();
    if (!ops) return;

    if (g_conn == NULL)
    {
        // Accept one connection (non-blocking).
        if (!ops->network_tcp_accept) return;
        char ip[16] = {0};
        void *c = ops->network_tcp_accept(g_listener, ip, sizeof(ip));
        if (!c) return;
        reset_conn_state();
        g_conn = c;
        strncpy(g_remote, ip, sizeof(g_remote) - 1);
        g_remote[sizeof(g_remote) - 1] = '\0';
        return;  // Read on the next poll.
    }

    if (g_pending)
    {
        // Awaiting a response: age the deadline, auto-503 if nobody answers.
        g_pending_ms += delta;
        if (g_pending_ms >= HTTPD_RESPOND_MS)
        {
            send_status(503, "Service Unavailable");
        }
        return;
    }

    // Still parsing: one bounded, non-blocking read.
    if (!ops->network_tcp_read || !g_buf) return;
    int space = (int)g_buf_cap - g_recv_len;
    if (space <= 0)
    {
        // Should not happen: header/body caps are enforced during parsing.
        send_status(400, "Bad Request");
        return;
    }
    int r = ops->network_tcp_read(g_conn, g_buf + g_recv_len, space, 0);
    if (r == 0)
    {
        // No data yet: age the stall timer.
        g_stall_ms += delta;
        if (g_stall_ms >= HTTPD_STALL_MS)
        {
            send_status(408, "Request Timeout");
        }
        return;
    }
    if (r < 0)
    {
        // Peer closed. If nothing arrived, just drop; otherwise it's malformed.
        if (g_recv_len == 0) close_conn();
        else send_status(400, "Bad Request");
        return;
    }

    g_recv_len += r;
    g_stall_ms = 0;
    parse_progress();
}

void httpd_maybe_poll(void)
{
    LogoIO *io = primitives_get_io();
    if (!io) return;

    // Without a clock there's no budget baseline: poll every time.
    if (!logo_io_has_ticks_ms(io))
    {
        httpd_poll();
        return;
    }

    uint32_t now = logo_io_ticks_ms(io);
    if (g_have_budget && (uint32_t)(now - g_budget_ms) < HTTPD_POLL_MS)
    {
        return;
    }
    g_budget_ms = now;
    g_have_budget = true;
    httpd_poll();
}

//==========================================================================
// Listen / lifetime
//==========================================================================

Result httpd_listen(uint16_t port)
{
    LogoHardwareOps *ops = httpd_ops();
    if (!ops || !ops->network_tcp_listen || !ops->network_tcp_accept)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }
    // Require a live WiFi connection when the device reports one.
    if (ops->wifi_is_connected && !ops->wifi_is_connected())
    {
        return result_error_arg(ERR_CANT_OPEN_NETWORK, NULL, "http server");
    }

    // Idempotent: re-listening on the current port is a no-op, so a program can
    // rerun without an "already listening" error; a different port moves the
    // listener (dropping any connection in progress).
    if (g_listener != NULL)
    {
        if (g_port == port)
        {
            return result_none();
        }
        httpd_unlisten();
    }

    httpd_buf_init();
    if (g_buf == NULL)
    {
        return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
    }

    void *l = ops->network_tcp_listen(port);
    if (!l)
    {
        return result_error_arg(ERR_CANT_OPEN_NETWORK, NULL, "http server");
    }
    g_listener = l;
    g_port = port;
    reset_conn_state();
    g_have_last = false;
    g_have_budget = false;
    return result_none();
}

void httpd_unlisten(void)
{
    LogoHardwareOps *ops = httpd_ops();
    if (g_conn && ops && ops->network_tcp_close)
    {
        ops->network_tcp_close(g_conn);
    }
    if (g_listener && ops && ops->network_tcp_unlisten)
    {
        ops->network_tcp_unlisten(g_listener);
    }
    g_listener = NULL;
    g_port = 0;
    reset_conn_state();
}

bool httpd_listening(void)
{
    return g_listener != NULL;
}

bool httpd_request_pending(void)
{
    return g_pending;
}

//==========================================================================
// Request accessors (valid only while a request is pending)
//==========================================================================

const char *httpd_method(void)
{
    return g_pending ? g_method : NULL;
}

const char *httpd_path(void)
{
    return g_pending ? g_path : NULL;
}

const char *httpd_query(int *len_out)
{
    if (!g_pending)
    {
        if (len_out) *len_out = 0;
        return NULL;
    }
    if (len_out) *len_out = g_query_len;
    return g_buf + g_query_off;
}

const char *httpd_body(int *len_out)
{
    // NULL when there is no request or the body fired unread (too large to
    // buffer); the caller distinguishes via httpd_body_unread().
    if (!g_pending || g_body_unread)
    {
        if (len_out) *len_out = 0;
        return NULL;
    }
    if (len_out) *len_out = g_content_length;
    return g_buf + g_body_off;
}

bool httpd_body_unread(void)
{
    return g_pending && g_body_unread;
}

bool httpd_reqheader(const char *name, const char **val, int *len_out)
{
    if (!g_pending) return false;
    return header_find(g_hdr_off, g_hdr_len, name, val, len_out);
}

const char *httpd_remote(void)
{
    return g_pending ? g_remote : NULL;
}

//==========================================================================
// Response (http.respond)
//==========================================================================

// Reason phrase for a status code: a few common ones, else a class default.
static const char *reason_phrase(int code)
{
    switch (code)
    {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 304: return "Not Modified";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 500: return "Internal Server Error";
    }
    if (code >= 200 && code < 300) return "OK";
    if (code >= 300 && code < 400) return "Redirect";
    if (code >= 400 && code < 500) return "Client Error";
    return "Server Error";
}

// FormatOutputFunc that counts output bytes.
static bool count_bytes(void *ctx, const char *str)
{
    *(size_t *)ctx += strlen(str);
    return true;
}

// FormatOutputFunc that streams output straight to the connection.
static bool write_chunk(void *ctx, const char *str)
{
    (void)ctx;
    write_all(str, (int)strlen(str));
    return true;
}

static void write_cstr(const char *s)
{
    write_all(s, (int)strlen(s));
}

// Reject CR/LF in a header name or value (header injection).
static bool token_safe(const char *s, int len)
{
    for (int i = 0; i < len; i++)
        if (s[i] == '\r' || s[i] == '\n') return false;
    return true;
}

// Validate header pairs for CR/LF injection and note whether Content-Type is
// overridden. Returns an error result on a bad token, else result_none().
static Result check_response_headers(int hdr_argc, Value *hdr_args, bool *have_ctype)
{
    *have_ctype = false;
    for (int i = 0; i + 1 < hdr_argc; i += 2)
    {
        const char *nm = mem_word_ptr(hdr_args[i].as.node);
        const char *vl = mem_word_ptr(hdr_args[i + 1].as.node);
        int nl = (int)mem_word_len(hdr_args[i].as.node);
        int vll = (int)mem_word_len(hdr_args[i + 1].as.node);
        if (!token_safe(nm, nl) || !token_safe(vl, vll))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, NULL);
        }
        if ((size_t)nl == strlen("Content-Type") && ci_eq(nm, "Content-Type", nl))
        {
            *have_ctype = true;
        }
    }
    return result_none();
}

// Write the status line, Content-Type (default unless overridden), the caller's
// extra headers (minus the framing ones we own), then Content-Length and
// Connection: close and the blank line. Shared by http.respond/http.respondfile.
static void write_response_head(int status, const char *default_ctype,
                                long content_length, int hdr_argc, Value *hdr_args,
                                bool have_ctype)
{
    char line[64];
    int n = snprintf(line, sizeof(line), "HTTP/1.1 %d %s\r\n",
                     status, reason_phrase(status));
    write_all(line, n);

    if (!have_ctype)
    {
        write_cstr("Content-Type: ");
        write_cstr(default_ctype);
        write_cstr("\r\n");
    }

    for (int i = 0; i + 1 < hdr_argc; i += 2)
    {
        const char *nm = mem_word_ptr(hdr_args[i].as.node);
        int nl = (int)mem_word_len(hdr_args[i].as.node);
        if (((size_t)nl == strlen("Content-Length") && ci_eq(nm, "Content-Length", nl)) ||
            ((size_t)nl == strlen("Connection") && ci_eq(nm, "Connection", nl)))
        {
            continue;
        }
        write_all(nm, nl);
        write_cstr(": ");
        write_all(mem_word_ptr(hdr_args[i + 1].as.node),
                  (int)mem_word_len(hdr_args[i + 1].as.node));
        write_cstr("\r\n");
    }

    char framing[64];
    n = snprintf(framing, sizeof(framing),
                 "Content-Length: %ld\r\nConnection: close\r\n\r\n", content_length);
    write_all(framing, n);
}

// Reject a path containing a ".." segment (directory traversal); http.path is
// attacker-supplied.
static bool path_is_safe(const char *p)
{
    const char *seg = p;
    while (*seg)
    {
        const char *slash = strchr(seg, '/');
        size_t len = slash ? (size_t)(slash - seg) : strlen(seg);
        if (len == 2 && seg[0] == '.' && seg[1] == '.') return false;
        if (!slash) break;
        seg = slash + 1;
    }
    return true;
}

Result httpd_respond(int status, Value body, int hdr_argc, Value *hdr_args)
{
    if (!g_pending)
    {
        return result_error_arg(ERR_NETWORK_NOT_OPEN, NULL, NULL);
    }
    if (status < 100 || status > 599)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, NULL);
    }
    bool have_ctype;
    Result hc = check_response_headers(hdr_argc, hdr_args, &have_ctype);
    if (hc.status == RESULT_ERROR) return hc;

    size_t body_len = 0;
    format_value(count_bytes, &body_len, body);

    write_response_head(status, "text/plain; charset=utf-8", (long)body_len,
                        hdr_argc, hdr_args, have_ctype);

    // Stream the body straight from the Logo value.
    format_value(write_chunk, NULL, body);

    close_conn();
    return result_none();
}

Result httpd_respondfile(int status, const char *path, int hdr_argc, Value *hdr_args)
{
    if (!g_pending)
    {
        return result_error_arg(ERR_NETWORK_NOT_OPEN, NULL, NULL);
    }
    if (status < 100 || status > 599)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, NULL);
    }
    if (!path_is_safe(path))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, path);
    }
    bool have_ctype;
    Result hc = check_response_headers(hdr_argc, hdr_args, &have_ctype);
    if (hc.status == RESULT_ERROR) return hc;

    LogoIO *io = primitives_get_io();
    if (!io || !io->storage)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }
    // A missing file is an ordinary error; the request stays pending so the
    // handler can decide to respond 404.
    if (!logo_io_file_exists(io, path))
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, NULL, path);
    }
    long size = logo_io_file_size(io, path);
    if (size < 0)
    {
        return result_error(ERR_DISK_TROUBLE);
    }
    LogoStream *f = logo_io_open(io, path);
    if (!f)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    write_response_head(status, "application/octet-stream", size,
                        hdr_argc, hdr_args, have_ctype);

    // Stream the file to the connection in chunks (binary-safe).
    char chunk[HTTPD_CHUNK_MAX];
    long remaining = size;
    while (remaining > 0)
    {
        int want = remaining < (long)sizeof(chunk) ? (int)remaining : (int)sizeof(chunk);
        int r = logo_stream_read_chars(f, chunk, want);
        if (r <= 0) break;
        write_all(chunk, r);
        remaining -= r;
    }
    logo_io_close(io, path);

    close_conn();
    return result_none();
}

Result httpd_savebody(const char *path)
{
    if (!g_pending)
    {
        return result_error_arg(ERR_NETWORK_NOT_OPEN, NULL, NULL);
    }
    if (!path_is_safe(path))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, path);
    }

    LogoIO *io = primitives_get_io();
    if (!io || !io->storage)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Start a fresh file: the storage open is RDWR|CREAT (not truncating), so
    // delete any existing file first to avoid a shorter body leaving an old tail.
    if (logo_io_file_exists(io, path))
    {
        logo_io_file_delete(io, path);
    }
    LogoStream *f = logo_io_open(io, path);
    if (!f)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    bool ok = true;

    // 1) Body bytes already buffered alongside the headers.
    int buffered = g_recv_len - g_body_off;
    if (buffered < 0) buffered = 0;
    if (buffered > g_content_length) buffered = g_content_length;
    if (buffered > 0)
    {
        logo_stream_write_bytes(f, g_buf + g_body_off, (size_t)buffered);
        if (f->write_error) ok = false;
    }

    // 2) If the body fired unread, drain the rest from the socket straight to the
    //    file, up to the declared Content-Length.
    if (ok && g_body_unread)
    {
        LogoHardwareOps *ops = httpd_ops();
        long remaining = (long)g_content_length - buffered;
        int timeout = io->network_timeout;
        char chunk[HTTPD_CHUNK_MAX];
        while (remaining > 0 && ops && ops->network_tcp_read && g_conn)
        {
            int want = remaining < (long)sizeof(chunk) ? (int)remaining : (int)sizeof(chunk);
            int r = ops->network_tcp_read(g_conn, chunk, want, timeout);
            if (r <= 0) break;  // peer closed or timed out
            logo_stream_write_bytes(f, chunk, (size_t)r);
            if (f->write_error) { ok = false; break; }
            remaining -= r;
        }
        // A short read (peer closed or timed out) leaves the file truncated;
        // report that rather than confirming a partial upload as saved.
        if (remaining > 0) ok = false;
    }

    logo_io_close(io, path);
    if (!ok) return result_error(ERR_DISK_TROUBLE);
    return result_none();
}

void httpd_reset(void)
{
    httpd_unlisten();
    g_have_last = false;
    g_have_budget = false;
    // The request buffer stays allocated (reused); re-selected only at init.
}
