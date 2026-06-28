//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  JSON primitives: json.get / json.count (read), json.object / json.array /
//  json.make (build)
//
//  Reading: a JSON document is kept as text (a word; large responses are PSRAM
//  blobs). json.get scans that text in place, following a path, and allocates
//  only the one value it extracts -- the document is never materialised into a
//  tree, so querying a 100 KB response costs no more node-pool memory than a
//  tiny one. A path is a list of steps: a word selects an object member by key
//  (case-sensitive, per the JSON spec); a number selects an array element by
//  position (1-based, like item). A scalar is returned as a Logo value (string
//  unescaped to a word, number as a numeric word, true/false as words, null as
//  the empty list). A nested object or array is returned as its raw JSON text,
//  which can be passed straight back into json.get. Any step that does not
//  match yields the empty list.
//
//  Building: json.object and json.array assemble a tagged Logo structure, which
//  json.make renders to JSON text. Builders take quoted-word arguments (so
//  values containing '/', '-' or spaces survive the reader, unlike list
//  literals) and nest, so json.make (json.object ...) produces text ready for
//  http.post.
//

#include "primitives.h"
#include "memory.h"
#include "value.h"
#include "error.h"
#include "format.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A read cursor over the document's bytes. The bytes are never modified.
typedef struct
{
    const char *p;
    const char *end;
} Scan;

static void skip_ws(Scan *s)
{
    while (s->p < s->end)
    {
        char c = *s->p;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            s->p++;
        else
            break;
    }
}

// Advance past a JSON string. Assumes *p == '"'. Leaves p just past the closing
// quote. Returns false if the string is unterminated.
static bool skip_string(Scan *s)
{
    s->p++; // opening quote
    while (s->p < s->end)
    {
        char c = *s->p++;
        if (c == '\\')
        {
            if (s->p < s->end)
                s->p++; // skip the escaped character
        }
        else if (c == '"')
        {
            return true;
        }
    }
    return false;
}

// Advance past one complete JSON value (object, array, string, or bare scalar).
// Objects/arrays are skipped iteratively with a depth counter -- no recursion,
// so deeply nested input cannot overflow the stack. Returns false on malformed
// input.
static bool skip_value(Scan *s)
{
    skip_ws(s);
    if (s->p >= s->end)
        return false;

    char c = *s->p;
    if (c == '"')
        return skip_string(s);

    if (c == '{' || c == '[')
    {
        int depth = 0;
        while (s->p < s->end)
        {
            char d = *s->p;
            if (d == '"')
            {
                if (!skip_string(s))
                    return false;
                continue;
            }
            if (d == '{' || d == '[')
                depth++;
            else if (d == '}' || d == ']')
            {
                depth--;
                s->p++;
                if (depth == 0)
                    return true;
                continue;
            }
            s->p++;
        }
        return false;
    }

    // Bare scalar (number, true, false, null): run to the next delimiter.
    while (s->p < s->end)
    {
        char d = *s->p;
        if (d == ',' || d == '}' || d == ']' ||
            d == ' ' || d == '\t' || d == '\n' || d == '\r')
            break;
        s->p++;
    }
    return true;
}

// Enter the object at the cursor and position p at the value of member `key`.
// Returns false if the cursor is not an object or the key is absent.
static bool enter_object(Scan *s, const char *key, size_t key_len)
{
    skip_ws(s);
    if (s->p >= s->end || *s->p != '{')
        return false;
    s->p++;

    for (;;)
    {
        skip_ws(s);
        if (s->p >= s->end || *s->p != '"') // includes the empty-object '}' case
            return false;

        const char *k_start = s->p + 1;
        if (!skip_string(s))
            return false;
        const char *k_end = s->p - 1; // before the closing quote

        skip_ws(s);
        if (s->p >= s->end || *s->p != ':')
            return false;
        s->p++;

        size_t k_len = (size_t)(k_end - k_start);
        if (k_len == key_len && memcmp(k_start, key, key_len) == 0)
        {
            skip_ws(s);
            return true; // positioned at the member value
        }

        if (!skip_value(s))
            return false;
        skip_ws(s);
        if (s->p < s->end && *s->p == ',')
        {
            s->p++;
            continue;
        }
        return false; // '}' or end of input: key not found
    }
}

// Enter the array at the cursor and position p at element `index` (1-based).
// Returns false if the cursor is not an array or the index is out of range.
static bool enter_array(Scan *s, int index)
{
    if (index < 1)
        return false;
    skip_ws(s);
    if (s->p >= s->end || *s->p != '[')
        return false;
    s->p++;

    for (int i = 1;; i++)
    {
        skip_ws(s);
        if (s->p >= s->end || *s->p == ']')
            return false; // out of range (or empty array)
        if (i == index)
            return true;
        if (!skip_value(s))
            return false;
        skip_ws(s);
        if (s->p < s->end && *s->p == ',')
        {
            s->p++;
            continue;
        }
        return false; // ']' or end: index past the last element
    }
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int utf8_encode(unsigned cp, char *out)
{
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800)
    {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    out[0] = (char)(0xE0 | (cp >> 12));
    out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
}

// Unescape a JSON string body [start,end) into out. Output is never longer than
// the input, so out must hold at least (end - start) bytes. \uXXXX is decoded
// for the basic multilingual plane (surrogate pairs are not combined).
static size_t json_unescape(const char *start, const char *end, char *out)
{
    char *o = out;
    const char *p = start;
    while (p < end)
    {
        char c = *p++;
        if (c != '\\' || p >= end)
        {
            *o++ = c;
            continue;
        }
        char e = *p++;
        switch (e)
        {
            case '"':  *o++ = '"';  break;
            case '\\': *o++ = '\\'; break;
            case '/':  *o++ = '/';  break;
            case 'n':  *o++ = '\n'; break;
            case 't':  *o++ = '\t'; break;
            case 'r':  *o++ = '\r'; break;
            case 'b':  *o++ = '\b'; break;
            case 'f':  *o++ = '\f'; break;
            case 'u':
            {
                int h0, h1, h2, h3;
                if (end - p >= 4 &&
                    (h0 = hex_val(p[0])) >= 0 && (h1 = hex_val(p[1])) >= 0 &&
                    (h2 = hex_val(p[2])) >= 0 && (h3 = hex_val(p[3])) >= 0)
                {
                    unsigned cp = (unsigned)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
                    o += utf8_encode(cp, o);
                    p += 4;
                }
                else
                {
                    *o++ = 'u';
                }
                break;
            }
            default: *o++ = e; break;
        }
    }
    return (size_t)(o - out);
}

// Intern [start,end) of raw JSON bytes as a word (atom or blob), returning an
// out-of-space error if allocation fails.
static Result word_from_bytes(const char *start, size_t len)
{
    Node node = mem_word(start, len);
    if (mem_is_nil(node) && len > 0)
        return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
    return result_ok(value_word(node));
}

// Build a Logo word from a JSON string body [start,end), unescaping if needed.
static Result string_value(const char *start, const char *end)
{
    size_t len = (size_t)(end - start);

    // Fast path: no escapes, intern the span directly.
    if (memchr(start, '\\', len) == NULL)
        return word_from_bytes(start, len);

    char *buf = malloc(len > 0 ? len : 1);
    if (!buf)
        return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
    size_t out_len = json_unescape(start, end, buf);
    Result r = word_from_bytes(buf, out_len);
    free(buf);
    return r;
}

// Extract the value at the cursor as a Logo value.
static Result extract_value(Scan *s)
{
    skip_ws(s);
    if (s->p >= s->end)
        return result_ok(value_list(NODE_NIL));

    char c = *s->p;

    if (c == '"')
    {
        const char *v_start = s->p + 1;
        Scan tmp = *s;
        if (!skip_string(&tmp))
            return result_ok(value_list(NODE_NIL));
        return string_value(v_start, tmp.p - 1);
    }

    if (c == '{' || c == '[')
    {
        const char *v_start = s->p;
        Scan tmp = *s;
        if (!skip_value(&tmp))
            return result_ok(value_list(NODE_NIL));
        return word_from_bytes(v_start, (size_t)(tmp.p - v_start)); // raw, re-queryable
    }

    // Bare scalar: number, true, false, or null.
    const char *v_start = s->p;
    Scan tmp = *s;
    skip_value(&tmp);
    size_t len = (size_t)(tmp.p - v_start);

    if (len == 4 && memcmp(v_start, "null", 4) == 0)
        return result_ok(value_list(NODE_NIL));

    return word_from_bytes(v_start, len); // number / true / false as a word
}

// json.get document path
static Result prim_json_get(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(2);
    REQUIRE_WORD(args[0]);
    REQUIRE_LIST(args[1]);

    const char *text = mem_word_ptr(args[0].as.node);
    size_t text_len = mem_word_len(args[0].as.node);
    Scan s = { text, text + text_len };

    for (Node step = args[1].as.node; !mem_is_nil(step); step = mem_cdr(step))
    {
        Node step_node = mem_car(step);
        if (!mem_is_word(step_node))
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));

        bool ok;
        float index_f;
        if (value_to_number(value_word(step_node), &index_f))
            ok = enter_array(&s, (int)index_f);
        else
            ok = enter_object(&s, mem_word_ptr(step_node), mem_word_len(step_node));

        if (!ok)
            return result_ok(value_list(NODE_NIL));
    }

    return extract_value(&s);
}

// json.count value
// Counts the elements of a JSON array, or the members of a JSON object, held in
// value (a word). A scalar counts as 0, and the empty list -- json.get's result
// for a missing path or JSON null -- counts as 0, so `json.count json.get ...`
// is safe to use directly as a loop bound.
static Result prim_json_count(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);

    if (value_is_list(args[0]))
    {
        if (mem_is_nil(args[0].as.node))
            return result_ok(value_number(0));
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    REQUIRE_WORD(args[0]);

    const char *text = mem_word_ptr(args[0].as.node);
    size_t text_len = mem_word_len(args[0].as.node);
    Scan s = { text, text + text_len };

    skip_ws(&s);
    if (s.p >= s.end)
        return result_ok(value_number(0));

    char open = *s.p;
    if (open != '[' && open != '{')
        return result_ok(value_number(0)); // a scalar is not a collection

    char close = (open == '[') ? ']' : '}';
    s.p++;
    skip_ws(&s);
    if (s.p < s.end && *s.p == close)
        return result_ok(value_number(0)); // empty collection

    // Count top-level separators: one more than the number of commas that are
    // not inside a nested container or a string.
    int count = 1;
    int depth = 0;
    while (s.p < s.end)
    {
        char c = *s.p;
        if (c == '"')
        {
            if (!skip_string(&s))
                break;
            continue;
        }
        if (c == '[' || c == '{')
        {
            depth++;
            s.p++;
        }
        else if (c == ']' || c == '}')
        {
            if (depth == 0)
                break;
            depth--;
            s.p++;
        }
        else if (c == ',' && depth == 0)
        {
            count++;
            s.p++;
        }
        else
        {
            s.p++;
        }
    }

    return result_ok(value_number((float)count));
}

//==========================================================================
// Building: json.object, json.array, json.make
//==========================================================================
//
// json.object / json.array assemble a tagged AST so that nesting stays
// unambiguous: an object is cons(OBJ_TAG, [k1 v1 k2 v2 ...]) and an array is
// cons(ARR_TAG, [v1 v2 ...]). The tags begin with a control byte that the Logo
// reader cannot produce, so they never collide with user data. A plain Logo
// list renders as an array, and the empty list renders as null (so it
// round-trips json.get, which returns the empty list for JSON null).

#define JSON_OBJ_TAG "\x01o"
#define JSON_ARR_TAG "\x01a"
#define JSON_TAG_LEN 2

static bool node_has_tag(Node n, const char *tag)
{
    return mem_is_word(n) && mem_word_len(n) == JSON_TAG_LEN &&
           memcmp(mem_word_ptr(n), tag, JSON_TAG_LEN) == 0;
}

// Coerce a builder argument to the node stored in the AST. Numbers become
// numeric words; words and lists are stored as-is (a list may be a nested
// builder result or a plain array).
static bool value_to_json_node(Value v, Node *out)
{
    if (value_is_number(v))
    {
        *out = number_to_word(v.as.number);
        return true;
    }
    if (value_is_word(v) || value_is_list(v))
    {
        *out = v.as.node;
        return true;
    }
    return false;
}

// A growable text buffer for rendering. oom latches on allocation failure.
typedef struct
{
    char *buf;
    size_t len;
    size_t cap;
    bool oom;
} StrBuf;

static bool sb_reserve(StrBuf *b, size_t extra)
{
    if (b->oom)
        return false;
    if (b->len + extra + 1 <= b->cap)
        return true;
    size_t ncap = b->cap ? b->cap : 64;
    while (ncap < b->len + extra + 1)
        ncap *= 2;
    char *n = realloc(b->buf, ncap);
    if (!n)
    {
        b->oom = true;
        return false;
    }
    b->buf = n;
    b->cap = ncap;
    return true;
}

static void sb_putc(StrBuf *b, char c)
{
    if (sb_reserve(b, 1))
        b->buf[b->len++] = c;
}

static void sb_put(StrBuf *b, const char *s, size_t n)
{
    if (sb_reserve(b, n))
    {
        memcpy(b->buf + b->len, s, n);
        b->len += n;
    }
}

// Append s as a quoted, escaped JSON string.
static void sb_put_string(StrBuf *b, const char *s, size_t len)
{
    sb_putc(b, '"');
    for (size_t i = 0; i < len; i++)
    {
        unsigned char c = (unsigned char)s[i];
        switch (c)
        {
            case '"':  sb_put(b, "\\\"", 2); break;
            case '\\': sb_put(b, "\\\\", 2); break;
            case '\n': sb_put(b, "\\n", 2); break;
            case '\t': sb_put(b, "\\t", 2); break;
            case '\r': sb_put(b, "\\r", 2); break;
            case '\b': sb_put(b, "\\b", 2); break;
            case '\f': sb_put(b, "\\f", 2); break;
            default:
                if (c < 0x20)
                {
                    char esc[7];
                    snprintf(esc, sizeof(esc), "\\u%04x", c);
                    sb_put(b, esc, 6);
                }
                else
                {
                    sb_putc(b, (char)c);
                }
        }
    }
    sb_putc(b, '"');
}

static void render_value(StrBuf *b, Value v);

static void render_node(StrBuf *b, Node n)
{
    render_value(b, mem_is_word(n) ? value_word(n) : value_list(n));
}

static void render_value(StrBuf *b, Value v)
{
    if (value_is_number(v))
    {
        Node w = number_to_word(v.as.number);
        sb_put(b, mem_word_ptr(w), mem_word_len(w));
        return;
    }

    if (value_is_word(v))
    {
        const char *s = mem_word_ptr(v.as.node);
        size_t len = mem_word_len(v.as.node);
        if (len == 4 && memcmp(s, "true", 4) == 0)
            sb_put(b, "true", 4);
        else if (len == 5 && memcmp(s, "false", 5) == 0)
            sb_put(b, "false", 5);
        else
        {
            float num;
            if (value_to_number(v, &num))
                sb_put(b, s, len); // numeric word -> JSON number
            else
                sb_put_string(b, s, len);
        }
        return;
    }

    // List: tagged object, tagged array, empty (null), or plain array.
    Node list = v.as.node;
    if (mem_is_nil(list))
    {
        sb_put(b, "null", 4);
        return;
    }

    Node head = mem_car(list);
    if (node_has_tag(head, JSON_OBJ_TAG))
    {
        sb_putc(b, '{');
        bool first = true;
        for (Node p = mem_cdr(list); !mem_is_nil(p);)
        {
            Node key = mem_car(p);
            Node rest = mem_cdr(p);
            Node val = mem_is_nil(rest) ? NODE_NIL : mem_car(rest);
            if (!first)
                sb_putc(b, ',');
            first = false;
            sb_put_string(b, mem_word_ptr(key), mem_word_len(key));
            sb_putc(b, ':');
            render_node(b, val);
            p = mem_is_nil(rest) ? rest : mem_cdr(rest);
        }
        sb_putc(b, '}');
        return;
    }

    bool tagged_array = node_has_tag(head, JSON_ARR_TAG);
    sb_putc(b, '[');
    bool first = true;
    for (Node p = tagged_array ? mem_cdr(list) : list; !mem_is_nil(p); p = mem_cdr(p))
    {
        if (!first)
            sb_putc(b, ',');
        first = false;
        render_node(b, mem_car(p));
    }
    sb_putc(b, ']');
}

// (json.object key1 value1 key2 value2 ...)
static Result prim_json_object(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    if (argc % 2 != 0)
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, NULL, NULL);

    Node list = NODE_NIL;
    for (int i = argc - 2; i >= 0; i -= 2)
    {
        if (!value_is_word(args[i]))
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[i]));
        Node val;
        if (!value_to_json_node(args[i + 1], &val))
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[i + 1]));
        list = mem_cons(val, list);
        list = mem_cons(args[i].as.node, list);
    }

    Node tagged = mem_cons(mem_atom(JSON_OBJ_TAG, JSON_TAG_LEN), list);
    if (mem_is_nil(tagged))
        return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
    return result_ok(value_list(tagged));
}

// (json.array value1 value2 ...)
static Result prim_json_array(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);

    Node list = NODE_NIL;
    for (int i = argc - 1; i >= 0; i--)
    {
        Node val;
        if (!value_to_json_node(args[i], &val))
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[i]));
        list = mem_cons(val, list);
    }

    Node tagged = mem_cons(mem_atom(JSON_ARR_TAG, JSON_TAG_LEN), list);
    if (mem_is_nil(tagged))
        return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
    return result_ok(value_list(tagged));
}

// json.make value
static Result prim_json_make(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);

    StrBuf b = { NULL, 0, 0, false };
    render_value(&b, args[0]);
    if (b.oom)
    {
        free(b.buf);
        return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
    }

    Node w = mem_word(b.buf ? b.buf : "", b.len);
    free(b.buf);
    if (mem_is_nil(w) && b.len > 0)
        return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
    return result_ok(value_word(w));
}

void primitives_json_init(void)
{
    primitive_register("json.get", 2, prim_json_get);
    primitive_register("json.count", 1, prim_json_count);
    primitive_register("json.object", 0, prim_json_object);
    primitive_register("json.array", 0, prim_json_array);
    primitive_register("json.make", 1, prim_json_make);
}
