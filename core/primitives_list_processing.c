//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  List processing primitives: apply, foreach, map, filter, find, reduce, crossmap
//
//  These primitives accept three forms of procedure specification:
//  1. Procedure name (word): "sum, "double
//  2. Lambda expression: [[x] :x + 1]
//  3. Procedure text: [[x y] [output :x + :y]]
//

#include "primitives.h"
#include "procedures.h"
#include "eval.h"
#include "format.h"
#include "value.h"
#include "variables.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

//==========================================================================
// Procedure Specification Types
//==========================================================================

// Type of procedure specification
typedef enum {
    PROC_SPEC_NAME,      // Named procedure (primitive or user-defined)
    PROC_SPEC_LAMBDA,    // Lambda expression: [[params] expr]
    PROC_SPEC_TEXT       // Procedure text: [[params] [line1] [line2]...]
} ProcSpecType;

// Parsed procedure specification
typedef struct {
    ProcSpecType type;
    union {
        struct {
            const Primitive *primitive;  // For primitive procedures
            UserProcedure *user_proc;    // For user-defined procedures
        } named;
        struct {
            const char *params[MAX_PROC_PARAMS];  // Parameter names
            int param_count;
            Node body;                            // Expression or body list
            bool is_expression;                   // True if body is a single expression
        } lambda;
    } as;
} ProcSpec;

//==========================================================================
// Helper: Parse procedure specification from Value
//==========================================================================

// Parse a procedure specification from a value
// Returns RESULT_OK with ProcSpec filled in, or RESULT_ERROR
static Result parse_proc_spec(Value proc_arg, ProcSpec *spec)
{
    if (value_is_word(proc_arg))
    {
        // Named procedure
        const char *name = mem_word_ptr(proc_arg.as.node);
        
        // First check for primitive
        const Primitive *prim = primitive_find(name);
        if (prim)
        {
            spec->type = PROC_SPEC_NAME;
            spec->as.named.primitive = prim;
            spec->as.named.user_proc = NULL;
            return result_ok(value_none());
        }
        
        // Check for user-defined procedure
        UserProcedure *user_proc = proc_find(name);
        if (user_proc)
        {
            spec->type = PROC_SPEC_NAME;
            spec->as.named.primitive = NULL;
            spec->as.named.user_proc = user_proc;
            return result_ok(value_none());
        }
        
        // Unknown procedure
        return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
    }
    else if (value_is_list(proc_arg))
    {
        // Lambda expression or procedure text
        // Format: [[param1 param2 ...] body...]
        // Lambda: [[x] :x + 1] - single expression after params
        // Text: [[x y] [output :x + :y]] - list of lines after params
        
        Node list = proc_arg.as.node;
        if (mem_is_nil(list))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, "[]");
        }
        
        // First element must be the parameter list
        Node first = mem_car(list);
        if (!mem_is_nil(first) && !mem_is_list(first))
        {
            // First element is not a list - could be params as direct words
            // Actually, per Logo convention, params must be in a list
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(proc_arg));
        }
        
        // Parse parameter names from the first element (parameter list)
        Node param_list = first;
        int param_count = 0;
        const char *params[MAX_PROC_PARAMS];
        
        while (!mem_is_nil(param_list) && param_count < MAX_PROC_PARAMS)
        {
            Node param_node = mem_car(param_list);
            if (!mem_is_word(param_node))
            {
                return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(proc_arg));
            }
            params[param_count++] = mem_word_ptr(param_node);
            param_list = mem_cdr(param_list);
        }
        
        // Rest of the list is the body
        Node rest = mem_cdr(list);
        if (mem_is_nil(rest))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(proc_arg));
        }
        
        // Determine if this is a lambda (expression) or procedure text (lines)
        // Lambda: [[x] :x + 1] - rest contains tokens for a single expression
        // Text: [[x] [output :x + 1]] - rest contains line-lists
        //
        // We check if the first element after params is a list (indicates text format)
        // or words/atoms (indicates lambda expression)
        Node first_body = mem_car(rest);
        bool is_expression;
        
        if (mem_is_list(first_body))
        {
            // Procedure text format: body is a list of line-lists
            is_expression = false;
        }
        else
        {
            // Lambda format: body is inline tokens forming an expression
            is_expression = true;
        }
        
        spec->type = is_expression ? PROC_SPEC_LAMBDA : PROC_SPEC_TEXT;
        spec->as.lambda.param_count = param_count;
        for (int i = 0; i < param_count; i++)
        {
            spec->as.lambda.params[i] = params[i];
        }
        spec->as.lambda.body = rest;
        spec->as.lambda.is_expression = is_expression;
        
        return result_ok(value_none());
    }
    
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(proc_arg));
}

//==========================================================================
// Helper: Invoke a procedure specification with arguments
//==========================================================================

// Invoke a procedure specification with the given arguments
// Returns the result of the procedure call
static Result invoke_proc_spec(Evaluator *eval, ProcSpec *spec, int argc, Value *args)
{
    if (spec->type == PROC_SPEC_NAME)
    {
        if (spec->as.named.primitive)
        {
            // Call primitive directly
            return spec->as.named.primitive->func(eval, argc, args);
        }
        else if (spec->as.named.user_proc)
        {
            // Call user procedure
            return proc_call(eval, spec->as.named.user_proc, argc, args);
        }
        // Should never reach here - parse_proc_spec validates the input
        return result_error(ERR_UNDEFINED);
    }
    else
    {
        // Lambda or procedure text
        // We need to set up local variables for parameters and run the body
        
        // Validate argument count
        if (argc < spec->as.lambda.param_count)
        {
            return result_error_arg(ERR_NOT_ENOUGH_INPUTS, NULL, NULL);
        }
        if (argc > spec->as.lambda.param_count)
        {
            return result_error_arg(ERR_TOO_MANY_INPUTS, NULL, NULL);
        }
        
        // For lambda expressions, we create local bindings and evaluate
        // We need to save/restore any existing variables with the same names
        Value saved_values[MAX_PROC_PARAMS];
        bool had_values[MAX_PROC_PARAMS];
        
        // Save existing values and set up parameters
        for (int i = 0; i < spec->as.lambda.param_count; i++)
        {
            const char *param_name = spec->as.lambda.params[i];
            
            // Check if variable exists and save its value
            had_values[i] = var_get(param_name, &saved_values[i]);
            
            // Set the parameter value
            if (!var_set(param_name, args[i]))
            {
                // Restore already-set variables on failure
                for (int j = 0; j < i; j++)
                {
                    if (had_values[j])
                    {
                        var_set(spec->as.lambda.params[j], saved_values[j]);
                    }
                    else
                    {
                        var_erase(spec->as.lambda.params[j]);
                    }
                }
                return result_error(ERR_OUT_OF_SPACE);
            }
        }
        
        // Execute the body
        Result r;
        if (spec->as.lambda.is_expression)
        {
            // Lambda expression: evaluate as expression
            r = eval_run_list_expr(eval, spec->as.lambda.body);
        }
        else
        {
            // Procedure text: run as list of lines, looking for output
            Node body = spec->as.lambda.body;
            r = result_none();
            
            while (!mem_is_nil(body))
            {
                Node line = mem_car(body);
                Node line_tokens = line;
                
                // Handle list marker
                if (NODE_GET_TYPE(line) == NODE_TYPE_LIST)
                {
                    line_tokens = NODE_MAKE_LIST(NODE_GET_INDEX(line));
                }
                
                // Skip empty lines
                if (!mem_is_nil(line_tokens))
                {
                    r = eval_run_list_expr(eval, line_tokens);
                    
                    // If we got an output, that's our return value
                    if (r.status == RESULT_OUTPUT)
                    {
                        r = result_ok(r.value);
                        break;
                    }
                    
                    // Propagate errors, stop, throw
                    if (r.status == RESULT_ERROR || r.status == RESULT_THROW ||
                        r.status == RESULT_STOP)
                    {
                        break;
                    }
                }
                
                body = mem_cdr(body);
            }
        }
        
        // Restore original variable state
        for (int i = 0; i < spec->as.lambda.param_count; i++)
        {
            if (had_values[i])
            {
                var_set(spec->as.lambda.params[i], saved_values[i]);
            }
            else
            {
                var_erase(spec->as.lambda.params[i]);
            }
        }
        
        return r;
    }
}

//==========================================================================
// apply procedure inputlist
//==========================================================================

// apply procedure inputlist
// Runs procedure with inputs from inputlist
static Result prim_apply(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    
    // Parse the procedure specification
    ProcSpec spec;
    Result r = parse_proc_spec(args[0], &spec);
    if (r.status == RESULT_ERROR)
    {
        return r;
    }
    
    // Second argument must be a list of inputs
    REQUIRE_LIST(args[1]);
    Node input_list = args[1].as.node;
    
    // Collect inputs from the list
    Value proc_args[MAX_PROC_PARAMS];
    int proc_argc = 0;
    
    while (!mem_is_nil(input_list) && proc_argc < MAX_PROC_PARAMS)
    {
        Node elem = mem_car(input_list);
        
        if (mem_is_word(elem))
        {
            proc_args[proc_argc++] = value_word(elem);
        }
        else if (mem_is_nil(elem))
        {
            proc_args[proc_argc++] = value_list(NODE_NIL);
        }
        else
        {
            proc_args[proc_argc++] = value_list(elem);
        }
        
        input_list = mem_cdr(input_list);
    }
    
    // Invoke the procedure with collected arguments
    return invoke_proc_spec(eval, &spec, proc_argc, proc_args);
}

//==========================================================================
// foreach data procedure
// (foreach data1 data2 ... procedure)
//==========================================================================

static Result prim_foreach(Evaluator *eval, int argc, Value *args)
{
    if (argc < 2)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, NULL, NULL);
    }
    
    // Last argument is the procedure
    ProcSpec spec;
    Result r = parse_proc_spec(args[argc - 1], &spec);
    if (r.status == RESULT_ERROR)
    {
        return r;
    }
    
    // Determine expected parameter count
    int expected_params;
    if (spec.type == PROC_SPEC_NAME)
    {
        if (spec.as.named.primitive)
        {
            expected_params = spec.as.named.primitive->default_args;
        }
        else
        {
            expected_params = spec.as.named.user_proc->param_count;
        }
    }
    else
    {
        expected_params = spec.as.lambda.param_count;
    }
    
    // Number of data lists
    int data_count = argc - 1;
    
    // Validate that we have the right number of data lists
    if (data_count != expected_params)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, NULL, NULL);
    }
    
    // Validate all data arguments are lists, words, or numbers
    // Convert numbers to words (numbers are self-quoting words in Logo)
    Node word_nodes[MAX_PROC_PARAMS];  // Store converted number nodes
    for (int i = 0; i < data_count; i++)
    {
        if (value_is_number(args[i]))
        {
            word_nodes[i] = number_to_word(args[i].as.number);
        }
        else if (value_is_word(args[i]))
        {
            word_nodes[i] = args[i].as.node;
        }
        else if (!value_is_list(args[i]))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[i]));
        }
        else
        {
            word_nodes[i] = NODE_NIL;  // Mark as list
        }
    }
    
    // Get length of first data list/word (all must be same length for multi-list)
    int length = 0;
    if (value_is_list(args[0]))
    {
        Node list = args[0].as.node;
        while (!mem_is_nil(list))
        {
            length++;
            list = mem_cdr(list);
        }
    }
    else
    {
        // Word or number - length is character count
        length = (int)mem_word_len(word_nodes[0]);
    }
    
    // For multi-list, verify all lists have same length
    if (data_count > 1)
    {
        for (int i = 1; i < data_count; i++)
        {
            int other_length = 0;
            if (value_is_list(args[i]))
            {
                Node list = args[i].as.node;
                while (!mem_is_nil(list))
                {
                    other_length++;
                    list = mem_cdr(list);
                }
            }
            else
            {
                // Use word_nodes which has already converted numbers
                other_length = (int)mem_word_len(word_nodes[i]);
            }
            
            if (other_length != length)
            {
                return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[i]));
            }
        }
    }
    
    // Set up iterators for each data list
    Node cursors[MAX_PROC_PARAMS];
    const char *word_ptrs[MAX_PROC_PARAMS];
    bool is_word[MAX_PROC_PARAMS];
    
    for (int i = 0; i < data_count; i++)
    {
        if (value_is_list(args[i]))
        {
            cursors[i] = args[i].as.node;
            is_word[i] = false;
        }
        else
        {
            // Use word_nodes which has already converted numbers
            word_ptrs[i] = mem_word_ptr(word_nodes[i]);
            is_word[i] = true;
        }
    }
    
    // Iterate through all elements
    Value proc_args[MAX_PROC_PARAMS];
    
    for (int idx = 0; idx < length; idx++)
    {
        // Collect current element from each data source
        for (int i = 0; i < data_count; i++)
        {
            if (is_word[i])
            {
                // Get character at current position
                Node char_atom = mem_atom(&word_ptrs[i][idx], 1);
                proc_args[i] = value_word(char_atom);
            }
            else
            {
                // Get element from list
                Node elem = mem_car(cursors[i]);
                if (mem_is_word(elem))
                {
                    proc_args[i] = value_word(elem);
                }
                else if (mem_is_nil(elem))
                {
                    proc_args[i] = value_list(NODE_NIL);
                }
                else
                {
                    proc_args[i] = value_list(elem);
                }
                cursors[i] = mem_cdr(cursors[i]);
            }
        }
        
        // Invoke the procedure
        r = invoke_proc_spec(eval, &spec, data_count, proc_args);
        
        // Propagate errors, stop, throw
        if (r.status == RESULT_ERROR || r.status == RESULT_THROW ||
            r.status == RESULT_STOP || r.status == RESULT_OUTPUT)
        {
            return r;
        }
    }
    
    return result_none();
}

//==========================================================================
// map procedure data
// (map procedure data1 data2 ...)
//==========================================================================

static Result prim_map(Evaluator *eval, int argc, Value *args)
{
    if (argc < 2)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, NULL, NULL);
    }
    
    // First argument is the procedure
    ProcSpec spec;
    Result r = parse_proc_spec(args[0], &spec);
    if (r.status == RESULT_ERROR)
    {
        return r;
    }
    
    // Determine expected parameter count
    int expected_params;
    if (spec.type == PROC_SPEC_NAME)
    {
        if (spec.as.named.primitive)
        {
            expected_params = spec.as.named.primitive->default_args;
        }
        else
        {
            expected_params = spec.as.named.user_proc->param_count;
        }
    }
    else
    {
        expected_params = spec.as.lambda.param_count;
    }
    
    // Number of data lists
    int data_count = argc - 1;
    
    // Validate that we have the right number of data lists
    if (data_count != expected_params)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, NULL, NULL);
    }
    
    // Validate all data arguments are lists, words, or numbers
    // Convert numbers to words (numbers are self-quoting words in Logo)
    Node word_nodes[MAX_PROC_PARAMS];  // Store converted number nodes
    for (int i = 1; i < argc; i++)
    {
        if (value_is_number(args[i]))
        {
            word_nodes[i - 1] = number_to_word(args[i].as.number);
        }
        else if (value_is_word(args[i]))
        {
            word_nodes[i - 1] = args[i].as.node;
        }
        else if (!value_is_list(args[i]))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[i]));
        }
        else
        {
            word_nodes[i - 1] = NODE_NIL;  // Mark as list
        }
    }
    
    // Get length of first data list/word (all must be same length for multi-list)
    int length = 0;
    if (value_is_list(args[1]))
    {
        Node list = args[1].as.node;
        while (!mem_is_nil(list))
        {
            length++;
            list = mem_cdr(list);
        }
    }
    else
    {
        // Word or number - use word_nodes which has converted numbers
        length = (int)mem_word_len(word_nodes[0]);
    }
    
    // For multi-list, verify all lists have same length
    if (data_count > 1)
    {
        for (int i = 2; i < argc; i++)
        {
            int other_length = 0;
            if (value_is_list(args[i]))
            {
                Node list = args[i].as.node;
                while (!mem_is_nil(list))
                {
                    other_length++;
                    list = mem_cdr(list);
                }
            }
            else
            {
                // Use word_nodes which has already converted numbers
                other_length = (int)mem_word_len(word_nodes[i - 1]);
            }
            
            if (other_length != length)
            {
                return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[i]));
            }
        }
    }
    
    // Set up iterators for each data list
    Node cursors[MAX_PROC_PARAMS];
    const char *word_ptrs[MAX_PROC_PARAMS];
    bool is_word[MAX_PROC_PARAMS];
    
    for (int i = 0; i < data_count; i++)
    {
        if (value_is_list(args[i + 1]))
        {
            cursors[i] = args[i + 1].as.node;
            is_word[i] = false;
        }
        else
        {
            // Use word_nodes which has already converted numbers
            word_ptrs[i] = mem_word_ptr(word_nodes[i]);
            is_word[i] = true;
        }
    }
    
    // Build result list
    Node result_head = NODE_NIL;
    Node result_tail = NODE_NIL;
    
    Value proc_args[MAX_PROC_PARAMS];
    
    for (int idx = 0; idx < length; idx++)
    {
        // Collect current element from each data source
        for (int i = 0; i < data_count; i++)
        {
            if (is_word[i])
            {
                Node char_atom = mem_atom(&word_ptrs[i][idx], 1);
                proc_args[i] = value_word(char_atom);
            }
            else
            {
                Node elem = mem_car(cursors[i]);
                if (mem_is_word(elem))
                {
                    proc_args[i] = value_word(elem);
                }
                else if (mem_is_nil(elem))
                {
                    proc_args[i] = value_list(NODE_NIL);
                }
                else
                {
                    proc_args[i] = value_list(elem);
                }
                cursors[i] = mem_cdr(cursors[i]);
            }
        }
        
        // Invoke the procedure
        r = invoke_proc_spec(eval, &spec, data_count, proc_args);
        
        // Propagate errors, stop, throw
        if (r.status == RESULT_ERROR || r.status == RESULT_THROW ||
            r.status == RESULT_STOP)
        {
            return r;
        }
        
        // Handle output from procedure text
        if (r.status == RESULT_OUTPUT)
        {
            r = result_ok(r.value);
        }
        
        // Collect result if we got one
        if (r.status == RESULT_OK)
        {
            Node result_node;
            if (value_is_word(r.value) || value_is_number(r.value))
            {
                if (value_is_number(r.value))
                {
                    // Convert number to word for list
                    char buf[32];
                    format_number(buf, sizeof(buf), r.value.as.number);
                    result_node = mem_atom_cstr(buf);
                }
                else
                {
                    result_node = r.value.as.node;
                }
            }
            else if (value_is_list(r.value))
            {
                result_node = r.value.as.node;
            }
            else
            {
                continue;  // Skip none values
            }
            
            // Append to result list
            Node new_cell = mem_cons(result_node, NODE_NIL);
            if (mem_is_nil(result_head))
            {
                result_head = new_cell;
                result_tail = new_cell;
            }
            else
            {
                mem_set_cdr(result_tail, new_cell);
                result_tail = new_cell;
            }
        }
    }
    
    return result_ok(value_list(result_head));
}

//==========================================================================
// map.se procedure data
// (map.se procedure data1 data2 ...)
//==========================================================================

static Result prim_map_se(Evaluator *eval, int argc, Value *args)
{
    if (argc < 2)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, NULL, NULL);
    }
    
    // First argument is the procedure
    ProcSpec spec;
    Result r = parse_proc_spec(args[0], &spec);
    if (r.status == RESULT_ERROR)
    {
        return r;
    }
    
    // Determine expected parameter count
    int expected_params;
    if (spec.type == PROC_SPEC_NAME)
    {
        if (spec.as.named.primitive)
        {
            expected_params = spec.as.named.primitive->default_args;
        }
        else
        {
            expected_params = spec.as.named.user_proc->param_count;
        }
    }
    else
    {
        expected_params = spec.as.lambda.param_count;
    }
    
    // Number of data lists
    int data_count = argc - 1;
    
    // Validate that we have the right number of data lists
    if (data_count != expected_params)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, NULL, NULL);
    }
    
    // Validate all data arguments are lists, words, or numbers
    // Convert numbers to words (numbers are self-quoting words in Logo)
    Node word_nodes[MAX_PROC_PARAMS];  // Store converted number nodes
    for (int i = 1; i < argc; i++)
    {
        if (value_is_number(args[i]))
        {
            word_nodes[i - 1] = number_to_word(args[i].as.number);
        }
        else if (value_is_word(args[i]))
        {
            word_nodes[i - 1] = args[i].as.node;
        }
        else if (!value_is_list(args[i]))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[i]));
        }
        else
        {
            word_nodes[i - 1] = NODE_NIL;  // Mark as list
        }
    }
    
    // Get length of first data list/word (all must be same length for multi-list)
    int length = 0;
    if (value_is_list(args[1]))
    {
        Node list = args[1].as.node;
        while (!mem_is_nil(list))
        {
            length++;
            list = mem_cdr(list);
        }
    }
    else
    {
        // Word or number - use word_nodes which has converted numbers
        length = (int)mem_word_len(word_nodes[0]);
    }
    
    // For multi-list, verify all lists have same length
    if (data_count > 1)
    {
        for (int i = 2; i < argc; i++)
        {
            int other_length = 0;
            if (value_is_list(args[i]))
            {
                Node list = args[i].as.node;
                while (!mem_is_nil(list))
                {
                    other_length++;
                    list = mem_cdr(list);
                }
            }
            else
            {
                // Use word_nodes which has already converted numbers
                other_length = (int)mem_word_len(word_nodes[i - 1]);
            }
            
            if (other_length != length)
            {
                return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[i]));
            }
        }
    }
    
    // Set up iterators for each data list
    Node cursors[MAX_PROC_PARAMS];
    const char *word_ptrs[MAX_PROC_PARAMS];
    bool is_word[MAX_PROC_PARAMS];
    
    for (int i = 0; i < data_count; i++)
    {
        if (value_is_list(args[i + 1]))
        {
            cursors[i] = args[i + 1].as.node;
            is_word[i] = false;
        }
        else
        {
            // Use word_nodes which has already converted numbers
            word_ptrs[i] = mem_word_ptr(word_nodes[i]);
            is_word[i] = true;
        }
    }
    
    // Build result list using sentence semantics
    Node result_head = NODE_NIL;
    Node result_tail = NODE_NIL;
    
    Value proc_args[MAX_PROC_PARAMS];
    
    for (int idx = 0; idx < length; idx++)
    {
        // Collect current element from each data source
        for (int i = 0; i < data_count; i++)
        {
            if (is_word[i])
            {
                Node char_atom = mem_atom(&word_ptrs[i][idx], 1);
                proc_args[i] = value_word(char_atom);
            }
            else
            {
                Node elem = mem_car(cursors[i]);
                if (mem_is_word(elem))
                {
                    proc_args[i] = value_word(elem);
                }
                else if (mem_is_nil(elem))
                {
                    proc_args[i] = value_list(NODE_NIL);
                }
                else
                {
                    proc_args[i] = value_list(elem);
                }
                cursors[i] = mem_cdr(cursors[i]);
            }
        }
        
        // Invoke the procedure
        r = invoke_proc_spec(eval, &spec, data_count, proc_args);
        
        // Propagate errors, stop, throw
        if (r.status == RESULT_ERROR || r.status == RESULT_THROW ||
            r.status == RESULT_STOP)
        {
            return r;
        }
        
        // Handle output from procedure text
        if (r.status == RESULT_OUTPUT)
        {
            r = result_ok(r.value);
        }
        
        // Collect result using sentence semantics
        if (r.status == RESULT_OK)
        {
            if (value_is_list(r.value))
            {
                // Append all elements of the list (sentence semantics)
                Node list = r.value.as.node;
                while (!mem_is_nil(list))
                {
                    Node new_cell = mem_cons(mem_car(list), NODE_NIL);
                    if (mem_is_nil(result_head))
                    {
                        result_head = new_cell;
                        result_tail = new_cell;
                    }
                    else
                    {
                        mem_set_cdr(result_tail, new_cell);
                        result_tail = new_cell;
                    }
                    list = mem_cdr(list);
                }
            }
            else if (value_is_word(r.value) || value_is_number(r.value))
            {
                // Add word or number as single element
                Node result_node;
                if (value_is_number(r.value))
                {
                    char buf[32];
                    format_number(buf, sizeof(buf), r.value.as.number);
                    result_node = mem_atom_cstr(buf);
                }
                else
                {
                    result_node = r.value.as.node;
                }
                
                Node new_cell = mem_cons(result_node, NODE_NIL);
                if (mem_is_nil(result_head))
                {
                    result_head = new_cell;
                    result_tail = new_cell;
                }
                else
                {
                    mem_set_cdr(result_tail, new_cell);
                    result_tail = new_cell;
                }
            }
            // None values and empty lists contribute nothing
        }
    }
    
    return result_ok(value_list(result_head));
}

//==========================================================================
// filter procedure data
//==========================================================================

static Result prim_filter(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    
    // First argument is the procedure
    ProcSpec spec;
    Result r = parse_proc_spec(args[0], &spec);
    if (r.status == RESULT_ERROR)
    {
        return r;
    }
    
    // Second argument is the data list, word, or number
    // Convert numbers to words (numbers are self-quoting words in Logo)
    Node word_node = NODE_NIL;
    if (value_is_number(args[1]))
    {
        word_node = number_to_word(args[1].as.number);
    }
    else if (value_is_word(args[1]))
    {
        word_node = args[1].as.node;
    }
    else if (!value_is_list(args[1]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }
    
    bool input_is_word = !value_is_list(args[1]);
    Node data = input_is_word ? NODE_NIL : args[1].as.node;
    const char *word_ptr = input_is_word ? mem_word_ptr(word_node) : NULL;
    int word_len = input_is_word ? (int)mem_word_len(word_node) : 0;
    int word_idx = 0;
    
    // Build result list
    Node result_head = NODE_NIL;
    Node result_tail = NODE_NIL;
    
    Value proc_args[1];
    
    while (input_is_word ? (word_idx < word_len) : !mem_is_nil(data))
    {
        Node elem;
        
        // Get current element
        if (input_is_word)
        {
            elem = mem_atom(&word_ptr[word_idx], 1);
            proc_args[0] = value_word(elem);
        }
        else
        {
            elem = mem_car(data);
            // Set up argument
            if (mem_is_word(elem))
            {
                proc_args[0] = value_word(elem);
            }
            else if (mem_is_nil(elem))
            {
                proc_args[0] = value_list(NODE_NIL);
            }
            else
            {
                proc_args[0] = value_list(elem);
            }
        }
        
        // Invoke the procedure
        r = invoke_proc_spec(eval, &spec, 1, proc_args);
        
        // Propagate errors, stop, throw
        if (r.status == RESULT_ERROR || r.status == RESULT_THROW ||
            r.status == RESULT_STOP)
        {
            return r;
        }
        
        // Handle output from procedure text
        if (r.status == RESULT_OUTPUT)
        {
            r = result_ok(r.value);
        }
        
        // Check if result is boolean true
        if (r.status == RESULT_OK && value_is_word(r.value))
        {
            const char *str = mem_word_ptr(r.value.as.node);
            if (strcasecmp(str, "true") == 0)
            {
                // Include this element
                Node new_cell = mem_cons(elem, NODE_NIL);
                if (mem_is_nil(result_head))
                {
                    result_head = new_cell;
                    result_tail = new_cell;
                }
                else
                {
                    mem_set_cdr(result_tail, new_cell);
                    result_tail = new_cell;
                }
            }
            else if (strcasecmp(str, "false") != 0)
            {
                // Not a boolean
                return result_error_arg(ERR_NOT_BOOL, NULL, str);
            }
        }
        else if (r.status == RESULT_OK)
        {
            return result_error_arg(ERR_NOT_BOOL, NULL, value_to_string(r.value));
        }
        
        // Advance to next element
        if (input_is_word)
        {
            word_idx++;
        }
        else
        {
            data = mem_cdr(data);
        }
    }
    
    return result_ok(value_list(result_head));
}

//==========================================================================
// find procedure data
//==========================================================================

static Result prim_find(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    
    // First argument is the procedure
    ProcSpec spec;
    Result r = parse_proc_spec(args[0], &spec);
    if (r.status == RESULT_ERROR)
    {
        return r;
    }
    
    // Second argument is the data list, word, or number
    // Convert numbers to words (numbers are self-quoting words in Logo)
    Node word_node = NODE_NIL;
    if (value_is_number(args[1]))
    {
        word_node = number_to_word(args[1].as.number);
    }
    else if (value_is_word(args[1]))
    {
        word_node = args[1].as.node;
    }
    else if (!value_is_list(args[1]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }
    
    bool input_is_word = !value_is_list(args[1]);
    Node data = input_is_word ? NODE_NIL : args[1].as.node;
    const char *word_ptr = input_is_word ? mem_word_ptr(word_node) : NULL;
    int word_len = input_is_word ? (int)mem_word_len(word_node) : 0;
    int word_idx = 0;
    
    Value proc_args[1];
    
    while (input_is_word ? (word_idx < word_len) : !mem_is_nil(data))
    {
        // Get current element
        if (input_is_word)
        {
            Node char_atom = mem_atom(&word_ptr[word_idx], 1);
            proc_args[0] = value_word(char_atom);
        }
        else
        {
            Node elem = mem_car(data);
            // Set up argument
            if (mem_is_word(elem))
            {
                proc_args[0] = value_word(elem);
            }
            else if (mem_is_nil(elem))
            {
                proc_args[0] = value_list(NODE_NIL);
            }
            else
            {
                proc_args[0] = value_list(elem);
            }
        }
        
        // Invoke the procedure
        r = invoke_proc_spec(eval, &spec, 1, proc_args);
        
        // Propagate errors, stop, throw
        if (r.status == RESULT_ERROR || r.status == RESULT_THROW ||
            r.status == RESULT_STOP)
        {
            return r;
        }
        
        // Handle output from procedure text
        if (r.status == RESULT_OUTPUT)
        {
            r = result_ok(r.value);
        }
        
        // Check if result is boolean true
        if (r.status == RESULT_OK && value_is_word(r.value))
        {
            const char *str = mem_word_ptr(r.value.as.node);
            if (strcasecmp(str, "true") == 0)
            {
                // Found it - return this element
                return result_ok(proc_args[0]);
            }
            else if (strcasecmp(str, "false") != 0)
            {
                // Not a boolean
                return result_error_arg(ERR_NOT_BOOL, NULL, str);
            }
        }
        else if (r.status == RESULT_OK)
        {
            return result_error_arg(ERR_NOT_BOOL, NULL, value_to_string(r.value));
        }
        
        // Advance to next element
        if (input_is_word)
        {
            word_idx++;
        }
        else
        {
            data = mem_cdr(data);
        }
    }
    
    // Not found - return empty list
    return result_ok(value_list(NODE_NIL));
}

//==========================================================================
// reduce procedure data
//==========================================================================

static Result prim_reduce(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    
    // First argument is the procedure
    ProcSpec spec;
    Result r = parse_proc_spec(args[0], &spec);
    if (r.status == RESULT_ERROR)
    {
        return r;
    }
    
    // Second argument is the data list, word, or number
    // Convert numbers to words (numbers are self-quoting words in Logo)
    Node word_node = NODE_NIL;
    if (value_is_number(args[1]))
    {
        word_node = number_to_word(args[1].as.number);
    }
    else if (value_is_word(args[1]))
    {
        word_node = args[1].as.node;
    }
    else if (!value_is_list(args[1]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }
    
    bool input_is_word = !value_is_list(args[1]);
    
    // Count elements and collect them
    int count = 0;
    
    if (input_is_word)
    {
        count = (int)mem_word_len(word_node);
    }
    else
    {
        Node data = args[1].as.node;
        Node temp = data;
        while (!mem_is_nil(temp))
        {
            count++;
            temp = mem_cdr(temp);
        }
    }
    
    if (count == 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, input_is_word ? "\"" : "[]");
    }
    
    if (count == 1)
    {
        // Single element - return it
        if (input_is_word)
        {
            const char *word_ptr = mem_word_ptr(word_node);
            Node char_atom = mem_atom(&word_ptr[0], 1);
            return result_ok(value_word(char_atom));
        }
        else
        {
            Node data = args[1].as.node;
            Node elem = mem_car(data);
            if (mem_is_word(elem))
            {
                return result_ok(value_word(elem));
            }
            else
            {
                return result_ok(value_list(elem));
            }
        }
    }
    
    // Collect elements into array (we need to process from the end)
    Value elements[256];  // Reasonable max
    if (count > 256)
    {
        return result_error(ERR_OUT_OF_SPACE);
    }
    
    if (input_is_word)
    {
        const char *word_ptr = mem_word_ptr(word_node);
        for (int i = 0; i < count; i++)
        {
            Node char_atom = mem_atom(&word_ptr[i], 1);
            elements[i] = value_word(char_atom);
        }
    }
    else
    {
        Node data = args[1].as.node;
        Node temp = data;
        for (int i = 0; i < count; i++)
        {
            Node elem = mem_car(temp);
            if (mem_is_word(elem))
            {
                elements[i] = value_word(elem);
            }
            else if (mem_is_nil(elem))
            {
                elements[i] = value_list(NODE_NIL);
            }
            else
            {
                elements[i] = value_list(elem);
            }
            temp = mem_cdr(temp);
        }
    }
    
    // Start from the last two elements and work backwards
    Value proc_args[2];
    proc_args[0] = elements[count - 2];
    proc_args[1] = elements[count - 1];
    
    r = invoke_proc_spec(eval, &spec, 2, proc_args);
    
    if (r.status == RESULT_ERROR || r.status == RESULT_THROW ||
        r.status == RESULT_STOP)
    {
        return r;
    }
    
    // Handle output from procedure text
    if (r.status == RESULT_OUTPUT)
    {
        r = result_ok(r.value);
    }
    
    if (r.status != RESULT_OK)
    {
        return result_error_arg(ERR_DIDNT_OUTPUT, NULL, NULL);
    }
    
    Value accumulator = r.value;
    
    // Continue with remaining elements (from count-3 down to 0)
    // Per spec: "the output of procedure is then combined with the previous member"
    // So we call (previous_element, accumulated_result)
    for (int i = count - 3; i >= 0; i--)
    {
        proc_args[0] = elements[i];       // previous element
        proc_args[1] = accumulator;       // accumulated result
        
        r = invoke_proc_spec(eval, &spec, 2, proc_args);
        
        if (r.status == RESULT_ERROR || r.status == RESULT_THROW ||
            r.status == RESULT_STOP)
        {
            return r;
        }
        
        if (r.status == RESULT_OUTPUT)
        {
            r = result_ok(r.value);
        }
        
        if (r.status != RESULT_OK)
        {
            return result_error_arg(ERR_DIDNT_OUTPUT, NULL, NULL);
        }
        
        accumulator = r.value;
    }
    
    return result_ok(accumulator);
}

//==========================================================================
// crossmap procedure listlist
// (crossmap procedure data1 data2 ...)
//==========================================================================

static Result prim_crossmap(Evaluator *eval, int argc, Value *args)
{
    if (argc < 2)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, NULL, NULL);
    }
    
    // First argument is the procedure
    ProcSpec spec;
    Result r = parse_proc_spec(args[0], &spec);
    if (r.status == RESULT_ERROR)
    {
        return r;
    }
    
    // Determine data sources
    // If argc == 2, second arg is a list of lists (or words)
    // If argc > 2, args 1..argc-1 are the data lists/words
    
    int data_count = 0;
    int lengths[MAX_PROC_PARAMS];
    bool is_word[MAX_PROC_PARAMS];
    Node data_lists[MAX_PROC_PARAMS];
    const char *word_ptrs[MAX_PROC_PARAMS];
    
    if (argc == 2)
    {
        // Single listlist argument - must be a list
        // Elements can be lists or words
        REQUIRE_LIST(args[1]);
        Node listlist = args[1].as.node;
        
        while (!mem_is_nil(listlist) && data_count < MAX_PROC_PARAMS)
        {
            Node elem = mem_car(listlist);
            // Accept empty list (NODE_NIL), proper list nodes, or words
            if (mem_is_word(elem))
            {
                is_word[data_count] = true;
                word_ptrs[data_count] = mem_word_ptr(elem);
                data_lists[data_count] = NODE_NIL;
                lengths[data_count] = (int)mem_word_len(elem);
            }
            else if (mem_is_nil(elem) || mem_is_list(elem))
            {
                is_word[data_count] = false;
                word_ptrs[data_count] = NULL;
                data_lists[data_count] = elem;
                
                // Count length
                int len = 0;
                Node temp = elem;
                while (!mem_is_nil(temp))
                {
                    len++;
                    temp = mem_cdr(temp);
                }
                lengths[data_count] = len;
            }
            else
            {
                return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
            }
            data_count++;
            
            listlist = mem_cdr(listlist);
        }
    }
    else
    {
        // Multiple data list/word arguments
        for (int i = 1; i < argc && data_count < MAX_PROC_PARAMS; i++)
        {
            if (value_is_word(args[i]))
            {
                is_word[data_count] = true;
                word_ptrs[data_count] = mem_word_ptr(args[i].as.node);
                data_lists[data_count] = NODE_NIL;
                lengths[data_count] = (int)mem_word_len(args[i].as.node);
            }
            else if (value_is_list(args[i]))
            {
                is_word[data_count] = false;
                word_ptrs[data_count] = NULL;
                data_lists[data_count] = args[i].as.node;
                
                // Count length
                int len = 0;
                Node temp = args[i].as.node;
                while (!mem_is_nil(temp))
                {
                    len++;
                    temp = mem_cdr(temp);
                }
                lengths[data_count] = len;
            }
            else
            {
                return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[i]));
            }
            data_count++;
        }
    }
    
    if (data_count == 0)
    {
        return result_ok(value_list(NODE_NIL));
    }
    
    // Check if any list is empty - if so, result is empty list
    for (int i = 0; i < data_count; i++)
    {
        if (lengths[i] == 0)
        {
            return result_ok(value_list(NODE_NIL));
        }
    }
    
    // Build result list by iterating through all combinations
    // Use nested iteration with indices
    Node result_head = NODE_NIL;
    Node result_tail = NODE_NIL;
    
    // Track current index for each data list
    int indices[MAX_PROC_PARAMS];
    for (int i = 0; i < data_count; i++)
    {
        indices[i] = 0;
    }
    
    // Cache all elements for efficient access
    Value *all_elements[MAX_PROC_PARAMS];
    for (int i = 0; i < data_count; i++)
    {
        all_elements[i] = NULL;
    }
    
    // Calculate total storage needed and allocate dynamically
    // This avoids the 32KB static allocation and fixes reentrancy for nested crossmap calls
    int total_elements = 0;
    for (int i = 0; i < data_count; i++)
    {
        total_elements += lengths[i];
    }
    
    Value *element_storage = (Value *)malloc(total_elements * sizeof(Value));
    if (!element_storage)
    {
        return result_error(ERR_OUT_OF_SPACE);
    }
    
    // Set up pointers into the contiguous storage
    Value *storage_ptr = element_storage;
    for (int i = 0; i < data_count; i++)
    {
        all_elements[i] = storage_ptr;
        storage_ptr += lengths[i];
    }
    
    for (int i = 0; i < data_count; i++)
    {
        
        if (is_word[i])
        {
            // Word - each character is an element
            for (int j = 0; j < lengths[i]; j++)
            {
                Node char_atom = mem_atom(&word_ptrs[i][j], 1);
                all_elements[i][j] = value_word(char_atom);
            }
        }
        else
        {
            // List
            Node temp = data_lists[i];
            for (int j = 0; j < lengths[i]; j++)
            {
                Node elem = mem_car(temp);
                if (mem_is_word(elem))
                {
                    all_elements[i][j] = value_word(elem);
                }
                else if (mem_is_nil(elem))
                {
                    all_elements[i][j] = value_list(NODE_NIL);
                }
                else
                {
                    all_elements[i][j] = value_list(elem);
                }
                temp = mem_cdr(temp);
            }
        }
    }
    
    // Iterate through all combinations
    Value proc_args[MAX_PROC_PARAMS];
    
    while (true)
    {
        // Collect current elements
        for (int i = 0; i < data_count; i++)
        {
            proc_args[i] = all_elements[i][indices[i]];
        }
        
        // Invoke the procedure
        r = invoke_proc_spec(eval, &spec, data_count, proc_args);
        
        // Propagate errors, stop, throw
        if (r.status == RESULT_ERROR || r.status == RESULT_THROW ||
            r.status == RESULT_STOP)
        {
            free(element_storage);
            return r;
        }
        
        // Handle output from procedure text
        if (r.status == RESULT_OUTPUT)
        {
            r = result_ok(r.value);
        }
        
        // Collect result if we got one
        if (r.status == RESULT_OK)
        {
            Node result_node;
            if (value_is_word(r.value) || value_is_number(r.value))
            {
                if (value_is_number(r.value))
                {
                    char buf[32];
                    format_number(buf, sizeof(buf), r.value.as.number);
                    result_node = mem_atom_cstr(buf);
                }
                else
                {
                    result_node = r.value.as.node;
                }
            }
            else if (value_is_list(r.value))
            {
                result_node = r.value.as.node;
            }
            else
            {
                goto next_combination;  // Skip none values
            }
            
            // Append to result list
            Node new_cell = mem_cons(result_node, NODE_NIL);
            if (mem_is_nil(result_head))
            {
                result_head = new_cell;
                result_tail = new_cell;
            }
            else
            {
                mem_set_cdr(result_tail, new_cell);
                result_tail = new_cell;
            }
        }
        
    next_combination:;
        // Advance to next combination (rightmost index first)
        int pos = data_count - 1;
        while (pos >= 0)
        {
            indices[pos]++;
            if (indices[pos] < lengths[pos])
            {
                break;
            }
            indices[pos] = 0;
            pos--;
        }
        
        if (pos < 0)
        {
            // All combinations exhausted
            break;
        }
    }
    
    free(element_storage);
    return result_ok(value_list(result_head));
}

//==========================================================================
// Registration
//==========================================================================

void primitives_list_processing_init(void)
{
    primitive_register("apply", 2, prim_apply);
    primitive_register("foreach", 2, prim_foreach);
    primitive_register("map", 2, prim_map);
    primitive_register("map.se", 2, prim_map_se);
    primitive_register("filter", 2, prim_filter);
    primitive_register("find", 2, prim_find);
    primitive_register("reduce", 2, prim_reduce);
    primitive_register("crossmap", 2, prim_crossmap);
}
