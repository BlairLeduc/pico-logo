//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Control flow primitives: run, repeat, repcount, stop, output, while, do.while, for
//

#include "primitives.h"
#include "eval.h"
#include "value.h"
#include "variables.h"
#include <strings.h>  // for strcasecmp

static FrameHeader *control_flow_current_frame(Evaluator *eval)
{
    FrameStack *frames = eval_get_frames(eval);
    if (!frames || frame_stack_is_empty(frames))
    {
        return NULL;
    }
    return frame_current(frames);
}

static Result continue_run(Evaluator *eval, FrameHeader *frame)
{
    Node cursor = frame->prim_cursor;
    if (frame->prim_flags & PRIM_FLAG_CURSOR_AT_END)
    {
        frame->prim_flags &= (uint8_t)~PRIM_FLAG_CURSOR_AT_END;
        frame->prim_cont = PRIM_CONT_NONE;
        frame->prim_cursor = NODE_NIL;
        return result_none();
    }
    Node body = frame->prim_body;
    Node next_cursor = NODE_NIL;
    Result r = eval_run_list_expr_cps(eval, mem_is_nil(cursor) ? body : cursor, &next_cursor);
    if (r.status == RESULT_CALL)
    {
        frame->prim_cont = PRIM_CONT_RUN;
        frame->prim_cursor = next_cursor;
        if (mem_is_nil(next_cursor))
        {
            frame->prim_flags |= PRIM_FLAG_CURSOR_AT_END;
        }
        return r;
    }
    frame->prim_cont = PRIM_CONT_NONE;
    frame->prim_cursor = NODE_NIL;
    return r;
}

static Result continue_repeat(Evaluator *eval, FrameHeader *frame)
{
    int count = frame->prim_count;
    int i = frame->prim_i;
    int saved_repcount = frame->prim_saved_repcount;
    Node body = frame->prim_body;
    Node cursor = frame->prim_cursor;

    while (i < count)
    {
        if (frame->prim_flags & PRIM_FLAG_CURSOR_AT_END)
        {
            frame->prim_flags &= (uint8_t)~PRIM_FLAG_CURSOR_AT_END;
            cursor = NODE_NIL;
            i++;
            continue;
        }
        eval->repcount = i + 1;
        Node next_cursor = NODE_NIL;
        Result r = eval_run_list_cps(eval, mem_is_nil(cursor) ? body : cursor, false, &next_cursor);
        if (r.status == RESULT_CALL)
        {
            frame->prim_cont = PRIM_CONT_REPEAT;
            frame->prim_i = i;
            frame->prim_count = count;
            frame->prim_body = body;
            frame->prim_saved_repcount = saved_repcount;
            frame->prim_cursor = next_cursor;
            if (mem_is_nil(next_cursor))
            {
                frame->prim_flags |= PRIM_FLAG_CURSOR_AT_END;
            }
            return r;
        }
        if (r.status != RESULT_NONE && r.status != RESULT_OK)
        {
            eval->repcount = saved_repcount;
            frame->prim_cont = PRIM_CONT_NONE;
            frame->prim_cursor = NODE_NIL;
            return r;
        }
        cursor = NODE_NIL;
        i++;
    }

    eval->repcount = saved_repcount;
    frame->prim_cont = PRIM_CONT_NONE;
    frame->prim_cursor = NODE_NIL;
    return result_none();
}

static Result continue_forever(Evaluator *eval, FrameHeader *frame)
{
    int i = frame->prim_i;
    int saved_repcount = frame->prim_saved_repcount;
    Node body = frame->prim_body;
    Node cursor = frame->prim_cursor;

    while (true)
    {
        if (frame->prim_flags & PRIM_FLAG_CURSOR_AT_END)
        {
            frame->prim_flags &= (uint8_t)~PRIM_FLAG_CURSOR_AT_END;
            cursor = NODE_NIL;
            i++;
            continue;
        }
        eval->repcount = i + 1;
        Node next_cursor = NODE_NIL;
        Result r = eval_run_list_cps(eval, mem_is_nil(cursor) ? body : cursor, false, &next_cursor);
        if (r.status == RESULT_CALL)
        {
            frame->prim_cont = PRIM_CONT_FOREVER;
            frame->prim_i = i;
            frame->prim_body = body;
            frame->prim_saved_repcount = saved_repcount;
            frame->prim_cursor = next_cursor;
            if (mem_is_nil(next_cursor))
            {
                frame->prim_flags |= PRIM_FLAG_CURSOR_AT_END;
            }
            return r;
        }
        if (r.status != RESULT_NONE && r.status != RESULT_OK)
        {
            eval->repcount = saved_repcount;
            frame->prim_cont = PRIM_CONT_NONE;
            frame->prim_cursor = NODE_NIL;
            return r;
        }
        cursor = NODE_NIL;
        i++;
    }
}

static Result continue_predicate_loop(Evaluator *eval, FrameHeader *frame,
                                      Node predicate_list, Node body,
                                      bool predicate_first, bool stop_when_true,
                                      PrimContinuationKind kind)
{
    PrimContinuationPhase phase = (PrimContinuationPhase)frame->prim_phase;
    if (phase == PRIM_PHASE_INIT)
    {
        phase = predicate_first ? PRIM_PHASE_PRED : PRIM_PHASE_BODY;
    }
    Node cursor = frame->prim_cursor;

    if ((frame->prim_flags & PRIM_FLAG_CURSOR_AT_END) && phase == PRIM_PHASE_BODY)
    {
        frame->prim_flags &= (uint8_t)~PRIM_FLAG_CURSOR_AT_END;
        phase = PRIM_PHASE_PRED;
        cursor = NODE_NIL;
    }

    while (true)
    {
        if (phase == PRIM_PHASE_PRED)
        {
            Node next_cursor = NODE_NIL;
            Result pred_result = eval_run_list_expr_cps(eval, mem_is_nil(cursor) ? predicate_list : cursor,
                                                        &next_cursor);
            if (pred_result.status == RESULT_CALL)
            {
                frame->prim_cont = kind;
                frame->prim_phase = PRIM_PHASE_PRED;
                frame->prim_body = body;
                frame->prim_pred = predicate_list;
                frame->prim_cursor = next_cursor;
                return pred_result;
            }
            if (pred_result.status == RESULT_ERROR)
            {
                frame->prim_cont = PRIM_CONT_NONE;
                frame->prim_cursor = NODE_NIL;
                return pred_result;
            }
            if (pred_result.status != RESULT_OK)
            {
                frame->prim_cont = PRIM_CONT_NONE;
                frame->prim_cursor = NODE_NIL;
                return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
            }

            REQUIRE_BOOL(pred_result.value, condition);
            if (stop_when_true ? condition : !condition)
            {
                frame->prim_cont = PRIM_CONT_NONE;
                frame->prim_cursor = NODE_NIL;
                return result_none();
            }

            phase = PRIM_PHASE_BODY;
            cursor = NODE_NIL;
            continue;
        }

        Node next_cursor = NODE_NIL;
        Result r = eval_run_list_cps(eval, mem_is_nil(cursor) ? body : cursor, false, &next_cursor);
        if (r.status == RESULT_CALL)
        {
            frame->prim_cont = kind;
            frame->prim_phase = PRIM_PHASE_BODY;
            frame->prim_body = body;
            frame->prim_pred = predicate_list;
            frame->prim_cursor = next_cursor;
            if (mem_is_nil(next_cursor))
            {
                frame->prim_flags |= PRIM_FLAG_CURSOR_AT_END;
            }
            return r;
        }
        if (r.status != RESULT_NONE && r.status != RESULT_OK)
        {
            frame->prim_cont = PRIM_CONT_NONE;
            frame->prim_cursor = NODE_NIL;
            return r;
        }

        phase = PRIM_PHASE_PRED;
        cursor = NODE_NIL;
    }
}

static Result continue_for(Evaluator *eval, FrameHeader *frame)
{
    const char *varname = frame->prim_varname;
    float current = frame->prim_current;
    float limit = frame->prim_limit;
    float step = frame->prim_step;
    Node body = frame->prim_body;
    Node cursor = frame->prim_cursor;

    while (true)
    {
        if (frame->prim_flags & PRIM_FLAG_CURSOR_AT_END)
        {
            frame->prim_flags &= (uint8_t)~PRIM_FLAG_CURSOR_AT_END;
            cursor = NODE_NIL;
            current += step;
            continue;
        }
        float diff = current - limit;
        if (diff * step > 0)
        {
            break;
        }

        if (!var_set(varname, value_number(current)))
        {
            frame->prim_cont = PRIM_CONT_NONE;
            frame->prim_cursor = NODE_NIL;
            return result_error(ERR_OUT_OF_SPACE);
        }

        Node next_cursor = NODE_NIL;
        Result r = eval_run_list_cps(eval, mem_is_nil(cursor) ? body : cursor, false, &next_cursor);
        if (r.status == RESULT_CALL)
        {
            frame->prim_cont = PRIM_CONT_FOR;
            frame->prim_phase = PRIM_PHASE_BODY;
            frame->prim_body = body;
            frame->prim_cursor = next_cursor;
            frame->prim_current = current;
            frame->prim_limit = limit;
            frame->prim_step = step;
            if (mem_is_nil(next_cursor))
            {
                frame->prim_flags |= PRIM_FLAG_CURSOR_AT_END;
            }
            return r;
        }
        if (r.status != RESULT_NONE && r.status != RESULT_OK)
        {
            frame->prim_cont = PRIM_CONT_NONE;
            frame->prim_cursor = NODE_NIL;
            return r;
        }

        cursor = NODE_NIL;
        current += step;
    }

    if ((frame->prim_flags & PRIM_FLAG_FOR_IN_PROC) == 0)
    {
        if (frame->prim_flags & PRIM_FLAG_FOR_HAD_VALUE)
        {
            var_set(varname, frame->prim_saved_value);
        }
        else
        {
            var_erase(varname);
        }
    }

    frame->prim_cont = PRIM_CONT_NONE;
    frame->prim_cursor = NODE_NIL;
    return result_none();
}

// run list - runs the provided list as Logo code
static Result prim_run(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);

    FrameHeader *frame = control_flow_current_frame(eval);
    if (frame && frame->prim_cont == PRIM_CONT_RUN)
    {
        return continue_run(eval, frame);
    }

    if (frame && (frame->prim_cont != PRIM_CONT_NONE || eval->cps_list_depth > 0))
    {
        return eval_run_list_expr(eval, args[0].as.node);
    }

    // Top-level or no frame: fall back to inline evaluation
    if (frame == NULL)
    {
        return eval_run_list_expr(eval, args[0].as.node);
    }

    Node cursor = NODE_NIL;
    Result r = eval_run_list_expr_cps(eval, args[0].as.node, &cursor);
    if (r.status == RESULT_CALL)
    {
        frame->prim_cont = PRIM_CONT_RUN;
        frame->prim_flags &= (uint8_t)~PRIM_FLAG_CURSOR_AT_END;
        frame->prim_body = args[0].as.node;
        frame->prim_cursor = cursor;
        return r;
    }
    return r;
}

// forever - repeats the provided list indefinitely
static Result prim_forever(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);

    FrameHeader *frame = control_flow_current_frame(eval);
    if (frame && frame->prim_cont == PRIM_CONT_FOREVER)
    {
        return continue_forever(eval, frame);
    }

    if (frame && (frame->prim_cont != PRIM_CONT_NONE || eval->cps_list_depth > 0))
    {
        Node body = args[0].as.node;
        int previous_repcount = eval->repcount;
        int iteration = 0;
        while (true)
        {
            eval->repcount = iteration + 1;
            Result r = eval_run_list(eval, body);
            iteration++;
            if (r.status != RESULT_NONE && r.status != RESULT_OK)
            {
                eval->repcount = previous_repcount;
                return r;
            }
        }
    }

    Node body = args[0].as.node;
    int previous_repcount = eval->repcount;

    if (frame == NULL)
    {
        int iteration = 0;
        while (true)
        {
            eval->repcount = iteration + 1;
            Result r = eval_run_list(eval, body);
            iteration++;
            if (r.status != RESULT_NONE && r.status != RESULT_OK)
            {
                eval->repcount = previous_repcount;
                return r;
            }
        }
    }

    frame->prim_cont = PRIM_CONT_FOREVER;
    frame->prim_flags &= (uint8_t)~PRIM_FLAG_CURSOR_AT_END;
    frame->prim_i = 0;
    frame->prim_body = body;
    frame->prim_saved_repcount = previous_repcount;
    frame->prim_cursor = NODE_NIL;
    return continue_forever(eval, frame);
}

// repeat count list - repeats the provided list count times
static Result prim_repeat(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_NUMBER(args[0], count_f);
    REQUIRE_LIST(args[1]);

    FrameHeader *frame = control_flow_current_frame(eval);
    if (frame && frame->prim_cont == PRIM_CONT_REPEAT)
    {
        return continue_repeat(eval, frame);
    }

    if (frame && (frame->prim_cont != PRIM_CONT_NONE || eval->cps_list_depth > 0))
    {
        int count = (int)count_f;
        Node body = args[1].as.node;
        int previous_repcount = eval->repcount;
        for (int i = 0; i < count; i++)
        {
            eval->repcount = i + 1;
            Result r = eval_run_list(eval, body);
            if (r.status != RESULT_NONE && r.status != RESULT_OK)
            {
                eval->repcount = previous_repcount;
                return r;
            }
        }
        eval->repcount = previous_repcount;
        return result_none();
    }

    int count = (int)count_f;
    Node body = args[1].as.node;
    int previous_repcount = eval->repcount;

    if (frame == NULL)
    {
        for (int i = 0; i < count; i++)
        {
            eval->repcount = i + 1;
            Result r = eval_run_list(eval, body);
            if (r.status != RESULT_NONE && r.status != RESULT_OK)
            {
                eval->repcount = previous_repcount;
                return r;
            }
        }
        eval->repcount = previous_repcount;
        return result_none();
    }

    frame->prim_cont = PRIM_CONT_REPEAT;
    frame->prim_flags &= (uint8_t)~PRIM_FLAG_CURSOR_AT_END;
    frame->prim_i = 0;
    frame->prim_count = count;
    frame->prim_body = body;
    frame->prim_saved_repcount = previous_repcount;
    frame->prim_cursor = NODE_NIL;
    return continue_repeat(eval, frame);
}

static Result prim_repcount(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc); UNUSED(args);
    
    // Return the innermost repeat count (1-based)
    return result_ok(value_number(eval->repcount));
}

static Result prim_stop(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    return result_stop();
}

static Result prim_output(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    return result_output(args[0]);
}

// ignore - does nothing
static Result prim_ignore(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    return result_none();
}

// ; (comment) - ignores its input (a word or list)
static Result prim_comment(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    return result_none();
}

// do.while list predicate_list - runs list repeatedly as long as predicate_list evaluates to true
// list is always run at least once
static Result prim_do_while(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);
    REQUIRE_LIST(args[1]);

    Node body = args[0].as.node;
    Node predicate_list = args[1].as.node;

    FrameHeader *frame = control_flow_current_frame(eval);
    if (frame && frame->prim_cont == PRIM_CONT_DO_WHILE)
    {
        return continue_predicate_loop(eval, frame, predicate_list, body, false, false, PRIM_CONT_DO_WHILE);
    }

    if (frame && (frame->prim_cont != PRIM_CONT_NONE || eval->cps_list_depth > 0))
    {
        do
        {
            Result r = eval_run_list(eval, body);
            if (r.status != RESULT_NONE && r.status != RESULT_OK)
            {
                return r;
            }

            Result pred_result = eval_run_list_expr(eval, predicate_list);
            if (pred_result.status == RESULT_ERROR)
            {
                return pred_result;
            }
            if (pred_result.status != RESULT_OK)
            {
                return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
            }

            REQUIRE_BOOL(pred_result.value, condition);
            if (!condition)
            {
                break;
            }
        } while (true);

        return result_none();
    }

    if (frame == NULL)
    {
        do
        {
            Result r = eval_run_list(eval, body);
            if (r.status != RESULT_NONE && r.status != RESULT_OK)
            {
                return r;
            }

            Result pred_result = eval_run_list_expr(eval, predicate_list);
            if (pred_result.status == RESULT_ERROR)
            {
                return pred_result;
            }
            if (pred_result.status != RESULT_OK)
            {
                return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
            }

            REQUIRE_BOOL(pred_result.value, condition);
            if (!condition)
            {
                break;
            }
        } while (true);

        return result_none();
    }

    frame->prim_cont = PRIM_CONT_DO_WHILE;
    frame->prim_flags &= (uint8_t)~PRIM_FLAG_CURSOR_AT_END;
    frame->prim_phase = PRIM_PHASE_INIT;
    frame->prim_body = body;
    frame->prim_pred = predicate_list;
    frame->prim_cursor = NODE_NIL;
    return continue_predicate_loop(eval, frame, predicate_list, body, false, false, PRIM_CONT_DO_WHILE);
}

// while predicate_list list - tests predicate_list and runs list if true, repeats until false
// list may not be run at all if predicate_list is initially false
static Result prim_while(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);
    REQUIRE_LIST(args[1]);

    Node predicate_list = args[0].as.node;
    Node body = args[1].as.node;

    FrameHeader *frame = control_flow_current_frame(eval);
    if (frame && frame->prim_cont == PRIM_CONT_WHILE)
    {
        return continue_predicate_loop(eval, frame, predicate_list, body, true, false, PRIM_CONT_WHILE);
    }

    if (frame && (frame->prim_cont != PRIM_CONT_NONE || eval->cps_list_depth > 0))
    {
        while (true)
        {
            Result pred_result = eval_run_list_expr(eval, predicate_list);
            if (pred_result.status == RESULT_ERROR)
            {
                return pred_result;
            }
            if (pred_result.status != RESULT_OK)
            {
                return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
            }

            REQUIRE_BOOL(pred_result.value, condition);
            if (!condition)
            {
                break;
            }

            Result r = eval_run_list(eval, body);
            if (r.status != RESULT_NONE && r.status != RESULT_OK)
            {
                return r;
            }
        }

        return result_none();
    }

    if (frame == NULL)
    {
        while (true)
        {
            Result pred_result = eval_run_list_expr(eval, predicate_list);
            if (pred_result.status == RESULT_ERROR)
            {
                return pred_result;
            }
            if (pred_result.status != RESULT_OK)
            {
                return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
            }

            REQUIRE_BOOL(pred_result.value, condition);
            if (!condition)
            {
                break;
            }

            Result r = eval_run_list(eval, body);
            if (r.status != RESULT_NONE && r.status != RESULT_OK)
            {
                return r;
            }
        }

        return result_none();
    }

    frame->prim_cont = PRIM_CONT_WHILE;
    frame->prim_flags &= (uint8_t)~PRIM_FLAG_CURSOR_AT_END;
    frame->prim_phase = PRIM_PHASE_INIT;
    frame->prim_body = body;
    frame->prim_pred = predicate_list;
    frame->prim_cursor = NODE_NIL;
    return continue_predicate_loop(eval, frame, predicate_list, body, true, false, PRIM_CONT_WHILE);
}

// do.until list predicate_list - runs list repeatedly until predicate_list evaluates to true
// list is always run at least once
static Result prim_do_until(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);
    REQUIRE_LIST(args[1]);

    Node body = args[0].as.node;
    Node predicate_list = args[1].as.node;

    FrameHeader *frame = control_flow_current_frame(eval);
    if (frame && frame->prim_cont == PRIM_CONT_DO_UNTIL)
    {
        return continue_predicate_loop(eval, frame, predicate_list, body, false, true, PRIM_CONT_DO_UNTIL);
    }

    if (frame && (frame->prim_cont != PRIM_CONT_NONE || eval->cps_list_depth > 0))
    {
        do
        {
            Result r = eval_run_list(eval, body);
            if (r.status != RESULT_NONE && r.status != RESULT_OK)
            {
                return r;
            }

            Result pred_result = eval_run_list_expr(eval, predicate_list);
            if (pred_result.status == RESULT_ERROR)
            {
                return pred_result;
            }
            if (pred_result.status != RESULT_OK)
            {
                return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
            }

            REQUIRE_BOOL(pred_result.value, condition);
            if (condition)
            {
                break;
            }
        } while (true);

        return result_none();
    }

    if (frame == NULL)
    {
        do
        {
            Result r = eval_run_list(eval, body);
            if (r.status != RESULT_NONE && r.status != RESULT_OK)
            {
                return r;
            }

            Result pred_result = eval_run_list_expr(eval, predicate_list);
            if (pred_result.status == RESULT_ERROR)
            {
                return pred_result;
            }
            if (pred_result.status != RESULT_OK)
            {
                return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
            }

            REQUIRE_BOOL(pred_result.value, condition);
            if (condition)
            {
                break;
            }
        } while (true);

        return result_none();
    }

    frame->prim_cont = PRIM_CONT_DO_UNTIL;
    frame->prim_flags &= (uint8_t)~PRIM_FLAG_CURSOR_AT_END;
    frame->prim_phase = PRIM_PHASE_INIT;
    frame->prim_body = body;
    frame->prim_pred = predicate_list;
    frame->prim_cursor = NODE_NIL;
    return continue_predicate_loop(eval, frame, predicate_list, body, false, true, PRIM_CONT_DO_UNTIL);
}

// until predicate_list list - tests predicate_list and runs list if false, repeats until true
// list may not be run at all if predicate_list is initially true
static Result prim_until(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);
    REQUIRE_LIST(args[1]);

    Node predicate_list = args[0].as.node;
    Node body = args[1].as.node;

    FrameHeader *frame = control_flow_current_frame(eval);
    if (frame && frame->prim_cont == PRIM_CONT_UNTIL)
    {
        return continue_predicate_loop(eval, frame, predicate_list, body, true, true, PRIM_CONT_UNTIL);
    }

    if (frame && (frame->prim_cont != PRIM_CONT_NONE || eval->cps_list_depth > 0))
    {
        while (true)
        {
            Result pred_result = eval_run_list_expr(eval, predicate_list);
            if (pred_result.status == RESULT_ERROR)
            {
                return pred_result;
            }
            if (pred_result.status != RESULT_OK)
            {
                return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
            }

            REQUIRE_BOOL(pred_result.value, condition);
            if (condition)
            {
                break;
            }

            Result r = eval_run_list(eval, body);
            if (r.status != RESULT_NONE && r.status != RESULT_OK)
            {
                return r;
            }
        }

        return result_none();
    }

    if (frame == NULL)
    {
        while (true)
        {
            Result pred_result = eval_run_list_expr(eval, predicate_list);
            if (pred_result.status == RESULT_ERROR)
            {
                return pred_result;
            }
            if (pred_result.status != RESULT_OK)
            {
                return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
            }

            REQUIRE_BOOL(pred_result.value, condition);
            if (condition)
            {
                break;
            }

            Result r = eval_run_list(eval, body);
            if (r.status != RESULT_NONE && r.status != RESULT_OK)
            {
                return r;
            }
        }

        return result_none();
    }

    frame->prim_cont = PRIM_CONT_UNTIL;
    frame->prim_flags &= (uint8_t)~PRIM_FLAG_CURSOR_AT_END;
    frame->prim_phase = PRIM_PHASE_INIT;
    frame->prim_body = body;
    frame->prim_pred = predicate_list;
    frame->prim_cursor = NODE_NIL;
    return continue_predicate_loop(eval, frame, predicate_list, body, true, true, PRIM_CONT_UNTIL);
}

// Helper to evaluate a word or list to get a number
// If the value is a word that looks like a number, use it directly
// If the value is a list, run it and expect a number output
static Result eval_to_number(Evaluator *eval, Value v, float *out)
{
    // First, try to interpret directly as a number
    if (value_to_number(v, out))
    {
        return result_none();
    }
    
    // If it's a list, run it to get a number
    if (value_is_list(v))
    {
        Result r = eval_run_list_expr(eval, v.as.node);
        if (r.status == RESULT_ERROR)
        {
            return r;
        }
        if (r.status != RESULT_OK)
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(v));
        }
        if (!value_to_number(r.value, out))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(r.value));
        }
        return result_none();
    }
    
    // Not a number and not a list
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(v));
}

// for forcontrol instructionlist
// forcontrol is [varname start limit] or [varname start limit step]
// Runs instructionlist repeatedly with varname set to start, start+step, ...
// Loop ends when (current - limit) has same sign as step
static Result prim_for(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);
    REQUIRE_LIST(args[1]);

    FrameHeader *frame = control_flow_current_frame(eval);
    if (frame && frame->prim_cont == PRIM_CONT_FOR)
    {
        return continue_for(eval, frame);
    }

    if (frame && (frame->prim_cont != PRIM_CONT_NONE || eval->cps_list_depth > 0))
    {
        Node forcontrol = args[0].as.node;
        Node body = args[1].as.node;

        int count = 0;
        Node temp = forcontrol;
        while (!mem_is_nil(temp))
        {
            count++;
            temp = mem_cdr(temp);
        }

        if (count < 3 || count > 4)
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS_LIST, NULL, value_to_string(args[0]));
        }

        Node elem1 = mem_car(forcontrol);
        Node rest1 = mem_cdr(forcontrol);
        Node elem2 = mem_car(rest1);
        Node rest2 = mem_cdr(rest1);
        Node elem3 = mem_car(rest2);
        Node rest3 = mem_cdr(rest2);

        if (!mem_is_word(elem1))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
        }
        const char *varname = mem_word_ptr(elem1);

        Value start_val = mem_is_word(elem2) ? value_word(elem2) : value_list(elem2);
        float start;
        Result r = eval_to_number(eval, start_val, &start);
        if (r.status == RESULT_ERROR)
        {
            return r;
        }

        Value limit_val = mem_is_word(elem3) ? value_word(elem3) : value_list(elem3);
        float limit;
        r = eval_to_number(eval, limit_val, &limit);
        if (r.status == RESULT_ERROR)
        {
            return r;
        }

        float step;
        bool has_explicit_step = !mem_is_nil(rest3);
        if (has_explicit_step)
        {
            Node elem4 = mem_car(rest3);
            Value step_val = mem_is_word(elem4) ? value_word(elem4) : value_list(elem4);
            r = eval_to_number(eval, step_val, &step);
            if (r.status == RESULT_ERROR)
            {
                return r;
            }
        }
        else
        {
            step = (limit >= start) ? 1.0f : -1.0f;
        }

        Value saved_value = value_none();
        bool had_value = var_get(varname, &saved_value);

        bool in_proc = eval_in_procedure(eval);
        if (in_proc)
        {
            if (!var_declare_local(varname))
            {
                return result_error(ERR_OUT_OF_SPACE);
            }
        }

        float current = start;
        Result loop_result = result_none();
        while (true)
        {
            float diff = current - limit;
            if (diff * step > 0)
            {
                break;
            }

            if (!var_set(varname, value_number(current)))
            {
                loop_result = result_error(ERR_OUT_OF_SPACE);
                break;
            }

            r = eval_run_list(eval, body);
            if (r.status != RESULT_NONE && r.status != RESULT_OK)
            {
                loop_result = r;
                break;
            }

            current += step;
        }

        if (!in_proc)
        {
            if (had_value)
            {
                var_set(varname, saved_value);
            }
            else
            {
                var_erase(varname);
            }
        }

        return loop_result;
    }

    Node forcontrol = args[0].as.node;
    Node body = args[1].as.node;

    int count = 0;
    Node temp = forcontrol;
    while (!mem_is_nil(temp))
    {
        count++;
        temp = mem_cdr(temp);
    }

    if (count < 3 || count > 4)
    {
        return result_error_arg(ERR_TOO_FEW_ITEMS_LIST, NULL, value_to_string(args[0]));
    }

    Node elem1 = mem_car(forcontrol);
    Node rest1 = mem_cdr(forcontrol);
    Node elem2 = mem_car(rest1);
    Node rest2 = mem_cdr(rest1);
    Node elem3 = mem_car(rest2);
    Node rest3 = mem_cdr(rest2);

    if (!mem_is_word(elem1))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    const char *varname = mem_word_ptr(elem1);

    Value start_val = mem_is_word(elem2) ? value_word(elem2) : value_list(elem2);
    float start;
    Result r = eval_to_number(eval, start_val, &start);
    if (r.status == RESULT_ERROR)
    {
        return r;
    }

    Value limit_val = mem_is_word(elem3) ? value_word(elem3) : value_list(elem3);
    float limit;
    r = eval_to_number(eval, limit_val, &limit);
    if (r.status == RESULT_ERROR)
    {
        return r;
    }

    float step;
    bool has_explicit_step = !mem_is_nil(rest3);
    if (has_explicit_step)
    {
        Node elem4 = mem_car(rest3);
        Value step_val = mem_is_word(elem4) ? value_word(elem4) : value_list(elem4);
        r = eval_to_number(eval, step_val, &step);
        if (r.status == RESULT_ERROR)
        {
            return r;
        }
    }
    else
    {
        step = (limit >= start) ? 1.0f : -1.0f;
    }

    Value saved_value = value_none();
    bool had_value = var_get(varname, &saved_value);

    bool in_proc = eval_in_procedure(eval);
    if (in_proc)
    {
        if (!var_declare_local(varname))
        {
            return result_error(ERR_OUT_OF_SPACE);
        }
    }

    if (frame == NULL)
    {
        float current = start;
        Result loop_result = result_none();
        while (true)
        {
            float diff = current - limit;
            if (diff * step > 0)
            {
                break;
            }

            if (!var_set(varname, value_number(current)))
            {
                loop_result = result_error(ERR_OUT_OF_SPACE);
                break;
            }

            r = eval_run_list(eval, body);
            if (r.status != RESULT_NONE && r.status != RESULT_OK)
            {
                loop_result = r;
                break;
            }

            current += step;
        }

        if (!in_proc)
        {
            if (had_value)
            {
                var_set(varname, saved_value);
            }
            else
            {
                var_erase(varname);
            }
        }

        return loop_result;
    }

    frame->prim_cont = PRIM_CONT_FOR;
    frame->prim_flags &= (uint8_t)~PRIM_FLAG_CURSOR_AT_END;
    frame->prim_phase = PRIM_PHASE_INIT;
    frame->prim_flags = in_proc ? PRIM_FLAG_FOR_IN_PROC : 0;
    if (had_value)
    {
        frame->prim_flags |= PRIM_FLAG_FOR_HAD_VALUE;
    }
    frame->prim_varname = varname;
    frame->prim_saved_value = saved_value;
    frame->prim_current = start;
    frame->prim_limit = limit;
    frame->prim_step = step;
    frame->prim_body = body;
    frame->prim_cursor = NODE_NIL;
    return continue_for(eval, frame);
}


void primitives_control_flow_init(void)
{
    primitive_register("run", 1, prim_run);
    primitive_register("forever", 1, prim_forever);
    primitive_register("repeat", 2, prim_repeat);
    primitive_register("repcount", 0, prim_repcount);
    primitive_register("stop", 0, prim_stop);
    primitive_register("output", 1, prim_output);
    primitive_register("op", 1, prim_output); // Abbreviation
    primitive_register("ignore", 1, prim_ignore);
    primitive_register(";", 1, prim_comment);
    primitive_register("do.while", 2, prim_do_while);
    primitive_register("while", 2, prim_while);
    primitive_register("do.until", 2, prim_do_until);
    primitive_register("until", 2, prim_until);
    primitive_register("for", 2, prim_for);
}

Result primitives_control_flow_continue(Evaluator *eval)
{
    FrameHeader *frame = control_flow_current_frame(eval);
    if (!frame)
    {
        return result_none();
    }

    switch ((PrimContinuationKind)frame->prim_cont)
    {
    case PRIM_CONT_RUN:
        return continue_run(eval, frame);
    case PRIM_CONT_REPEAT:
        return continue_repeat(eval, frame);
    case PRIM_CONT_FOREVER:
        return continue_forever(eval, frame);
    case PRIM_CONT_WHILE:
        return continue_predicate_loop(eval, frame, frame->prim_pred, frame->prim_body, true, false, PRIM_CONT_WHILE);
    case PRIM_CONT_DO_WHILE:
        return continue_predicate_loop(eval, frame, frame->prim_pred, frame->prim_body, false, false, PRIM_CONT_DO_WHILE);
    case PRIM_CONT_UNTIL:
        return continue_predicate_loop(eval, frame, frame->prim_pred, frame->prim_body, true, true, PRIM_CONT_UNTIL);
    case PRIM_CONT_DO_UNTIL:
        return continue_predicate_loop(eval, frame, frame->prim_pred, frame->prim_body, false, true, PRIM_CONT_DO_UNTIL);
    case PRIM_CONT_FOR:
        return continue_for(eval, frame);
    case PRIM_CONT_NONE:
    default:
        return result_none();
    }
}
