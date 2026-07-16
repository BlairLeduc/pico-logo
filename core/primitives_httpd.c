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
#include "memory.h"
#include "value.h"
#include "format.h"
#include "limits.h"

#include <string.h>

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

// Shared error for the accessors when no request is pending.
static Result no_request(void)
{
    return result_error_arg(ERR_NETWORK_NOT_OPEN, NULL, NULL);
}

// http.method — the request method (GET, POST, ...) as a word.
static Result prim_http_method(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    const char *m = httpd_method();
    if (!m) return no_request();
    return result_ok(value_word(mem_atom_cstr(m)));
}

// http.path — the percent-decoded request path (query excluded) as a word.
static Result prim_http_path(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    const char *p = httpd_path();
    if (!p) return no_request();
    return result_ok(value_word(mem_atom_cstr(p)));
}

// http.query — the raw query string (empty word if none) as a word.
static Result prim_http_query(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    int len = 0;
    const char *q = httpd_query(&len);
    if (!q) return no_request();
    return result_ok(value_word(mem_word(q, (size_t)len)));
}

// http.body — the request body (empty word for bodyless methods) as a word.
// Errors if the body was too large to buffer (fired unread) — use http.savebody.
static Result prim_http_body(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    if (!httpd_request_pending()) return no_request();
    if (httpd_body_unread())
        return result_error_arg(ERR_FILE_TOO_BIG, NULL, NULL);
    int len = 0;
    const char *b = httpd_body(&len);
    Node w = mem_word(b, (size_t)len);
    if (len > 0 && mem_is_nil(w))
        return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
    return result_ok(value_word(w));
}

// http.reqheader name — a request header's value as a word, empty list if absent.
static Result prim_http_reqheader(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_WORD(args[0]);
    if (!httpd_request_pending()) return no_request();

    const char *val;
    int len;
    if (httpd_reqheader(mem_word_ptr(args[0].as.node), &val, &len))
        return result_ok(value_word(mem_word(val, (size_t)len)));
    return result_ok(value_list(NODE_NIL));  // Absent header -> empty list.
}

// http.remote — the client's IP address as a word.
static Result prim_http_remote(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    const char *r = httpd_remote();
    if (!r) return no_request();
    return result_ok(value_word(mem_atom_cstr(r)));
}

// http.respond status body
// (http.respond status body name1 value1 ...)
// Send a response to the pending request and close the connection.
static Result prim_http_respond(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(2);
    REQUIRE_NUMBER(args[0], status);
    int code = (int)status;
    if ((float)code != status)
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    if (!value_is_word(args[1]) && !value_is_list(args[1]))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));

    int hdr_argc = argc - 2;
    Value *hdr_args = args + 2;
    if (hdr_argc % 2 != 0)
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, NULL, NULL);
    for (int i = 0; i < hdr_argc; i++)
        if (!value_is_word(hdr_args[i]))
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(hdr_args[i]));

    return httpd_respond(code, args[1], hdr_argc, hdr_args);
}

// http.respondfile status path
// (http.respondfile status path name1 value1 ...)
// Respond with the contents of a file as the body, streamed in chunks.
static Result prim_http_respondfile(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(2);
    REQUIRE_NUMBER(args[0], status);
    int code = (int)status;
    if ((float)code != status)
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    REQUIRE_WORD(args[1]);
    const char *path = mem_word_ptr(args[1].as.node);

    int hdr_argc = argc - 2;
    Value *hdr_args = args + 2;
    if (hdr_argc % 2 != 0)
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, NULL, NULL);
    for (int i = 0; i < hdr_argc; i++)
        if (!value_is_word(hdr_args[i]))
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(hdr_args[i]));

    return httpd_respondfile(code, path, hdr_argc, hdr_args);
}

// http.savebody path
// Stream the pending request's body to a file (the request stays pending).
static Result prim_http_savebody(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_WORD(args[0]);
    return httpd_savebody(mem_word_ptr(args[0].as.node));
}

// Append `len` bytes to buf at *n, bounded by cap. Sets *ok false on overflow.
static void el_append(char *buf, int cap, int *n, const char *s, int len, bool *ok)
{
    if (!*ok) return;
    if (*n + len > cap) { *ok = false; return; }
    memcpy(buf + *n, s, (size_t)len);
    *n += len;
}

static void el_append_cstr(char *buf, int cap, int *n, const char *s, bool *ok)
{
    el_append(buf, cap, n, s, (int)strlen(s), ok);
}

static void el_append_word(char *buf, int cap, int *n, Value w, bool *ok)
{
    el_append(buf, cap, n, mem_word_ptr(w.as.node), (int)mem_word_len(w.as.node), ok);
}

// http.element tag content
// (http.element tag content name1 value1 ...)
// Builds "<tag name1=value1 ...>content</tag>" as a word. `content` is a word or
// list formatted like print (so a list's spaces come through and nested
// http.element results compose). Attribute names and values are words.
static Result prim_http_element(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(2);
    REQUIRE_WORD(args[0]);  // tag
    if (!value_is_word(args[1]) && !value_is_list(args[1]))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));

    int pair_argc = argc - 2;
    Value *pairs = args + 2;
    if (pair_argc % 2 != 0)
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, NULL, NULL);
    for (int i = 0; i < pair_argc; i++)
        if (!value_is_word(pairs[i]))
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(pairs[i]));

    char buf[HTTPD_ELEMENT_MAX];
    int n = 0;
    bool ok = true;

    // <tag name=value ...>
    el_append_cstr(buf, sizeof(buf), &n, "<", &ok);
    el_append_word(buf, sizeof(buf), &n, args[0], &ok);
    for (int i = 0; i + 1 < pair_argc; i += 2)
    {
        el_append_cstr(buf, sizeof(buf), &n, " ", &ok);
        el_append_word(buf, sizeof(buf), &n, pairs[i], &ok);
        el_append_cstr(buf, sizeof(buf), &n, "=", &ok);
        el_append_word(buf, sizeof(buf), &n, pairs[i + 1], &ok);
    }
    el_append_cstr(buf, sizeof(buf), &n, ">", &ok);

    // content (formatted like print)
    if (ok)
    {
        FormatBufferContext fb;
        format_buffer_init(&fb, buf + n, sizeof(buf) - (size_t)n);
        if (format_value_to_buffer(&fb, args[1]))
            n += (int)format_buffer_pos(&fb);
        else
            ok = false;
    }

    // </tag>
    el_append_cstr(buf, sizeof(buf), &n, "</", &ok);
    el_append_word(buf, sizeof(buf), &n, args[0], &ok);
    el_append_cstr(buf, sizeof(buf), &n, ">", &ok);

    if (!ok)
        return result_error_arg(ERR_FILE_TOO_BIG, NULL, NULL);

    Node w = mem_word(buf, (size_t)n);
    if (n > 0 && mem_is_nil(w))
        return result_error_arg(ERR_FILE_TOO_BIG, NULL, NULL);
    return result_ok(value_word(w));
}

void primitives_httpd_init(void)
{
    // Fresh interpreter: no listener open.
    httpd_reset();

    primitive_register("http.listen", 1, prim_http_listen);
    primitive_register("http.unlisten", 0, prim_http_unlisten);
    primitive_register("http.request?", 0, prim_http_request_p);
    primitive_register("http.requestp", 0, prim_http_request_p);  // Alias
    primitive_register("http.method", 0, prim_http_method);
    primitive_register("http.path", 0, prim_http_path);
    primitive_register("http.query", 0, prim_http_query);
    primitive_register("http.body", 0, prim_http_body);
    primitive_register("http.reqheader", 1, prim_http_reqheader);
    primitive_register("http.remote", 0, prim_http_remote);
    primitive_register("http.respond", 2, prim_http_respond);
    primitive_register("http.respondfile", 2, prim_http_respondfile);
    primitive_register("http.savebody", 1, prim_http_savebody);
    primitive_register("http.element", 2, prim_http_element);
}
