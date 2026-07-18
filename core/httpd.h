//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  HTTP server pump: a non-blocking, one-connection-at-a-time HTTP/1.1 request
//  parser riding on the demon poll sites. See docs/http-server-design.md §4/§6.
//
//  The pump accepts a connection, reads and incrementally parses the request
//  line, headers, and Content-Length body, then marks the request "pending"
//  (http.request?). A handler answers with http.respond (M3); malformed,
//  stalled, oversized, or unanswered requests are auto-responded by the pump.
//

#pragma once

#include "error.h"
#include "value.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Start listening on `port` (backs http.listen). Errors if the device has
    // no server ops or WiFi is not connected. Idempotent when already listening:
    // the same port is a no-op, a different port moves the listener.
    Result httpd_listen(uint16_t port);

    // Stop listening and drop any connection (backs http.unlisten). A no-op when
    // not listening.
    void httpd_unlisten(void);

    // True while a listener is open.
    bool httpd_listening(void);

    // True when a fully-parsed request is waiting for a response
    // (backs http.request?).
    bool httpd_request_pending(void);

    // Request accessors, valid only while a request is pending (they return
    // NULL / false otherwise). Spans point into the request buffer and stay
    // valid until http.respond / close. Back http.method / path / query / body /
    // reqheader / remote.
    const char *httpd_method(void);
    const char *httpd_path(void);
    const char *httpd_query(int *len_out);
    const char *httpd_body(int *len_out);
    bool httpd_reqheader(const char *name, const char **val, int *len_out);
    const char *httpd_remote(void);

    // True when the pending request's body was too large to buffer and fired
    // unread: http.body then errors, and http.savebody must be used to stream it.
    bool httpd_body_unread(void);

    // Send a response to the pending request and close (backs http.respond).
    // `body` is formatted like print; `hdr_args` are hdr_argc/2 name/value word
    // pairs (a Content-Type pair overrides the text/plain default). Errors when
    // no request is pending or a header token contains CR/LF.
    Result httpd_respond(int status, Value body, int hdr_argc, Value *hdr_args);

    // Respond with the contents of file `path` as the body, streamed in chunks
    // (backs http.respondfile). Default Content-Type application/octet-stream,
    // overridable via header pairs. A missing file is an ordinary error, left
    // for the handler to turn into a 404; the request stays pending in that case.
    Result httpd_respondfile(int status, const char *path,
                             int hdr_argc, Value *hdr_args);

    // Stream the pending request's body to file `path`, whether it was buffered
    // or fired unread (backs http.savebody). The request stays pending; the
    // handler still finishes with http.respond.
    Result httpd_savebody(const char *path);

    // Advance the pump once: accept, or one bounded read + parse, or age the
    // stall/response deadlines. Does nothing while frozen (demons_frozen()).
    void httpd_poll(void);

    // Budget-gated pump for the instruction poll point and the device idle loop:
    // calls httpd_poll() at most once per HTTPD_POLL_MS.
    void httpd_maybe_poll(void);

    // Clear the listener and any connection. Applied on error-unwind to the
    // toplevel REPL (the demon lifetime; `cs` touches neither demons nor the
    // server): nothing serves after a reset.
    void httpd_reset(void);

#ifdef __cplusplus
}
#endif
