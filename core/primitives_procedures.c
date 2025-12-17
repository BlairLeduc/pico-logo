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
    (void)eval;
    (void)argc;
    
    // First arg must be word (procedure name)
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "define", value_to_string(args[0]));
    }
    
    // Second arg must be list [[params] [body...]]
    if (!value_is_list(args[1]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "define", value_to_string(args[1]));
    }
    
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
        return result_error_arg(ERR_TOO_FEW_ITEMS, "define", NULL);
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
                params[param_count++] = mem_word_ptr(param);
            }
            curr = mem_cdr(curr);
        }
    }
    
    // Build body list from remaining elements
    // Each element after params should be a line (list) of the body
    // We need to flatten these into a single body list
    Node body = NODE_NIL;
    Node body_tail = NODE_NIL;
    
    Node line = body_start;
    while (!mem_is_nil(line))
    {
        Node line_content = mem_car(line);
        
        // If line_content is a list, unwrap and add each element to body
        Node inner = line_content;
        if (NODE_GET_TYPE(inner) == NODE_TYPE_LIST)
        {
            // Marked as list - it's a nested list, iterate through it
            while (!mem_is_nil(inner))
            {
                Node elem = mem_car(inner);
                Node new_cons = mem_cons(elem, NODE_NIL);
                if (mem_is_nil(body))
                {
                    body = new_cons;
                    body_tail = new_cons;
                }
                else
                {
                    mem_set_cdr(body_tail, new_cons);
                    body_tail = new_cons;
                }
                inner = mem_cdr(inner);
            }
        }
        else if (mem_is_word(line_content))
        {
            // Add word as-is
            Node new_cons = mem_cons(line_content, NODE_NIL);
            if (mem_is_nil(body))
            {
                body = new_cons;
                body_tail = new_cons;
            }
            else
            {
                mem_set_cdr(body_tail, new_cons);
                body_tail = new_cons;
            }
        }
        
        line = mem_cdr(line);
    }
    
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
Result proc_define_from_text(const char *text)
{
    Lexer lexer;
    lexer_init(&lexer, text);
    
    // Skip 'to'
    Token t = lexer_next_token(&lexer);
    if (t.type != TOKEN_WORD)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "to", "expected procedure name");
    }
    
    // Get procedure name
    t = lexer_next_token(&lexer);
    if (t.type != TOKEN_WORD)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "to", NULL);
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
    // Build body as a list
    Node body = NODE_NIL;
    Node body_tail = NODE_NIL;
    
    while (t.type != TOKEN_EOF)
    {
        // Check for 'end'
        if (t.type == TOKEN_WORD && t.length == 3 &&
            strncasecmp(t.start, "end", 3) == 0)
        {
            break;
        }
        
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
        
        if (!mem_is_nil(item))
        {
            Node new_cons = mem_cons(item, NODE_NIL);
            if (mem_is_nil(body))
            {
                body = new_cons;
                body_tail = new_cons;
            }
            else
            {
                mem_set_cdr(body_tail, new_cons);
                body_tail = new_cons;
            }
        }
        
        t = lexer_next_token(&lexer);
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
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "text", value_to_string(args[0]));
    }
    
    const char *name = mem_word_ptr(args[0].as.node);
    UserProcedure *proc = proc_find(name);
    
    if (!proc)
    {
        return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
    }
    
    // Build [[params] [body]]
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
    
    // Build result: [[params] body...]
    // Mark params as a list
    Node params_list = mem_is_nil(params) ? params : NODE_MAKE_LIST(NODE_GET_INDEX(params));
    
    // Create outer list starting with params
    Node result = mem_cons(params_list, NODE_NIL);
    Node result_tail = result;
    
    // Add body as second element (as a list)
    Node body_list = mem_is_nil(proc->body) ? proc->body : NODE_MAKE_LIST(NODE_GET_INDEX(proc->body));
    Node body_cons = mem_cons(body_list, NODE_NIL);
    mem_set_cdr(result_tail, body_cons);
    
    return result_ok(value_list(result));
}

// procedurep "name - outputs true if name is a defined procedure
static Result prim_procedurep(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "procedurep", value_to_string(args[0]));
    }
    
    const char *name = mem_word_ptr(args[0].as.node);
    
    if (proc_exists(name) || primitive_find(name))
    {
        return result_ok(value_word(mem_atom("true", 4)));
    }
    return result_ok(value_word(mem_atom("false", 5)));
}

// primitivep "name - outputs true if name is a primitive
static Result prim_primitivep(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "primitivep", value_to_string(args[0]));
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
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "definedp", value_to_string(args[0]));
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
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "copydef", value_to_string(args[0]));
    }
    if (!value_is_word(args[1]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "copydef", value_to_string(args[1]));
    }
    
    const char *source_name = mem_word_ptr(args[0].as.node);
    const char *dest_name = mem_word_ptr(args[1].as.node);
    
    // Check source exists
    UserProcedure *source = proc_find(source_name);
    if (!source)
    {
        return result_error_arg(ERR_DONT_KNOW_HOW, source_name, NULL);
    }
    
    // Check destination is not a primitive
    if (primitive_find(dest_name))
    {
        return result_error_arg(ERR_IS_PRIMITIVE, dest_name, NULL);
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
    primitive_register("procedurep", 1, prim_procedurep);
    primitive_register("primitive?", 1, prim_primitivep);
    primitive_register("primitivep", 1, prim_primitivep);
    primitive_register("defined?", 1, prim_definedp);
    primitive_register("definedp", 1, prim_definedp);
    primitive_register("copydef", 2, prim_copydef);
}
