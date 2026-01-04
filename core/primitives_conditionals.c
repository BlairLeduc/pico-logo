//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Conditional primitives: if, true, false, test, iftrue, iffalse
//

#include "primitives.h"
#include "eval.h"
#include "memory.h"
#include "value.h"
#include "variables.h"
#include <strings.h>  // for strcasecmp

//==========================================================================
// IF Command/Operation
//==========================================================================

// if predicate list1 [list2]
// If predicate is true, run list1
// If predicate is false and list2 provided, run list2
static Result prim_if(Evaluator *eval, int argc, Value *args)
{
    REQUIRE_BOOL(args[0], condition);
    REQUIRE_LIST(args[1]);
    
    if (condition)
    {
        // Use eval_run_list_expr so if can act as an operation
        return eval_run_list_expr(eval, args[1].as.node);
    }
    else if (argc >= 3)
    {
        REQUIRE_LIST(args[2]);

        // Use eval_run_list_expr so if can act as an operation
        return eval_run_list_expr(eval, args[2].as.node);
    }
    
    return result_none();
}

//==========================================================================
// Boolean Operations
//==========================================================================

static Result prim_true(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    Node true_word = mem_atom_cstr("true");
    return result_ok(value_word(true_word));
}

static Result prim_false(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    Node false_word = mem_atom_cstr("false");
    return result_ok(value_word(false_word));
}

//==========================================================================
// Test/Conditional Flow
//==========================================================================

static Result prim_test(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_BOOL(args[0], condition);

    var_set_test(condition);
    
    return result_none();
}

static Result prim_iftrue(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);
    
    bool test_value;
    if (var_get_test(&test_value) && test_value)
    {
        return eval_run_list(eval, args[0].as.node);
    }
    
    return result_none();
}

static Result prim_iffalse(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);
    
    bool test_value;
    if (var_get_test(&test_value) && !test_value)
    {
        return eval_run_list(eval, args[0].as.node);
    }
    
    return result_none();
}

//==========================================================================
// CASE and COND
//==========================================================================

static Result prim_case(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    Value value = args[0];
    REQUIRE_LIST(args[1]);
    Node clauses = args[1].as.node;

    while (!mem_is_nil(clauses))
    {
        Node clause_node = mem_car(clauses);
        if (!mem_is_list(clause_node))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(value_word(clause_node)));
        }

        Node first_element = mem_car(clause_node);
        Node rest_of_clause = mem_cdr(clause_node);

        bool match = false;

        // Check for "else"
        if (mem_is_word(first_element))
        {
            Value v_first = value_word(first_element);
            const char *s_first = value_to_string(v_first);
            if (strcasecmp(s_first, "else") == 0)
            {
                match = true;
            }
        }
        
        if (!match && mem_is_list(first_element))
        {
            // Check if value is a member of the list
            Node candidates = first_element;
            while (!mem_is_nil(candidates))
            {
                Node candidate_node = mem_car(candidates);
                Value candidate_val;
                if (mem_is_list(candidate_node)) {
                    candidate_val = value_list(candidate_node);
                } else {
                    candidate_val = value_word(candidate_node);
                }
                
                if (values_equal(value, candidate_val))
                {
                    match = true;
                    break;
                }
                candidates = mem_cdr(candidates);
            }
        }

        if (match)
        {
            return eval_run_list_expr(eval, rest_of_clause);
        }

        clauses = mem_cdr(clauses);
    }

    return result_none();
}

static Result prim_cond(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);
    Node clauses = args[0].as.node;

    while (!mem_is_nil(clauses))
    {
        Node clause_node = mem_car(clauses);
        if (!mem_is_list(clause_node))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(value_word(clause_node)));
        }

        Node first_element = mem_car(clause_node);
        Node rest_of_clause = mem_cdr(clause_node);

        bool match = false;

        // Check for "else"
        if (mem_is_word(first_element))
        {
            Value v_first = value_word(first_element);
            const char *s_first = value_to_string(v_first);
            if (strcasecmp(s_first, "else") == 0)
            {
                match = true;
            }
        }

        if (!match)
        {
            Node condition_list;
            if (mem_is_list(first_element))
            {
                condition_list = first_element;
            }
            else
            {
                // Wrap word in a list
                condition_list = mem_cons(first_element, NODE_NIL);
            }
            
            Result r = eval_run_list_expr(eval, condition_list);
            if (r.status != RESULT_OK) return r;
            
            REQUIRE_BOOL(r.value, condition);
            if (condition)
            {
                match = true;
            }
        }

        if (match)
        {
            return eval_run_list_expr(eval, rest_of_clause);
        }

        clauses = mem_cdr(clauses);
    }

    return result_none();
}

void primitives_conditionals_init(void)
{
    primitive_register("if", 2, prim_if);
    
    // Boolean operations
    primitive_register("true", 0, prim_true);
    primitive_register("false", 0, prim_false);
    
    // Test/conditional flow
    primitive_register("test", 1, prim_test);
    primitive_register("iftrue", 1, prim_iftrue);
    primitive_register("ift", 1, prim_iftrue); // Abbreviation
    primitive_register("iffalse", 1, prim_iffalse);
    primitive_register("iff", 1, prim_iffalse); // Abbreviation

    // Case and Cond
    primitive_register("case", 2, prim_case);
    primitive_register("cond", 1, prim_cond);
}
