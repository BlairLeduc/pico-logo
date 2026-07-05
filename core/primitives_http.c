//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  HTTP primitives: http.get, http.post, http.put, http.patch, http.delete,
//  http.status, http.header
//
//  A high-level, blocking HTTP/1.1 client built on the device TCP ops. Each
//  request operation opens a connection, sends the request, reads the whole
//  response, closes the connection, and outputs the response body as a word.
//  Status code and response headers of the most recently completed request are
//  retained for http.status / http.header.
//
//  Both http:// and https:// URLs are supported. https:// requests are opened
//  through the device's TLS connect op (network_tls_connect); the transport is
//  otherwise opaque, so request building and response parsing are shared. See
//  the "HTTP Operations" section of reference/Pico_Logo_Reference.md for the
//  language-level contract.
//
//  Memory: the RP2350's SRAM is largely spoken for (Logo arena, LCD frame
//  buffer, operand stack), so this client keeps a tiny static footprint -- a
//  single shared transfer buffer reused for the request and the response, plus
//  one small buffer holding the most recent response headers.
//

#include "primitives.h"
#include "memory.h"
#include "value.h"
#include "format.h"
#include "limits.h"
#include "devices/io.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

//==========================================================================
// Static buffers
//==========================================================================

// One scratch buffer holds the request while it is built and sent, then is
// reused to receive the response (the request bytes are no longer needed once
// network_tcp_write returns). It is allocated from the aux/PSRAM region when
// available (raising the body cap to HTTP_MAX_BODY_PSRAM); otherwise it falls
// back to a one-time SRAM heap buffer (cap HTTP_MAX_BODY). It is deliberately
// not a static array so PSRAM-backed builds do not reserve the SRAM.
#define HTTP_IO_OVERHEAD (HTTP_MAX_HEADERS + 64)
#define HTTP_IO_SRAM_CAP (HTTP_IO_OVERHEAD + HTTP_MAX_BODY)
static char *g_io = NULL;        // active transfer buffer
static size_t g_io_cap = 0;      // capacity of g_io
static size_t g_body_max = 0;    // max body bytes the active buffer can hold
static char *g_io_heap = NULL;   // process-lifetime SRAM fallback, reused

// Choose the transfer buffer: PSRAM if a region is available, else the cached
// SRAM heap fallback. Re-selectable after primitives_http_init() clears g_io.
static void http_io_init(void)
{
    if (g_io != NULL)
    {
        return;
    }
    size_t psram_cap = HTTP_IO_OVERHEAD + (size_t)HTTP_MAX_BODY_PSRAM;
    char *p = (char *)mem_region_alloc(psram_cap);
    if (p != NULL)
    {
        g_io = p;
        g_io_cap = psram_cap;
    }
    else
    {
        if (g_io_heap == NULL)
        {
            g_io_heap = (char *)malloc(HTTP_IO_SRAM_CAP);
        }
        g_io = g_io_heap;
        g_io_cap = (g_io_heap != NULL) ? HTTP_IO_SRAM_CAP : 0;
    }
    g_body_max = (g_io_cap > HTTP_IO_OVERHEAD) ? (g_io_cap - HTTP_IO_OVERHEAD) : 0;
}

// Most-recent-response state (for http.status / http.header).
static bool g_has_response = false;
static int g_status_code = 0;
static char g_headers[HTTP_MAX_HEADERS]; // header lines only (status line excluded)

//==========================================================================
// Small string helpers
//==========================================================================

// Case-insensitive comparison of exactly n bytes (ASCII).
static bool ci_equal(const char *a, const char *b, size_t n)
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

// Append a formatted string to buf at *len, bounded by cap.
// Returns false on overflow (leaving *len unchanged).
static bool buf_appendf(char *buf, int cap, int *len, const char *fmt, ...)
{
    if (*len >= cap) return false;
    va_list ap;
    va_start(ap, fmt);
    int w = vsnprintf(buf + *len, (size_t)(cap - *len), fmt, ap);
    va_end(ap);
    if (w < 0 || w >= cap - *len) return false;
    *len += w;
    return true;
}

// Format callback that only counts output bytes (no storage).
static bool count_output(void *ctx, const char *str)
{
    *(size_t *)ctx += strlen(str);
    return true;
}

// Find the value of header `name` within a "Name: Value\r\n..." block.
// Writes the trimmed value to out and returns true if found.
static bool header_value(const char *headers, const char *name,
                         char *out, size_t out_size)
{
    size_t name_len = strlen(name);
    const char *p = headers;
    while (*p)
    {
        const char *eol = strstr(p, "\r\n");
        size_t line_len = eol ? (size_t)(eol - p) : strlen(p);

        const char *colon = memchr(p, ':', line_len);
        if (colon)
        {
            size_t hn = (size_t)(colon - p);
            if (hn == name_len && ci_equal(p, name, name_len))
            {
                const char *v = colon + 1;
                const char *vend = p + line_len;
                while (v < vend && (*v == ' ' || *v == '\t')) v++;
                while (vend > v && (vend[-1] == ' ' || vend[-1] == '\t')) vend--;
                size_t vlen = (size_t)(vend - v);
                if (vlen >= out_size) vlen = out_size - 1;
                memcpy(out, v, vlen);
                out[vlen] = '\0';
                return true;
            }
        }

        if (!eol) break;
        p = eol + 2;
    }
    return false;
}

//==========================================================================
// URL parsing
//==========================================================================

// Parse an "http://host[:port][/path]" or "https://host[:port][/path]" URL.
// Sets *secure for https (default port 443) vs http (default port 80). Returns
// false for any other scheme or a malformed authority/port.
static bool parse_http_url(const char *url, char *host, size_t host_size,
                           uint16_t *port, char *path, size_t path_size,
                           bool *secure)
{
    size_t scheme_len;
    if (strncmp(url, "http://", 7) == 0)
    {
        *secure = false;
        scheme_len = 7;
    }
    else if (strncmp(url, "https://", 8) == 0)
    {
        *secure = true;
        scheme_len = 8;
    }
    else
    {
        return false;
    }

    const char *authority = url + scheme_len;
    const char *slash = strchr(authority, '/');
    size_t auth_len = slash ? (size_t)(slash - authority) : strlen(authority);

    char authbuf[256];
    if (auth_len == 0 || auth_len >= sizeof(authbuf)) return false;
    memcpy(authbuf, authority, auth_len);
    authbuf[auth_len] = '\0';

    uint16_t p = *secure ? 443 : 80;
    char *colon = strchr(authbuf, ':');
    if (colon)
    {
        *colon = '\0';
        char *end;
        long pv = strtol(colon + 1, &end, 10);
        if (*end != '\0' || pv < 1 || pv > 65535) return false;
        p = (uint16_t)pv;
    }

    size_t hlen = strlen(authbuf);
    if (hlen == 0 || hlen >= host_size) return false;
    memcpy(host, authbuf, hlen + 1);
    *port = p;

    if (slash)
    {
        if (strlen(slash) >= path_size) return false;
        strcpy(path, slash);
    }
    else
    {
        if (path_size < 2) return false;
        strcpy(path, "/");
    }
    return true;
}

//==========================================================================
// Chunked transfer decoding (in place)
//==========================================================================

// Decode chunked data in `buf` (length len) in place, since the decoded form is
// always shorter than the encoded form. Returns decoded length (>= 0), or -1 if
// it would exceed `cap` ("too big"), or -2 if the encoding is malformed or
// truncated before the 0-length chunk.
static int decode_chunked(char *buf, int len, int cap)
{
    int si = 0, oi = 0;
    while (si < len)
    {
        // Chunk-size line: hex digits up to CRLF (chunk extensions after ';').
        int line_start = si;
        while (si + 1 < len && !(buf[si] == '\r' && buf[si + 1] == '\n')) si++;
        if (si + 1 >= len) return -2; // no CRLF -> truncated

        long chunk_size = 0;
        bool any_digit = false;
        for (int k = line_start; k < si; k++)
        {
            char c = buf[k];
            int d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else if (c == ';') break;               // chunk extension: ignore rest
            else if (c == ' ' || c == '\t') continue;
            else return -2;                         // junk in size line
            chunk_size = chunk_size * 16 + d;
            if (chunk_size > (long)cap) return -1;  // over-size: stop before it grows
            any_digit = true;
        }
        if (!any_digit) return -2;
        si += 2; // skip CRLF after size line

        if (chunk_size == 0) return oi; // final chunk; ignore trailers

        if (si + (int)chunk_size > len) return -2; // body truncated
        if (oi + (int)chunk_size > cap) return -1; // too big
        // Write lags read (we stripped the size line), so memmove is safe.
        memmove(buf + oi, buf + si, (size_t)chunk_size);
        oi += (int)chunk_size;
        si += (int)chunk_size;

        // Skip the CRLF that follows the chunk data.
        if (si + 1 < len && buf[si] == '\r' && buf[si + 1] == '\n') si += 2;
    }
    return -2; // ran out before the 0-length terminator
}

// Reject control characters and spaces in a URL. Without this, a URL built from
// user data (e.g. via `char 13`/`char 10`) could inject extra request lines or
// produce a malformed request line.
static bool url_is_safe(const char *s)
{
    for (; *s != '\0'; s++)
    {
        unsigned char c = (unsigned char)*s;
        if (c <= 0x20u || c == 0x7Fu)
            return false;
    }
    return true;
}

// Reject CR/LF in a header name or value, which would otherwise allow header
// injection / request smuggling.
static bool header_token_is_safe(const char *s)
{
    if (s == NULL)
        return false;
    for (; *s != '\0'; s++)
        if (*s == '\r' || *s == '\n')
            return false;
    return true;
}

//==========================================================================
// Core request
//==========================================================================

// Header arguments are supplied as alternating name/value words:
// hdr_args[0]=name0, hdr_args[1]=value0, ... (validated by callers).
// `body` is NULL for GET, or the POST body (a word or list).
static Result http_request(const char *method, const char *url,
                           const Value *body,
                           int hdr_argc, Value *hdr_args)
{
    LogoIO *io = primitives_get_io();
    if (!io || !io->hardware || !io->hardware->ops)
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    LogoHardwareOps *ops = io->hardware->ops;
    if (!ops->network_tcp_connect || !ops->network_tcp_read ||
        !ops->network_tcp_write || !ops->network_tcp_close)
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);

    // Select the transfer buffer (PSRAM if available, else SRAM) on first use.
    http_io_init();
    if (g_io == NULL)
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);

    // If the device reports WiFi status, require a connection.
    if (ops->wifi_is_connected && !ops->wifi_is_connected())
        return result_error_arg(ERR_CANT_OPEN_NETWORK, NULL, NULL);

    // Reject injection attempts before touching the network.
    if (!url_is_safe(url))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, url);
    for (int i = 0; i + 1 < hdr_argc; i += 2)
    {
        if (!header_token_is_safe(mem_word_ptr(hdr_args[i].as.node)) ||
            !header_token_is_safe(mem_word_ptr(hdr_args[i + 1].as.node)))
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, NULL);
    }

    char host[LOGO_STREAM_NAME_MAX];
    char path[256];
    uint16_t port;
    bool secure;
    if (!parse_http_url(url, host, sizeof(host), &port, path, sizeof(path), &secure))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, url);

    // https requires the device's TLS transport. Name "https" (not the http.get
    // command, which works over http://) so the message is not misleading on a
    // WiFi-but-no-TLS board; pre-setting error_proc keeps the evaluator from
    // overwriting it with the command name.
    if (secure && !ops->network_tls_connect)
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, "https", NULL);

    // Determine the body length up front (Content-Length precedes the body).
    int body_len = 0;
    if (body)
    {
        size_t bl = 0;
        format_value(count_output, &bl, *body);
        if (bl > g_body_max)
            return result_error_arg(ERR_FILE_TOO_BIG, NULL, NULL);
        body_len = (int)bl;
    }

    int timeout_ms = io->network_timeout;

    void *conn;
    if (secure)
    {
        // TLS needs the hostname (for SNI and certificate verification), so we
        // hand it the host directly; the device resolves it internally.
        conn = ops->network_tls_connect(host, port, timeout_ms);
    }
    else
    {
        // Resolve the host to an IP if the device provides a resolver.
        char ip[16];
        if (ops->network_resolve)
        {
            if (!ops->network_resolve(host, ip, sizeof(ip)))
                return result_error_arg(ERR_CANT_OPEN_NETWORK, NULL, NULL);
        }
        else
        {
            strncpy(ip, host, sizeof(ip) - 1);
            ip[sizeof(ip) - 1] = '\0';
        }
        conn = ops->network_tcp_connect(ip, port, timeout_ms);
    }
    if (!conn)
        return result_error_arg(ERR_CANT_OPEN_NETWORK, NULL, NULL);

    // Build the request into the shared buffer.
    int n = 0;
    bool ok = buf_appendf(g_io, g_io_cap, &n, "%s %s HTTP/1.1\r\n", method, path);
    if (port == (secure ? 443 : 80))
        ok = ok && buf_appendf(g_io, g_io_cap, &n, "Host: %s\r\n", host);
    else
        ok = ok && buf_appendf(g_io, g_io_cap, &n, "Host: %s:%u\r\n", host, (unsigned)port);

    for (int i = 0; ok && i + 1 < hdr_argc; i += 2)
    {
        const char *hname = mem_word_ptr(hdr_args[i].as.node);
        const char *hval = mem_word_ptr(hdr_args[i + 1].as.node);
        ok = ok && buf_appendf(g_io, g_io_cap, &n, "%s: %s\r\n", hname, hval);
    }

    if (body)
        ok = ok && buf_appendf(g_io, g_io_cap, &n, "Content-Length: %d\r\n", body_len);
    ok = ok && buf_appendf(g_io, g_io_cap, &n, "Connection: close\r\n\r\n");

    if (ok && body)
    {
        FormatBufferContext bctx;
        format_buffer_init(&bctx, g_io + n, g_io_cap - (size_t)n);
        if (format_value_to_buffer(&bctx, *body))
            n += (int)format_buffer_pos(&bctx);
        else
            ok = false;
    }

    if (!ok)
    {
        ops->network_tcp_close(conn);
        return result_error_arg(ERR_NETWORK_ERROR, NULL, NULL);
    }

    // tcp_write may perform a short write; loop until the whole request is sent.
    int sent = 0;
    while (sent < n)
    {
        int w = ops->network_tcp_write(conn, g_io + sent, n - sent);
        if (w <= 0)
        {
            ops->network_tcp_close(conn);
            return result_error_arg(ERR_LOST_CONNECTION, NULL, NULL);
        }
        sent += w;
    }

    // Reuse the buffer to read the whole response (we asked for Connection:
    // close, so the peer closes when done).
    int total = 0;
    bool timed_out = false;
    while (total < (int)g_io_cap)
    {
        int r = ops->network_tcp_read(conn, g_io + total, (int)g_io_cap - total, timeout_ms);
        if (r > 0) { total += r; continue; }
        if (r == 0) { timed_out = true; break; }
        break; // r < 0: peer closed / EOF
    }
    ops->network_tcp_close(conn);

    // The receive buffer filled before the peer closed: we cannot tell whether
    // the response was complete, so the body framing must treat this as too big
    // rather than risk a silent truncation.
    bool buffer_full = (total >= (int)g_io_cap);

    if (timed_out)
        return result_error_arg(ERR_NETWORK_ERROR, NULL, NULL);

    // Locate the end of the header block.
    int hdr_end = -1;
    for (int i = 0; i + 3 < total; i++)
    {
        if (g_io[i] == '\r' && g_io[i + 1] == '\n' &&
            g_io[i + 2] == '\r' && g_io[i + 3] == '\n')
        {
            hdr_end = i;
            break;
        }
    }
    if (hdr_end < 0)
        return result_error_arg(ERR_NETWORK_ERROR, NULL, NULL);

    // Status line ends at the first CRLF.
    int sl_end = total;
    for (int i = 0; i + 1 < total; i++)
    {
        if (g_io[i] == '\r' && g_io[i + 1] == '\n') { sl_end = i; break; }
    }

    // Status code: the integer after the first space of the status line.
    const char *sp = memchr(g_io, ' ', (size_t)sl_end);
    if (!sp)
        return result_error_arg(ERR_NETWORK_ERROR, NULL, NULL);
    int code = atoi(sp + 1);

    // Header lines occupy [hdr_start, hdr_end). Temporarily NUL-terminate them
    // in place (overwriting the first '\r' of the blank line) for parsing; the
    // body starts later at hdr_end + 4 and is unaffected.
    int hdr_start = sl_end + 2;
    int hdr_len = hdr_end - hdr_start;
    if (hdr_len < 0) hdr_len = 0;
    if (hdr_len >= (int)sizeof(g_headers))
        return result_error_arg(ERR_NETWORK_ERROR, NULL, NULL);
    g_io[hdr_end] = '\0';
    const char *parsed_headers = g_io + hdr_start;

    // Determine the body framing.
    int body_start = hdr_end + 4;
    int body_avail = total - body_start;
    if (body_avail < 0) body_avail = 0;

    char *body_ptr;
    int final_len;

    char te[64];
    bool chunked = header_value(parsed_headers, "Transfer-Encoding", te, sizeof(te));
    if (chunked)
    {
        for (char *t = te; *t; t++)
            if (*t >= 'A' && *t <= 'Z') *t = (char)(*t + 32);
        chunked = strstr(te, "chunked") != NULL;
    }

    if (chunked)
    {
        int decoded_len = decode_chunked(g_io + body_start, body_avail, (int)g_body_max);
        if (decoded_len == -1)
            return result_error_arg(ERR_FILE_TOO_BIG, NULL, NULL);
        if (decoded_len < 0)
            return result_error_arg(ERR_LOST_CONNECTION, NULL, NULL);
        body_ptr = g_io + body_start;
        final_len = decoded_len;
    }
    else
    {
        char cl[32];
        if (header_value(parsed_headers, "Content-Length", cl, sizeof(cl)))
        {
            long declared = atol(cl);
            if (declared < 0)
                return result_error_arg(ERR_NETWORK_ERROR, NULL, NULL);
            if (declared > (long)g_body_max)
                return result_error_arg(ERR_FILE_TOO_BIG, NULL, NULL);
            if (body_avail < declared)
                return result_error_arg(ERR_LOST_CONNECTION, NULL, NULL);
            body_ptr = g_io + body_start;
            final_len = (int)declared;
        }
        else
        {
            // No length framing: the body is everything until the close. If the
            // buffer filled first, the peer never closed, so we may be missing
            // the tail -> error instead of returning a truncated body.
            if (buffer_full || body_avail > (int)g_body_max)
                return result_error_arg(ERR_FILE_TOO_BIG, NULL, NULL);
            body_ptr = g_io + body_start;
            final_len = body_avail;
        }
    }

    // Turn the body into a word before committing metadata (keep the body bytes
    // valid in g_io until copied out). Large bodies (> 255 bytes) become blobs
    // in the aux region; small ones are interned atoms.
    Node body_node = mem_word(body_ptr, (size_t)final_len);

    // A non-empty body that cannot be interned (atom length cap or out of
    // memory) is an error, never a silent truncation/empty result.
    if (final_len > 0 && mem_is_nil(body_node))
        return result_error_arg(ERR_FILE_TOO_BIG, NULL, NULL);

    // Commit the response metadata now that the request fully succeeded.
    memcpy(g_headers, parsed_headers, (size_t)hdr_len);
    g_headers[hdr_len] = '\0';
    g_status_code = code;
    g_has_response = true;

    return result_ok(value_word(body_node));
}

//==========================================================================
// Primitives
//==========================================================================

// Validate that trailing header arguments form name/value word pairs.
static Result check_header_args(int hdr_argc, Value *hdr_args)
{
    if (hdr_argc % 2 != 0)
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, NULL, NULL);
    for (int i = 0; i < hdr_argc; i++)
    {
        if (!value_is_word(hdr_args[i]))
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL,
                                    value_to_string(hdr_args[i]));
    }
    return result_ok(value_list(NODE_NIL));
}

// http.get url
// (http.get url name1 value1 name2 value2 ...)
static Result prim_http_get(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_WORD(args[0]);
    const char *url = mem_word_ptr(args[0].as.node);

    int hdr_argc = argc - 1;
    Value *hdr_args = args + 1;
    Result chk = check_header_args(hdr_argc, hdr_args);
    if (chk.status != RESULT_OK) return chk;

    return http_request("GET", url, NULL, hdr_argc, hdr_args);
}

// Shared implementation for the body-carrying methods (POST, PUT, PATCH):
//   method url data
//   (method url data name1 value1 name2 value2 ...)
static Result http_body_method(int argc, Value *args, const char *method)
{
    REQUIRE_ARGC(2);
    REQUIRE_WORD(args[0]);
    const char *url = mem_word_ptr(args[0].as.node);

    if (!value_is_word(args[1]) && !value_is_list(args[1]))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));

    int hdr_argc = argc - 2;
    Value *hdr_args = args + 2;
    Result chk = check_header_args(hdr_argc, hdr_args);
    if (chk.status != RESULT_OK) return chk;

    return http_request(method, url, &args[1], hdr_argc, hdr_args);
}

// http.post url data
// (http.post url data name1 value1 name2 value2 ...)
static Result prim_http_post(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    return http_body_method(argc, args, "POST");
}

// http.put url data
// (http.put url data name1 value1 name2 value2 ...)
static Result prim_http_put(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    return http_body_method(argc, args, "PUT");
}

// http.patch url data
// (http.patch url data name1 value1 name2 value2 ...)
static Result prim_http_patch(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    return http_body_method(argc, args, "PATCH");
}

// http.delete url
// (http.delete url name1 value1 name2 value2 ...)
static Result prim_http_delete(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_WORD(args[0]);
    const char *url = mem_word_ptr(args[0].as.node);

    int hdr_argc = argc - 1;
    Value *hdr_args = args + 1;
    Result chk = check_header_args(hdr_argc, hdr_args);
    if (chk.status != RESULT_OK) return chk;

    return http_request("DELETE", url, NULL, hdr_argc, hdr_args);
}

// http.status
static Result prim_http_status(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    if (!g_has_response)
        return result_ok(value_list(NODE_NIL));
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", g_status_code);
    return result_ok(value_word(mem_atom_cstr(buf)));
}

// http.header name
static Result prim_http_header(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_WORD(args[0]);
    if (!g_has_response)
        return result_ok(value_list(NODE_NIL));

    const char *name = mem_word_ptr(args[0].as.node);
    char val[256];
    if (header_value(g_headers, name, val, sizeof(val)))
        return result_ok(value_word(mem_atom_cstr(val)));
    return result_ok(value_list(NODE_NIL));
}

void primitives_http_init(void)
{
    // Reset state so repeated init (e.g. across tests) starts clean.
    g_has_response = false;
    g_status_code = 0;
    g_headers[0] = '\0';

    // Re-select the transfer buffer on next request: a previous run may have
    // chosen a PSRAM buffer from an aux region that has since been reset.
    g_io = NULL;
    g_io_cap = 0;
    g_body_max = 0;

    primitive_register("http.get", 1, prim_http_get);
    primitive_register("http.post", 2, prim_http_post);
    primitive_register("http.put", 2, prim_http_put);
    primitive_register("http.patch", 2, prim_http_patch);
    primitive_register("http.delete", 1, prim_http_delete);
    primitive_register("http.status", 0, prim_http_status);
    primitive_register("http.header", 1, prim_http_header);
}
