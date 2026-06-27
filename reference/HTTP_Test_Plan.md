# HTTP Operations — Test Plan

Status: draft 2026-06-27. Covers the v1 HTTP-only surface (`http.get`,
`http.post`, `http.status`, `http.header`) specified in the `# HTTP Operations`
section of `Pico_Logo_Reference.md`. Storage/PSRAM rationale lives in
`Networking_Memory_and_Storage_Notes.md`.

Per CLAUDE.md, the tests come first or alongside the code, run natively via
Unity/ctest, and exercise the **mock device** rather than real hardware. The
HTTP client is built on the existing `network_tcp_*` hardware ops, so the mock
operates at the **byte level**: the test scripts the raw HTTP response bytes the
"server" returns, and asserts on the raw request bytes the client sent plus the
Logo-level result. This validates the whole client — URL parsing, request
framing, response parsing, chunked decoding — against a controlled stream.

---

## 1. Prerequisite: a mock TCP backend

`tests/test_scaffold.c` currently wires `network_tcp_connect/close/read/write/
can_read` to `NULL` (lines ~246–250), so the existing `open "host:port` path is
**untested** too. This work adds the backend; doing so also retroactively makes
that path testable.

### Mock state (add to `mock_state.network` in `mock_device.c`)

- Scripted response: byte buffer + length + read cursor.
- Recorded request: byte buffer + length (everything the client wrote).
- Connect behaviour: success flag; recorded last IP and port.
- Read shaping: max bytes returned per `read` call (simulate TCP segmentation);
  optional "timeout at byte N" and "close at byte N" triggers.

All of it is zeroed by `mock_device_init()`.

### Mock helper API (add to `mock_device.h`)

```c
// Scripting the connection and server response
void  mock_device_set_tcp_connect_result(bool success);      // NULL handle on false
void  mock_device_set_tcp_response(const char *bytes, size_t len);
void  mock_device_set_tcp_read_chunk(int max_bytes_per_read); // 0 = unlimited
void  mock_device_set_tcp_timeout_after(int bytes);           // read returns 0 (timeout)
void  mock_device_set_tcp_close_after(int bytes);             // read returns -1 (closed)

// Inspecting what the client did
const char *mock_device_get_tcp_request(void);     // raw bytes written by client
size_t      mock_device_get_tcp_request_len(void);
const char *mock_device_get_last_tcp_ip(void);
uint16_t    mock_device_get_last_tcp_port(void);

// Hardware ops (wired into mock_hardware_ops, replacing the NULLs)
void *mock_network_tcp_connect(const char *ip, uint16_t port, int timeout_ms);
void  mock_network_tcp_close(void *conn);
int   mock_network_tcp_read(void *conn, char *buf, int count, int timeout_ms);
int   mock_network_tcp_write(void *conn, const char *data, int count);
bool  mock_network_tcp_can_read(void *conn);
```

These honour the `hardware.h` contracts exactly: `connect` → handle or NULL;
`read` → bytes / `0` on timeout / `-1` on error or close; `write` → bytes or `-1`.

### Test fixture

`setUp()` calls `test_scaffold_setUp()` + `mock_device_init()`, then establishes
a working baseline so each test overrides only what it cares about:

```c
mock_device_set_wifi_connected(true);
mock_device_set_resolve_result("93.184.216.34", true); // example.com
mock_device_set_tcp_connect_result(true);
```

---

## 2. Test file: `tests/test_primitives_http.c`

Registered with `add_logo_test(test_primitives_http)` in `tests/CMakeLists.txt`.
Same shape as `test_primitives_network.c`: `setUp`/`tearDown`, one function per
case, a `main()` running them with `RUN_TEST`.

A small helper keeps the response-scripting readable:

```c
static void script_response(const char *raw) {
    mock_device_set_tcp_response(raw, strlen(raw));
}
```

---

## 3. Test cases

### `http.get` — happy path & request framing
- **returns_body_on_200** — script `HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 12\r\n\r\nHello World!`; assert `VALUE_WORD` == `"Hello World!"`.
- **sets_status_200** — after the above, `http.status` outputs `200`.
- **request_has_get_line_and_host** — assert recorded request contains `GET /menu.txt HTTP/1.1` and `Host: example.com` for url `"http://example.com/menu.txt`.
- **root_path_when_url_has_none** — url `"http://example.com` → request line `GET / HTTP/1.1`.
- **default_port_80** — `mock_device_get_last_tcp_port()` == 80.
- **explicit_port** — url `"http://example.com:8080/x` → port 8080, `Host: example.com:8080`.
- **resolves_host_to_ip** — `mock_device_get_last_tcp_ip()` == the resolved IP; resolve hostname recorded == `example.com`.
- **custom_headers** — `(http.get "http://example.com/ [[Accept text/plain] [User-Agent PicoLogo]])` → request contains both header lines.
- **empty_body** — `Content-Length: 0` → outputs the empty word.

### `http.get` — response parsing
- **content_length_exact** — body delivered equals `Content-Length`; trailing bytes (if any) ignored.
- **partial_reads_reassembled** — `mock_device_set_tcp_read_chunk(1)`; full body still assembled byte-by-byte.
- **chunked_transfer_decoding** — `Transfer-Encoding: chunked` with `5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n` → body `Hello World`.
- **status_line_http_1_0** — `HTTP/1.0 200 OK` parsed the same as 1.1.
- **header_value_whitespace_trimmed** — `Content-Type:   text/plain  ` → `http.header "Content-Type` == `text/plain`.
- **no_response_headers** — status line + blank line + body, no headers → body still returned, `http.header` of anything → empty list.

### `http.get` — completed-but-error responses (still RESULT_OK)
- **404_returns_body_not_error** — `HTTP/1.1 404 Not Found` → `RESULT_OK`, body returned, `http.status` == `404`.
- **500_returns_body** — same shape for a 500.

### `http.get` — failures (RESULT_ERROR, catchable)
- **connection_refused_errors** — `mock_device_set_tcp_connect_result(false)` → `RESULT_ERROR`.
- **dns_failure_errors** — `mock_device_set_resolve_result(NULL, false)` → `RESULT_ERROR`.
- **read_timeout_errors** — `mock_device_set_tcp_timeout_after(N)` mid-response → `RESULT_ERROR`.
- **closed_midstream_errors** — `mock_device_set_tcp_close_after(N)` before `Content-Length` satisfied → `RESULT_ERROR`.
- **oversize_body_errors** — `Content-Length` (or chunk total) exceeding `HTTP_MAX_BODY` → `RESULT_ERROR`, not a truncated word.
- **requires_wifi** — `mock_device_set_wifi_connected(false)` → `RESULT_ERROR`.
- **rejects_https_url** — `"https://example.com/` → `RESULT_ERROR`.
- **rejects_non_http_url** — `"example.com/` (no scheme) → `RESULT_ERROR`.
- **requires_one_argument** — `http.get` with no input → `RESULT_ERROR`.
- **requires_word_argument** — `http.get [example.com]` → `RESULT_ERROR`.

### `http.post`
- **sends_method_and_body** — request line `POST ...`, body present, `Content-Length` == body length.
- **list_body_joined** — data `[pierogi fries]` → request body `pierogi fries` (no brackets).
- **word_body** — data as a word sent verbatim.
- **returns_response_body** — output is the server's response body word.
- **sets_status** — `http.status` reflects the POST response (e.g. `201`).
- **custom_headers** — third (variadic) arg injects request headers.
- **requires_two_arguments** — `(http.post "http://example.com/)` → `RESULT_ERROR`.

### `http.status`
- **empty_before_any_request** — fresh interpreter → outputs the empty list (`VALUE_LIST`, `NODE_NIL`).
- **reflects_most_recent** — GET (200) then GET (404) → `http.status` == `404`.

### `http.header`
- **returns_value** — `http.header "Content-Type` == `text/plain`.
- **case_insensitive** — `http.header "content-type` matches a `Content-Type` header.
- **absent_returns_empty_list** — header not in response → empty list.
- **empty_before_any_request** — fresh interpreter → empty list.

### Statefulness contract (the "most recent request" rule)
- **metadata_replaced_by_next_request** — request 1 has `X-Tag: one`, request 2 omits it; after request 2, `http.header "X-Tag` → empty list and `http.status` reflects request 2.

---

## 4. Out of scope (tracked for later)

- HTTPS / TLS paths — deferred with the TLS phase; no cases here.
- Redirect following (3xx) — v1 returns the 3xx body and status as-is; a
  `follow-redirects` behaviour, if added, gets its own cases.
- PSRAM-backed large bodies — `HTTP_MAX_BODY` is exercised by
  **oversize_body_errors**; behaviour above that cap is the only contract for now.

## 5. Done criteria

- New cases written first / alongside the client; `ctest` green for the whole
  suite (not just the new file).
- `mock_network_tcp_*` wired into `mock_hardware_ops`; the previously-`NULL` TCP
  ops are now exercised.
