//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  JSON access primitive: json.get
//
//  A JSON document is kept as text (a word; large responses are PSRAM blobs).
//  json.get scans that text in place, following a path, and allocates only the
//  one value it extracts -- the document is never materialised into a tree, so
//  querying a 100 KB response costs no more node-pool memory than a tiny one.
//
//  A path is a list of steps: a word selects an object member by key
//  (case-sensitive, per the JSON spec); a number selects an array element by
//  position (1-based, like item). A scalar is returned as a Logo value (string
//  unescaped to a word, number as a numeric word, true/false as words, null as
//  the empty list). A nested object or array is returned as its raw JSON text,
//  which can be passed straight back into json.get. Any step that does not
//  match yields the empty list.
//

#include "primitives.h"
#include "memory.h"
#include "value.h"
#include "error.h"
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

void primitives_json_init(void)
{
    primitive_register("json.get", 2, prim_json_get);
}
