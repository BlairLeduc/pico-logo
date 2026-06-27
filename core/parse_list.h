//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Shared helper that turns a string into a Logo list using the lexer,
//  recursively handling nested bracketed sublists. Used by both the
//  `parse` primitive and the `readlist` primitive so bracket matching
//  behaves consistently with the main parser.
//

#pragma once

#include <stdbool.h>
#include "memory.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Result of a parse attempt.
    typedef struct
    {
        Node node;    // Parsed list head, or NODE_NIL on empty/failure.
        bool success; // false only on cell-allocation failure.
    } ParseListResult;

    // Parse `line` as the body of a Logo list. Stops at end-of-string,
    // a stray right bracket, or a lexer error. Nested `[...]` produce
    // sublists. The empty list `[]` is encoded as `NODE_MAKE_LIST(0)`
    // so it round-trips through cell encoding.
    ParseListResult parse_list_from_string(const char *line);

#ifdef __cplusplus
}
#endif
