//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Procedure definition primitives: to, define, end
//

#include "primitives.h"
#include "procedures.h"
#include "error.h"
#include "eval.h"
#include "lexer.h"
#include <string.h>

// Forward declaration for recursive bracket parsing
static Node parse_bracket_contents(Lexer *lexer, Token *t, int pending_newline_count);

// Helper to append a node to a list, returning updated tail
static Node append_to_list(Node *list, Node *tail, Node item)
{
    Node new_cons = mem_cons(item, NODE_NIL);
    if (mem_is_nil(*list))
    {
        *list = new_cons;
    }
    else
    {
        mem_set_cdr(*tail, new_cons);
    }
    return new_cons;
}

// Helper to create a token atom from a token
static Node token_to_atom(Token *t)
{
    switch (t->type)
    {
        case TOKEN_LEFT_PAREN:
            return mem_atom("(", 1);
        case TOKEN_RIGHT_PAREN:
            return mem_atom(")", 1);
        case TOKEN_WORD:
        case TOKEN_NUMBER:
            return mem_atom(t->start, t->length);
        case TOKEN_QUOTED:
            return mem_atom(t->start, t->length);  // Keep the quote
        case TOKEN_COLON:
            return mem_atom(t->start, t->length);  // Keep the colon
        case TOKEN_PLUS:
            return mem_atom("+", 1);
        case TOKEN_MINUS:
        case TOKEN_UNARY_MINUS:
            return mem_atom("-", 1);
        case TOKEN_MULTIPLY:
            return mem_atom("*", 1);
        case TOKEN_DIVIDE:
            return mem_atom("/", 1);
        case TOKEN_EQUALS:
            return mem_atom("=", 1);
        case TOKEN_LESS_THAN:
            return mem_atom("<", 1);
        case TOKEN_GREATER_THAN:
            return mem_atom(">", 1);
        default:
            return NODE_NIL;
    }
}

// Parse bracket contents recursively until matching ]
// Returns a list of items, with nested brackets as sublists
// Updates *t to the token after ]
// pending_newline_count tracks newlines that occurred before the next token
static Node parse_bracket_contents(Lexer *lexer, Token *t, int pending_newline_count)
{
    Node list = NODE_NIL;
    Node tail = NODE_NIL;
    
    while (t->type != TOKEN_EOF && t->type != TOKEN_RIGHT_BRACKET)
    {
        // Add any pending newlines that preceded this token
        // The lexer tracks newlines in had_newline/newline_count
        if (lexer->had_newline)
        {
            pending_newline_count += lexer->newline_count;
            lexer->had_newline = false;  // Consume the newline flag
        }
        
        // Insert newline markers BEFORE the current token
        while (pending_newline_count > 0)
        {
            tail = append_to_list(&list, &tail, mem_newline_marker);
            pending_newline_count--;
        }
        
        Node item = NODE_NIL;
        bool is_nested_list = false;
        
        if (t->type == TOKEN_LEFT_BRACKET)
        {
            // Recursively parse nested brackets
            *t = lexer_next_token(lexer);
            item = parse_bracket_contents(lexer, t, 0);
            // Mark as nested list - even empty lists should be stored!
            item = NODE_MAKE_LIST(NODE_GET_INDEX(item));
            is_nested_list = true;
            // Don't advance token - recursive call already positioned past ]
        }
        else
        {
            item = token_to_atom(t);
            *t = lexer_next_token(lexer);
        }
        
        // Always append nested lists (including empty ones [])
        // Only skip nil items from token_to_atom (unexpected tokens)
        if (is_nested_list || !mem_is_nil(item))
        {
            tail = append_to_list(&list, &tail, item);
        }
    }
    
    // Capture any newlines that preceded the closing ]
    // These newlines mean the ] should appear on its own line
    if (t->type == TOKEN_RIGHT_BRACKET && lexer->had_newline)
    {
        pending_newline_count += lexer->newline_count;
        lexer->had_newline = false;
        while (pending_newline_count > 0)
        {
            tail = append_to_list(&list, &tail, mem_newline_marker);
            pending_newline_count--;
        }
    }
    
    // Skip the closing ]
    if (t->type == TOKEN_RIGHT_BRACKET)
    {
        *t = lexer_next_token(lexer);
    }
    
    return list;
}

// define "name [[params] [body line 1] [body line 2] ...]
// Formal Logo procedure definition
static Result prim_define(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD_STR(args[0], name);
    REQUIRE_LIST_NONEMPTY(args[1], def_list);
    
    // Check not redefining a primitive
    if (primitive_find(name))
    {
        return result_error_arg(ERR_IS_PRIMITIVE, name, NULL);
    }
    
    Node params_elem = mem_car(def_list);
    Node body_start = mem_cdr(def_list);
    
    // Parse parameters
    const char *params[MAX_PROC_PARAMS];
    int param_count = 0;
    
    // params_elem should be a list (possibly empty) - could be marked or unmarked
    // When stored in a list, nested lists are marked with NODE_TYPE_LIST
    Node param_list = params_elem;
    if (NODE_GET_TYPE(param_list) == NODE_TYPE_LIST)
    {
        // It's marked as a list reference, get actual index
        param_list = NODE_MAKE_LIST(NODE_GET_INDEX(param_list));
    }
    
    if (mem_is_list(param_list) || mem_is_nil(param_list))
    {
        Node curr = param_list;
        while (!mem_is_nil(curr) && param_count < MAX_PROC_PARAMS)
        {
            Node param = mem_car(curr);
            if (mem_is_word(param))
            {
                // Parameter name - intern it
                // Skip leading colon if present (e.g., ":x" -> "x")
                const char *pname = mem_word_ptr(param);
                if (pname[0] == ':')
                {
                    pname++;  // Skip the colon
                }
                params[param_count++] = pname;
            }
            curr = mem_cdr(curr);
        }
    }
    
    // Body is the remaining elements (list of line-lists)
    // Store body_start directly as list-of-lists
    Node body = body_start;
    
    // Define the procedure
    if (!proc_define(name, params, param_count, body))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }
    
    return result_none();
}

// Simple text-based procedure definition for testing
// This parses: to name :param1 :param2 ... body... end
// Used when we have the full definition as a string
// Body is stored as list-of-lists: [[line1-tokens] [line2-tokens] ...]
Result proc_define_from_text(const char *text)
{
    Lexer lexer;
    lexer_init(&lexer, text);
    
    // Skip 'to'
    Token t = lexer_next_token(&lexer);
    if (t.type != TOKEN_WORD)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, "expected procedure name");
    }
    
    // Get procedure name
    t = lexer_next_token(&lexer);
    if (t.type != TOKEN_WORD)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, NULL, NULL);
    }
    
    Node name_atom = mem_atom(t.start, t.length);
    const char *name = mem_word_ptr(name_atom);
    
    // Check not redefining a primitive
    if (primitive_find(name))
    {
        return result_error_arg(ERR_IS_PRIMITIVE, name, NULL);
    }
    
    // Parse parameters (words starting with :)
    const char *params[MAX_PROC_PARAMS];
    int param_count = 0;
    
    while (true)
    {
        t = lexer_next_token(&lexer);
        if (t.type == TOKEN_EOF)
            break;
        if (t.type == TOKEN_COLON && param_count < MAX_PROC_PARAMS)
        {
            // :param - skip the colon
            Node param_atom = mem_atom(t.start + 1, t.length - 1);
            params[param_count++] = mem_word_ptr(param_atom);
        }
        else
        {
            // Not a parameter - must be start of body
            break;
        }
    }
    
    // Now collect body until 'end'
    // Build body as list-of-lists: [[line1] [line2] ...]
    // Note: 'end' must be at the start of a line - either right after parsing params
    // or right after a newline marker (\n) or an actual newline character
    Node body = NODE_NIL;           // Outer list of lines
    Node body_tail = NODE_NIL;
    Node current_line = NODE_NIL;   // Current line being built
    Node current_line_tail = NODE_NIL;
    bool at_line_start = lexer.had_newline;  // Check if we crossed a newline to get here
    bool body_started = false;      // Have we added any content to body?
    
    while (t.type != TOKEN_EOF)
    {
        // Check for 'end' only at line start
        if (at_line_start && t.type == TOKEN_WORD && t.length == 3 &&
            strncasecmp(t.start, "end", 3) == 0)
        {
            break;
        }
        
        at_line_start = false;
        
        // Build token node
        Node item = NODE_NIL;
        
        if (t.type == TOKEN_LEFT_BRACKET)
        {
            // Recursively parse bracket contents to create a nested list
            // This handles brackets spanning multiple lines
            t = lexer_next_token(&lexer);
            item = parse_bracket_contents(&lexer, &t, 0);
            // Mark as nested list
            item = NODE_MAKE_LIST(NODE_GET_INDEX(item));
            
            // Don't advance token - parse_bracket_contents already did
            // Add to current line and continue (skip the normal lexer_next_token)
            body_started = true;
            Node new_cons = mem_cons(item, NODE_NIL);
            if (mem_is_nil(current_line))
            {
                current_line = new_cons;
                current_line_tail = new_cons;
            }
            else
            {
                mem_set_cdr(current_line_tail, new_cons);
                current_line_tail = new_cons;
            }
            
            // Check if we crossed newlines while parsing bracket contents
            if (lexer.had_newline)
            {
                int newline_count = lexer.newline_count;
                lexer.had_newline = false;
                lexer.newline_count = 0;
                
                // Finish current line
                Node line_to_add = mem_is_nil(current_line) ? NODE_NIL : 
                                   NODE_MAKE_LIST(NODE_GET_INDEX(current_line));
                Node line_cons = mem_cons(line_to_add, NODE_NIL);
                
                if (mem_is_nil(body))
                {
                    body = line_cons;
                    body_tail = line_cons;
                }
                else
                {
                    mem_set_cdr(body_tail, line_cons);
                    body_tail = line_cons;
                }
                
                // Additional newlines create empty lines
                for (int i = 1; i < newline_count; i++)
                {
                    Node empty_line = mem_cons(NODE_NIL, NODE_NIL);
                    mem_set_cdr(body_tail, empty_line);
                    body_tail = empty_line;
                }
                
                // Start new line
                current_line = NODE_NIL;
                current_line_tail = NODE_NIL;
                at_line_start = true;
            }
            continue;  // Skip the normal token handling below
        }
        else if (t.type == TOKEN_RIGHT_BRACKET)
        {
            // Stray ] without matching [ - store as atom for error handling
            item = mem_atom("]", 1);
        }
        else
        {
            // Use helper for all other token types
            item = token_to_atom(&t);
        }
        
        // Add item to current line
        if (!mem_is_nil(item))
        {
            body_started = true;  // We have actual content
            Node new_cons = mem_cons(item, NODE_NIL);
            if (mem_is_nil(current_line))
            {
                current_line = new_cons;
                current_line_tail = new_cons;
            }
            else
            {
                mem_set_cdr(current_line_tail, new_cons);
                current_line_tail = new_cons;
            }
        }
        
        t = lexer_next_token(&lexer);
        
        // Check if the lexer crossed newline(s) to get the next token
        if (lexer.had_newline)
        {
            int newline_count = lexer.newline_count;
            lexer.had_newline = false;
            lexer.newline_count = 0;
            
            // Skip leading newlines before body starts
            if (!body_started && mem_is_nil(current_line))
            {
                at_line_start = true;
                continue;
            }
            
            // First newline finishes the current line
            Node line_to_add = mem_is_nil(current_line) ? NODE_NIL : 
                               NODE_MAKE_LIST(NODE_GET_INDEX(current_line));
            Node line_cons = mem_cons(line_to_add, NODE_NIL);
            
            if (mem_is_nil(body))
            {
                body = line_cons;
                body_tail = line_cons;
            }
            else
            {
                mem_set_cdr(body_tail, line_cons);
                body_tail = line_cons;
            }
            
            // Additional newlines (newline_count > 1) create empty lines
            for (int i = 1; i < newline_count; i++)
            {
                Node empty_line = mem_cons(NODE_NIL, NODE_NIL);
                mem_set_cdr(body_tail, empty_line);
                body_tail = empty_line;
            }
            
            // Start new line
            current_line = NODE_NIL;
            current_line_tail = NODE_NIL;
            at_line_start = true;
        }
    }
    
    // Don't forget the last line if it wasn't terminated by newline
    if (!mem_is_nil(current_line))
    {
        Node line_to_add = NODE_MAKE_LIST(NODE_GET_INDEX(current_line));
        Node line_cons = mem_cons(line_to_add, NODE_NIL);
        
        if (mem_is_nil(body))
        {
            body = line_cons;
            body_tail = line_cons;
        }
        else
        {
            mem_set_cdr(body_tail, line_cons);
            body_tail = line_cons;
        }
    }
    
    // Define the procedure
    if (!proc_define(name, params, param_count, body))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }
    
    return result_ok(value_word(name_atom));
}

// text "name - outputs the text (definition) of a procedure as a list
static Result prim_text(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    REQUIRE_WORD_STR(args[0], name);
    
    UserProcedure *proc = proc_find(name);
    
    if (!proc)
    {
        return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
    }
    
    // Build [[params] [line1] [line2] ...]
    // First build params list
    Node params = NODE_NIL;
    Node params_tail = NODE_NIL;
    
    for (int i = 0; i < proc->param_count; i++)
    {
        Node param = mem_atom(proc->params[i], strlen(proc->params[i]));
        Node new_cons = mem_cons(param, NODE_NIL);
        if (mem_is_nil(params))
        {
            params = new_cons;
            params_tail = new_cons;
        }
        else
        {
            mem_set_cdr(params_tail, new_cons);
            params_tail = new_cons;
        }
    }
    
    // Mark params as a list for proper nesting
    Node params_list = mem_is_nil(params) ? params : NODE_MAKE_LIST(NODE_GET_INDEX(params));
    
    // Body is already stored as list-of-lists [[line1] [line2] ...]
    // Prepend params to create [[params] [line1] [line2] ...]
    Node result = mem_cons(params_list, proc->body);
    
    return result_ok(value_list(result));
}

// primitivep "name - outputs true if name is a primitive
static Result prim_primitivep(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD_STR(args[0], name);
    
    if (primitive_find(name))
    {
        return result_ok(value_word(mem_atom("true", 4)));
    }
    return result_ok(value_word(mem_atom("false", 5)));
}

// definedp "name - outputs true if name is a user-defined procedure
static Result prim_definedp(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD_STR(args[0], name);
    
    if (proc_exists(name))
    {
        return result_ok(value_word(mem_atom("true", 4)));
    }
    return result_ok(value_word(mem_atom("false", 5)));
}

// copydef "name "newname - copies the definition of name to newname
static Result prim_copydef(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD_STR(args[0], source_name);
    REQUIRE_WORD_STR(args[1], dest_name);
    
    // Check destination is not already a primitive
    if (primitive_find(dest_name))
    {
        return result_error_arg(ERR_IS_PRIMITIVE, dest_name, NULL);
    }
    
    // Check if source is a primitive
    const Primitive *source_prim = primitive_find(source_name);
    if (source_prim)
    {
        // Create alias to the primitive
        // The dest_name is already interned (from mem_word_ptr)
        if (!primitive_register_alias(dest_name, source_prim))
        {
            return result_error(ERR_OUT_OF_SPACE);
        }
        return result_none();
    }
    
    // Check if source is a user-defined procedure
    UserProcedure *source = proc_find(source_name);
    if (!source)
    {
        return result_error_arg(ERR_DONT_KNOW_HOW, source_name, NULL);
    }
    
    // Define new procedure with same params and body
    if (!proc_define(dest_name, source->params, source->param_count, source->body))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }
    
    return result_none();
}

void primitives_procedures_init(void)
{
    primitive_register("define", 2, prim_define);
    primitive_register("text", 1, prim_text);
    primitive_register("primitive?", 1, prim_primitivep);
    primitive_register("primitivep", 1, prim_primitivep);
    primitive_register("defined?", 1, prim_definedp);
    primitive_register("definedp", 1, prim_definedp);
    primitive_register("copydef", 2, prim_copydef);
}
