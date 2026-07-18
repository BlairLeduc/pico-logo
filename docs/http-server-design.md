# HTTP server (design)

Status: **v3** — the design gate for the HTTP server closed 2026-07-10
(the open questions in §10 were resolved with the user; none remain).

v3 (2026-07-12) adds mDNS naming (§7, milestone M0): the device answers
to `picologo.local` on the LAN, with `wifi.sethostname`/`wifi.hostname`
primitives; the responder starts with `wifi.connect`, independent of
the server.

v2 adds file transfer (§3 file primitives, the oversized-body pump rule
in §4, milestone M5): upload/download of files of any size — including
binary and files larger than the request buffer — streamed directly
between the connection and storage, on both WiFi boards.

Scope: plain **HTTP only**. HTTPS serving was considered and dropped
(2026-07-10): a LAN device has no public hostname, so certificates would
be self-signed and every browser would warn; the provisioning pain buys
no real trust. The server needs only `LOGO_HAS_WIFI`, so it works on the
Pico 2 W *and* the Pico Plus 2 W (the client's `LOGO_HAS_TLS` tiering is
untouched).

---

## 1. Goal

Let a Logo program answer HTTP requests from a browser or another
program on the LAN — "control your PicoCalc from your phone" — with the
same shape the rest of the language already has: dot-namespaced
primitives, demon-driven events, tiny static memory footprint.

What a working server looks like:

```logo
wifi.connect "TimHortonsWiFi "double-double
http.listen 80
when [http.request?] [
  if equal? http.path "/forward [fd 20]
  if equal? http.path "/right   [rt 90]
  http.respond 200 sentence [heading is] heading
]
pr sentence [serving at] wifi.ip
```

The demon fires while the program runs *and while the user types at the
prompt* — the same Atari-style liveness `when` already delivers. Demons
are optional: `forever [if http.request? [...]]` works too, since
`http.request?` is an ordinary predicate.

With mDNS naming (§7) the phone doesn't need the IP at all: the browser
reaches the device at `http://picologo.local` (or whatever
`wifi.sethostname` chose).

## 2. What already exists

- lwIP is configured for listening (`MEMP_NUM_TCP_PCB_LISTEN 4`,
  `TCP_LISTEN_BACKLOG 1`), and all TCP flows through altcp.
- The device ops (`devices/hardware.h`) have connect/read/write/close/
  can-read for client connections; the picocalc implementation already
  wraps an altcp PCB in a connection struct with receive buffering. A
  server connection is the same struct born from `accept` instead of
  `connect`.
- The HTTP client (`core/primitives_http.c`) has request/response
  header formatting and the tiered-buffer pattern (PSRAM region when
  available, small SRAM heap fallback, sizes in `core/limits.h`).
- The demon system (`core/demons.c`) provides the poll points — top of
  `eval_instruction` plus the device idle loop — that the server pump
  rides on.

## 3. Primitive surface

Commands and queries, all under the existing `http.` namespace:

- `http.listen port` — start listening on *port* (1–65535). Error if
  WiFi is not connected. Idempotent (decided 2026-07-16): re-listening on
  the current port is a no-op, so a program that opens with `http.listen`
  reruns cleanly; a different port moves the listener. (Supersedes the
  original "error if already listening" — see §10.)
- `http.unlisten` — stop listening, drop any pending connection.
  (Naming mirrors `wifi.connect`/`wifi.disconnect`.)
- `http.request?` — `true` when a complete, parsed request is waiting
  for a response. The demon condition and the polling-loop predicate.

Request accessors (error if no request is pending):

- `http.method` — word: `GET`, `POST`, …
- `http.path` — word: percent-decoded path, query string excluded.
- `http.query` — word: the raw query string (`a=1&b=2`), empty word if
  none. (Parameter parsing in Logo is a `parse`-able one-liner; a
  `http.param` helper can come later if usage demands it.)
- `http.body` — word: the request body (empty word for bodyless
  methods).
- `http.reqheader name` — word: value of a request header, empty list
  if absent. (Named to avoid colliding with `http.header`, which
  reports the client's last *response*.)
- `http.remote` — word: the client's IP address, for logging/greeting.

Response:

- `http.respond status body` — send `HTTP/1.1 <status>`, default
  `Content-Type: text/plain; charset=utf-8`, `Connection: close`, then
  *body*; close the connection and clear the pending request.
- `(http.respond status body name1 value1 ...)` — extra response
  headers as name/value pairs, the same parenthesised convention as
  `(http.get url name value ...)`. Supplying `Content-Type` overrides
  the default (so `"text/html` pages work).

HTML helper (added 2026-07-16 during M4, user request): building markup
by hand is painful because the Logo lexer treats `<`, `>`, `=`, and
space as token delimiters, so a literal `<a href=...>` cannot be typed
as a word.

- `http.element tag content` / `(http.element tag content name1 value1
  ...)` — builds `<tag name1=value1 ...>content</tag>` as a word.
  *content* is a word or list (formatted like `print`, so a list's
  spaces come through and its brackets drop); since the result is a
  word, elements nest by passing one `http.element` as another's
  content. Attribute values are single words (escape `=`/`:` with a
  backslash). The HTML analog of the parenthesised name/value
  convention; a pure string builder that touches no server state, so it
  is usable outside a handler too.

File transfer (M5) — bytes move **connection ↔ storage directly**,
never materializing as Logo words, so they are binary-safe (BMPs) and
unbounded by the request buffer or the Logo arena:

- `http.respondfile status path` — like `http.respond`, but the body
  is the named file's contents, streamed in small chunks. Default
  `Content-Type: application/octet-stream` (no extension sniffing);
  override via the parenthesised header form. Missing file is an
  ordinary Logo error — the handler decides whether that becomes a
  `404`.
- `http.savebody path` — stream the pending request's body to the
  named file, whether the body was buffered or is still unread (§4).
  The request stays pending; the handler still finishes with
  `http.respond`.

Both block until the transfer completes, the precedent `http.get`
already set. Both take Logo file paths routed through the storage
router like every other file primitive, and the C side rejects `..`
segments — `http.path` is attacker-supplied.

A file server is then three lines:

```logo
http.listen 80
when [http.request?] [
  if equal? http.method "GET [http.respondfile 200 http.path]
  if equal? http.method "PUT [http.savebody http.path  http.respond 201 "saved]
]
```

paired with `curl -T invaders http://<ip>/games/invaders` to upload and
`curl -o` to download.

## 4. Execution model: a poll-driven pump

One connection at a time, fully buffered, strictly serial:

```
poll point (eval_instruction / idle loop, budget-gated like demons)
  → no connection? accept one (non-blocking)
  → connection? read available bytes, parse incrementally
  → request complete? → http.request? becomes true → demon fires
  → handler runs http.respond → write response, close, pump idle again
```

- **The pump never blocks.** Each poll does at most one non-blocking
  accept or one bounded read. Parsing is an incremental state machine
  (request line → headers → `Content-Length` body).
- **Serial by design.** While a request is pending, no new connection
  is accepted; `TCP_LISTEN_BACKLOG 1` holds one caller in the SYN
  queue. Browsers open parallel connections (notably `favicon.ico`) —
  they are simply served one after another.
- **Protocol floor:** HTTP/1.1 responses, always `Connection: close`,
  no keep-alive, no chunked transfer (a chunked request gets `411`).
  This keeps the state machine small and every request self-contained.
- **Oversized bodies fire unread (M5).** A body up to the buffer cap is
  buffered before the event fires and read with `http.body`. A larger
  `Content-Length` is not an error: the pump fires the event as soon as
  the headers are parsed, with the body *unread* — in that state
  `http.body` errors, and `http.savebody` drains the bytes straight to
  a file through a small chunk buffer. A handler that responds without
  draining costs nothing: `Connection: close` discards the rest.
- **Auto-responses** (pump-generated, no Logo involvement): `400`
  malformed request, `431` header block larger than its buffer, `408`
  if the request stops arriving mid-parse, and `503` if a parsed
  request is still unanswered after `HTTPD_RESPOND_MS` (proposed
  10 s) — the safety valve for a handler that forgot `http.respond` or
  for no handler being armed at all. (Until M5 lands, a body larger
  than the buffer auto-responds `413`; M5 replaces that with the
  fires-unread rule.)
- **`freeze` / `thaw`** suspend the pump along with demons: no accepts,
  and the pending-response deadline pauses. One rule for everything
  autonomous.
- **Lifetime:** the listener follows the demon lifetime — closed on
  error-unwind to toplevel, and explicitly by `http.unlisten`. `cs`
  closes neither the listener nor the demons (revisited 2026-07-18,
  §10): demon lifecycle is separate from turtle graphics, so a program
  that redraws with `cs` keeps serving. A serving program that errors
  still stops serving; restart it by rerunning.

## 5. Device interface changes (`devices/hardware.h`)

Three new ops beside the existing TCP client ops; accepted connections
reuse `network_tcp_read` / `network_tcp_write` / `network_tcp_close` /
`network_tcp_can_read` unchanged:

```c
// Listen for TCP connections on a port.
// Returns an opaque listener handle, NULL on failure.
void *(*network_tcp_listen)(uint16_t port);

// Stop listening and free the listener.
void (*network_tcp_unlisten)(void *listener);

// Accept a pending connection, if any (non-blocking).
// Returns a connection handle usable with the tcp read/write/close
// ops, or NULL when nothing is waiting. remote_ip (16 bytes) receives
// the client address in dotted-decimal form.
void *(*network_tcp_accept)(void *listener, char *remote_ip, size_t ip_size);
```

- **picocalc:** `altcp_new` + `altcp_bind` + `altcp_listen`. The listener
  pre-allocates one connection struct (the same one the client path uses,
  so read/write/close need no changes); the accept callback attaches the
  new PCB to that slot **and wires its receive callback right there**,
  then flags it ready for `network_tcp_accept` to hand over. Wiring recv
  in the accept callback is load-bearing: lwIP *frees* inbound data
  delivered to a PCB whose recv callback is still NULL, so a browser that
  sends its whole request immediately after the handshake would otherwise
  lose it and stall until the pump's `408`. Pre-allocating (in `listen`,
  never in the callback) keeps `malloc` out of the lwIP background
  context. Backlog is one: a second connection while the slot is busy is
  aborted and the client retries; `close` on a server connection detaches
  the PCB but keeps the slot for reuse, and `unlisten` frees it.
  Two reliability details, added 2026-07-16 after a large-upload failure
  on hardware: (1) the receive callback applies **backpressure** — it
  refuses (returns `ERR_MEM`) a segment that will not fit the receive ring
  rather than dropping-and-ACKing the overflow, so a fast/large upload
  throttles the sender instead of corrupting the stream; the ring is sized
  above `TCP_WND` so a refusal can never deadlock. (2) a server connection
  **abandoned without a response** (an upload that failed, or a teardown
  mid-transfer) is closed with a RST rather than a graceful FIN, so its
  local port frees immediately with no TIME_WAIT that would make a
  re-`http.listen` on the same port fail; a connection that did send its
  response still closes gracefully.
- **mock:** scripted — `mock_httpd_queue_connection(request_bytes)`
  queues an incoming connection whose reads return the scripted bytes
  (with a dribble mode that returns them a few bytes per call);
  everything written is recorded for assertions.
- **host:** ops stay `NULL`; primitives raise the same no-network error
  the client ops do.

## 6. Core structure

- `core/httpd.c` / `httpd.h` — the pump: listener handle, connection
  state machine, request buffer, parsed-request fields, auto-response
  logic, `httpd_poll_budgeted()` called from the same two poll sites as
  `demons_poll_budgeted()`. Unit-testable against the mock without any
  primitive involvement.
- `core/primitives_httpd.c` — the Logo surface from §3, all thin
  wrappers over `httpd.c` state.
- **Buffers:** the server gets its own request buffer, deliberately
  *not* shared with the client's `g_io` — a handler that calls
  `http.get` to aggregate or proxy would otherwise clobber the pending
  request. Same tiering as the client: `mem_region_alloc` (PSRAM) at a
  generous cap when available, else a one-time SRAM heap fallback.
  Proposed `core/limits.h` entries: `HTTPD_MAX_HEADERS` 1 KB,
  `HTTPD_MAX_BODY` 4 KB (SRAM tier), `HTTPD_MAX_BODY_PSRAM` 64 KB.
  Allocated lazily on first `http.listen`, so programs that never
  serve pay nothing.
- **Responses need no big buffer:** status line + headers are built in
  a small stack buffer; the body is written straight from the Logo
  word via `network_tcp_write`.

SRAM budget: ~5 KB heap on the SRAM tier, only if `http.listen` is
used; pump state well under 100 B; one extra TCP PCB within the
existing `MEMP_NUM_TCP_PCB 5`. No new static arrays.

## 7. mDNS naming (added 2026-07-12)

Reach the device by name — `http://picologo.local` — instead of
reading `wifi.ip` off the screen and typing an address into the phone.

- **Responder, not resolver:** lwIP ships an mDNS responder app the
  Pico SDK already builds (`pico_lwip_mdns`). It answers A-record
  queries for `<hostname>.local` over multicast UDP. Config deltas in
  `lwipopts.h`: `LWIP_MDNS_RESPONDER 1`, `LWIP_IGMP 1`, one
  `LWIP_NUM_NETIF_CLIENT_DATA` slot, a few extra `MEMP_NUM_SYS_TIMEOUT`
  entries, and one more UDP PCB. Footprint is code plus a handful of
  timers — no meaningful SRAM.
- **Lifecycle (decided 2026-07-12):** the responder starts inside
  `wifi.connect` (once the interface is up) and stops on
  `wifi.disconnect` — *not* tied to `http.listen`. The device is
  findable on the LAN as soon as WiFi is up, which also serves
  cable-free transfer and plain `ping picologo.local` from a laptop.
- **Primitives** (beside the other WiFi primitives in
  `core/primitives_wifi.c`, gated on `LOGO_HAS_WIFI`):
  - `wifi.sethostname name` — set the device's network name. A word
    following hostname-label rules: letters, digits, hyphens; no dots —
    the name **excludes** `.local`, mDNS appends it. Length capped via
    `core/limits.h` (proposed `HOSTNAME_MAX` 32). If WiFi is already
    connected, the responder re-announces under the new name
    immediately.
  - `wifi.hostname` — outputs the current name (again without `.local`).
- **Default `picologo`; no persistence (decided 2026-07-12):** the name
  resets to `picologo` on boot; a custom name is one `wifi.sethostname` line
  in the user's startup file. No flash writes involved.
- **DHCP agrees:** the same name is passed as the lwIP netif hostname
  (`LWIP_NETIF_HOSTNAME` is already on), so the router's device list
  shows the same name mDNS answers to.
- **Device interface:** one new op beside the WiFi ops; core owns the
  name (so `wifi.hostname` reads it back and reconnects reuse it), the
  device applies it:

  ```c
  // Set the device's network hostname (mDNS + DHCP). Takes effect
  // immediately if the network is up, otherwise at the next connect.
  void (*network_set_hostname)(const char *name);
  ```

  picocalc sets the netif hostname and (re)starts the mDNS responder;
  mock records the name for assertions; host stays `NULL`.

## 8. Milestones

Each independently shippable; gate = all tests green + all three
firmware presets linking + reference sections for anything user-visible
(they *are* the help text).

- **M0 — mDNS naming (§7).** Independent of the server pump — ships on
  `wifi.connect` alone, in any order relative to M1–M5.
  `network_set_hostname` device op; lwipopts/CMake enablement
  (`pico_lwip_mdns`); `wifi.sethostname` / `wifi.hostname` with label validation
  and `HOSTNAME_MAX`; responder start/stop with connect/disconnect and
  re-announce on rename. Acceptance: mock tests for default name,
  rename before and after connect, and label validation errors;
  hardware check: `ping picologo.local` from a laptop, rename, ping the
  new name; reference sections for both primitives.

- **M1 — TCP server device ops.** `network_tcp_listen` / `unlisten` /
  `accept` in `hardware.h`; picocalc altcp implementation reusing the
  client connection wrapper; mock scripted connections; host `NULL`.
  Acceptance: mock tests cover listen → accept → read → write → close,
  accept-when-empty, and unlisten-with-connection-pending; firmware
  smoke test echoes one raw TCP connection on hardware.

- **M2 — request pump and parser.** `core/httpd.c` state machine on
  the demon poll sites; `http.listen` / `http.unlisten` /
  `http.request?`; incremental parse of request line, headers, and
  `Content-Length` body; percent-decoding; auto-responses (`400`,
  `408`, `411`, `413`, `431`, `503` deadline); `Connection: close`
  always; freeze/thaw and lifetime rules. Acceptance: mock tests for a
  whole request in one read, byte-dribbled reads, oversize body and
  header block, mid-request stall → `408`, unanswered request → `503`,
  listener cleared on error-unwind.

- **M3 — Logo handler surface.** `http.method` / `http.path` /
  `http.query` / `http.body` / `http.reqheader` / `http.remote`;
  `http.respond` with header pairs and Content-Type defaulting; demon
  integration end-to-end. Acceptance: mock test scripts a full GET and
  a POST → `when [http.request?]` handler fires via `demons_poll` →
  response bytes (status line, headers, body) asserted; handler calling
  `http.get` mid-request doesn't disturb the pending request; reference
  gains an "HTTP Server" section documenting all §3 primitives.

- **M4 — hardware validation and example.** Browser smoke test against
  both WiFi boards (page load, form POST, the favicon double-connect);
  an example program in `logo/` (proposed: `webturtle.logo` — drive the
  turtle from a phone browser via links, serving a small HTML page);
  roadmap progress-log entry.

- **M5 — file transfer.** `http.respondfile` (stream file →
  connection) and `http.savebody` (stream body → file), both through a
  small chunk buffer, binary-safe, routed via the storage router with
  `..` rejection; the oversized-body fires-unread pump rule replaces
  the auto-`413`. Acceptance: mock round-trip of a binary payload
  (containing NUL bytes) larger than the buffer cap — upload to a file,
  download it back, byte-identical; `http.body` on an unread body
  errors; a handler that responds without draining closes cleanly;
  traversal paths (`/../secrets`) rejected. Hardware check: `curl -T` a
  19 KB game onto a **Pico 2 W** (the 4 KB-buffer board) and fetch it
  back byte-identical; reference sections for both primitives.

## 9. Rejected alternatives

| Alternative | Why not |
|---|---|
| HTTPS serving | Self-signed-cert warnings on every client; provisioning pain with no trust gained on a LAN. Client-side `https://` is unaffected. Revisit only against a concrete integration that demands it. |
| Server as a Logo *stream* (`open "listen:8080` handing out connection streams) | The stream layer already does outbound TCP, but raw streams push HTTP parsing, timeouts, and error responses into every user program. HTTP-level primitives are the classroom-sized surface; a raw TCP listener could still be added to the stream layer later without conflict. |
| Concurrent connections | Multiplies buffers and states on a machine with one thread of control and ~5 KB to spare; serial-with-backlog serves the browser use case. |
| Keep-alive / chunked transfer | State-machine complexity for no benefit at LAN latencies; `Connection: close` makes every request self-contained. |
| Sharing the client's `g_io` transfer buffer | A handler calling `http.get` (aggregate/proxy) would clobber the pending request. Separate lazily-allocated buffer costs nothing until used. |
| Blocking `http.serve [instrs]` loop | Freezes the interpreter; the demon pump keeps the prompt and autonomous turtles alive while serving, and a blocking loop remains writable in Logo (`forever [if http.request? [...]]`). |
| `multipart/form-data` upload parsing | Raw `PUT` bodies (`curl -T`) cover the transfer workflow without it; browser `<form>` file upload would drag multipart framing into the pump. Defer until a real page needs it. |
| Content-Type sniffing by file extension in `http.respondfile` | A table to maintain for marginal gain; `application/octet-stream` downloads correctly everywhere, and a handler that cares passes the header explicitly. |
| mDNS *querying* (resolving other machines' `.local` names) | lwIP's responder answers queries but doesn't issue them; a querier is a separate component. `network.resolve` stays plain-DNS-only. |
| DNS-SD service advertisement (`_http._tcp` when `http.listen` is active) | The hostname A record already covers the use case — type `http://picologo.local` into a browser; service *browsing* adds protocol surface and listener/responder coupling for no classroom gain. |
| Persisting `wifi.sethostname` to flash | A startup-file line does the same job with no flash-write path; the name is one word of state (decided 2026-07-12). |

## 10. Decisions (resolved with the user)

Resolved 2026-07-10:

1. **Lifetime on `cs`/error-unwind (§4):** the listener follows the
   demon rule — closed by `cs`/`draw` and on error-unwind to toplevel.
   One teachable rule: *nothing acts on its own after a reset.* A
   serving program that errors stops serving; rerun it to restart.
2. **`http.query` outputs the raw query string.** No parameter parsing
   in M3; a `http.param name` helper can be added later without
   breaking anything if usage demands it.
3. **`http.listen` requires an explicit port** — no default. One fewer
   hidden convention; examples show `http.listen 80` so browsers need
   no port suffix.
4. **`webturtle.logo` (M4) is the links page only** — forward/left/
   right/clear links, each click a GET the handler maps to turtle
   commands. A canvas-snapshot upgrade (`savepic` + `http.respondfile`)
   is a natural follow-on once M5 lands, not an M4 deliverable.
5. **Write protection stays in the handler, not the pump.** No auth in
   C; the example and reference document the shared-secret pattern
   (`if not equal? http.reqheader "X-Key :key [http.respond 403 "no]`).
   Logo stays in control; classroom-appropriate.

Revisited 2026-07-16 (after M0–M4 shipped): the `cs` / error-unwind
lifetime (decision 1) was reopened once serving was demon-integrated in
practice. Kept as-is — a `cs` clears the handler demon anyway, so a
listener surviving `cs` would only sit headless and `503`; closing both
together stays the coherent rule. The re-run friction it existed to
avoid is instead removed by making `http.listen` idempotent (§3), so a
program no longer depends on `cs` closing the listener to rerun. (An
in-handler screen clear uses `clean` / `clean home`, which leaves the
demons and the pending connection intact — `cs` in a handler would
disarm the handler and drop the connection.)

Revisited 2026-07-18: reversed. Demons had grown into a general-purpose
tool (server handlers, timers), so turtle graphics and demon lifecycle
were separated: `cs` no longer clears demons — and therefore no longer
closes the listener; the 2026-07-16 "headless listener" argument
dissolves once `cs` leaves the handler demon armed. Demon teardown is
now the new `cleardemons` primitive (which *can* leave a listener
headless — pair it with `http.unlisten`). Error-unwind to toplevel
still clears demons and closes the listener: *nothing acts on its own
after an error* stands.

Resolved 2026-07-12 (mDNS, §7):

6. **The responder starts with `wifi.connect`, not `http.listen`** —
   the device is findable on the LAN as soon as WiFi is up, independent
   of whether anything is being served.
7. **Default hostname is `picologo`.**
8. **No persistence across reboots** — a custom name is a `wifi.sethostname`
   line in the user's startup file, not a flash setting.
