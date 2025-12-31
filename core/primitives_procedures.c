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

// define "name [[params] [body line 1] [body line 2] ...]
// Formal Logo procedure definition
static Result prim_define(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);
    REQUIRE_LIST(args[1]);
    
    const char *name = mem_word_ptr(args[0].as.node);
    Node def_list = args[1].as.node;
    
    // Check not redefining a primitive
    if (primitive_find(name))
    {
        return result_error_arg(ERR_IS_PRIMITIVE, name, NULL);
    }
    
    // First element should be parameter list
    if (mem_is_nil(def_list))
    {
        return result_error_arg(ERR_TOO_FEW_ITEMS, NULL, NULL);
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
            item = mem_atom("[", 1);
        }
        else if (t.type == TOKEN_RIGHT_BRACKET)
        {
            item = mem_atom("]", 1);
        }
        else if (t.type == TOKEN_LEFT_PAREN)
        {
            item = mem_atom("(", 1);
        }
        else if (t.type == TOKEN_RIGHT_PAREN)
        {
            item = mem_atom(")", 1);
        }
        else if (t.type == TOKEN_WORD || t.type == TOKEN_NUMBER)
        {
            item = mem_atom(t.start, t.length);
        }
        else if (t.type == TOKEN_QUOTED)
        {
            item = mem_atom(t.start, t.length);  // Keep the quote
        }
        else if (t.type == TOKEN_COLON)
        {
            item = mem_atom(t.start, t.length);  // Keep the colon
        }
        else if (t.type == TOKEN_PLUS)
        {
            item = mem_atom("+", 1);
        }
        else if (t.type == TOKEN_MINUS || t.type == TOKEN_UNARY_MINUS)
        {
            item = mem_atom("-", 1);
        }
        else if (t.type == TOKEN_MULTIPLY)
        {
            item = mem_atom("*", 1);
        }
        else if (t.type == TOKEN_DIVIDE)
        {
            item = mem_atom("/", 1);
        }
        else if (t.type == TOKEN_EQUALS)
        {
            item = mem_atom("=", 1);
        }
        else if (t.type == TOKEN_LESS_THAN)
        {
            item = mem_atom("<", 1);
        }
        else if (t.type == TOKEN_GREATER_THAN)
        {
            item = mem_atom(">", 1);
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
    (void)eval;
    (void)argc;
    
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    const char *name = mem_word_ptr(args[0].as.node);
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
    (void)eval;
    (void)argc;
    
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    const char *name = mem_word_ptr(args[0].as.node);
    
    if (primitive_find(name))
    {
        return result_ok(value_word(mem_atom("true", 4)));
    }
    return result_ok(value_word(mem_atom("false", 5)));
}

// definedp "name - outputs true if name is a user-defined procedure
static Result prim_definedp(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    const char *name = mem_word_ptr(args[0].as.node);
    
    if (proc_exists(name))
    {
        return result_ok(value_word(mem_atom("true", 4)));
    }
    return result_ok(value_word(mem_atom("false", 5)));
}

// copydef "name "newname - copies the definition of name to newname
static Result prim_copydef(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    if (!value_is_word(args[1]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }
    
    const char *source_name = mem_word_ptr(args[0].as.node);
    const char *dest_name = mem_word_ptr(args[1].as.node);
    
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
