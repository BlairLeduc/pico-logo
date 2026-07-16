//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  HTTP server primitives: http.listen, http.unlisten, http.request?
//
//  Thin wrappers over the pump in core/httpd.c. The request accessors
//  (http.method / path / query / body / reqheader / remote) and http.respond
//  are added in M3. See docs/http-server-design.md §3.
//

#include "primitives.h"
#include "httpd.h"
#include "value.h"

// http.listen port
// Start listening for HTTP requests on the given port (1-65535).
static Result prim_http_listen(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_NUMBER(args[0], port);

    int p = (int)port;
    if ((float)p != port || p < 1 || p > 65535)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    return httpd_listen((uint16_t)p);
}

// http.unlisten
// Stop listening and drop any pending connection. A no-op when not listening.
static Result prim_http_unlisten(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    httpd_unlisten();
    return result_none();
}

// http.request?
// Outputs true when a complete request is waiting for a response.
static Result prim_http_request_p(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    return result_ok(value_bool(httpd_request_pending()));
}

void primitives_httpd_init(void)
{
    // Fresh interpreter: no listener open.
    httpd_reset();

    primitive_register("http.listen", 1, prim_http_listen);
    primitive_register("http.unlisten", 0, prim_http_unlisten);
    primitive_register("http.request?", 0, prim_http_request_p);
    primitive_register("http.requestp", 0, prim_http_request_p);  // Alias
}
