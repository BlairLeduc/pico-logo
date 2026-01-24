//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  VM evaluator scaffolding (Phase 0).
//

#include "vm.h"
#include "error.h"
#include "primitives.h"
#include "procedures.h"
#include "variables.h"
#include "eval.h"
#include "frame.h"

enum
{
    VM_DEFAULT_STACK_CAP = 64
};

void vm_init(VM *vm)
{
    if (!vm)
        return;
    vm->bc = NULL;
    vm->eval = NULL;
    vm->stack = NULL;
    vm->stack_cap = 0;
    vm->sp = 0;
}

static bool vm_push(VM *vm, Value v)
{
    if (!vm->stack || vm->sp >= vm->stack_cap)
        return false;
    vm->stack[vm->sp++] = v;
    return true;
}

static bool vm_pop(VM *vm, Value *out)
{
    if (!vm->stack || vm->sp == 0)
        return false;
    *out = vm->stack[--vm->sp];
    return true;
}

static Result vm_error_stack(void)
{
    return result_error(ERR_OUT_OF_SPACE);
}

Result vm_exec(VM *vm, Bytecode *bc)
{
    if (!vm || !bc || !bc->code)
        return result_error(ERR_UNSUPPORTED_ON_DEVICE);

    vm->bc = bc;

    Value default_stack[VM_DEFAULT_STACK_CAP];
    if (!vm->stack)
    {
        vm->stack = default_stack;
        vm->stack_cap = VM_DEFAULT_STACK_CAP;
        vm->sp = 0;
    }

    size_t ip = 0;
    while (ip < bc->code_len)
    {
        Instruction ins = bc->code[ip++];
        switch ((Op)ins.op)
        {
        case OP_NOP:
            break;
        case OP_PUSH_CONST:
            if (ins.a >= bc->const_len)
                return result_error(ERR_OUT_OF_SPACE);
            if (!vm_push(vm, bc->const_pool[ins.a]))
                return vm_error_stack();
            break;
        case OP_LOAD_VAR:
        {
            if (ins.a >= bc->const_len)
                return result_error(ERR_OUT_OF_SPACE);
            Value name_val = bc->const_pool[ins.a];
            const char *name = value_to_string(name_val);
            Value v;
            if (!var_get(name, &v))
                return result_error_arg(ERR_NO_VALUE, NULL, name);
            if (!vm_push(vm, v))
                return vm_error_stack();
            break;
        }
        case OP_CALL_PRIM:
        {
            if (ins.a >= bc->const_len)
                return result_error(ERR_OUT_OF_SPACE);
            Value name_val = bc->const_pool[ins.a];
            const char *user_name = value_to_string(name_val);
            const Primitive *prim = primitive_find(user_name);
            if (!prim)
                return result_error_arg(ERR_DONT_KNOW_HOW, user_name, NULL);

            uint16_t argc = ins.b;
            Value args[16];
            if (argc > 16)
                return result_error(ERR_TOO_MANY_INPUTS);
            for (int i = (int)argc - 1; i >= 0; i--)
            {
                if (!vm_pop(vm, &args[i]))
                    return vm_error_stack();
            }

            int old_depth = vm->eval ? vm->eval->primitive_arg_depth : 0;
            if (vm->eval)
                vm->eval->primitive_arg_depth++;
            Result r = prim->func(vm->eval, (int)argc, args);
            if (vm->eval)
                vm->eval->primitive_arg_depth = old_depth;
            if (r.status != RESULT_OK)
                return result_set_error_proc(r, user_name);
            if (!vm_push(vm, r.value))
                return vm_error_stack();
            break;
        }
            case OP_CALL_PRIM_INSTR:
            {
                bool instr_mode = (ins.op == OP_CALL_PRIM_INSTR);
                if (ins.a >= bc->const_len)
                    return result_error(ERR_OUT_OF_SPACE);
                Value name_val = bc->const_pool[ins.a];
                const char *user_name = value_to_string(name_val);
                const Primitive *prim = primitive_find(user_name);
                if (!prim)
                    return result_error_arg(ERR_DONT_KNOW_HOW, user_name, NULL);

                uint16_t argc = ins.b;
                Value args[16];
                if (argc > 16)
                    return result_error(ERR_TOO_MANY_INPUTS);
                for (int i = (int)argc - 1; i >= 0; i--)
                {
                    if (!vm_pop(vm, &args[i]))
                        return vm_error_stack();
                }

                int old_depth = vm->eval ? vm->eval->primitive_arg_depth : 0;
                if (vm->eval)
                    vm->eval->primitive_arg_depth++;
                Result r = prim->func(vm->eval, (int)argc, args);
                if (vm->eval)
                    vm->eval->primitive_arg_depth = old_depth;

                if (r.status == RESULT_OK)
                {
                    if (!vm_push(vm, r.value))
                        return vm_error_stack();
                    break;
                }
                if (instr_mode)
                {
                    if (r.status == RESULT_NONE)
                        break;
                    if (r.status == RESULT_ERROR)
                        return result_set_error_proc(r, user_name);
                    return r;
                }
                if (r.status != RESULT_OK)
                    return result_set_error_proc(r, user_name);
                break;
            }
            case OP_CALL_USER_TAIL:
            {
                if (ins.a >= bc->const_len)
                    return result_error(ERR_OUT_OF_SPACE);
                Value name_val = bc->const_pool[ins.a];
                const char *user_name = value_to_string(name_val);
                UserProcedure *proc = proc_find(user_name);
                if (!proc)
                    return result_error_arg(ERR_DONT_KNOW_HOW, user_name, NULL);

                uint16_t argc = ins.b;
                Value args[16];
                if (argc > 16)
                    return result_error(ERR_TOO_MANY_INPUTS);
                for (int i = (int)argc - 1; i >= 0; i--)
                {
                    if (!vm_pop(vm, &args[i]))
                        return vm_error_stack();
                }

                if (vm->eval && vm->eval->proc_depth > 0)
                {
                    FrameStack *frames = vm->eval->frames;
                    if (frames && !frame_stack_is_empty(frames))
                    {
                        FrameHeader *current = frame_current(frames);
                        if (current && current->proc == proc)
                        {
                            TailCall *tc = proc_get_tail_call();
                            tc->is_tail_call = true;
                            tc->proc_name = proc->name;
                            tc->arg_count = argc;
                            for (int i = 0; i < argc; i++)
                            {
                                tc->args[i] = args[i];
                            }
                            return result_stop();
                        }
                    }

                    if (vm->eval->primitive_arg_depth == 0)
                    {
                        return result_call(proc, (int)argc, args);
                    }
                }

                return proc_call(vm->eval, proc, (int)argc, args);
            }
        case OP_NEG:
        {
            Value v;
            if (!vm_pop(vm, &v))
                return vm_error_stack();
            float n;
            if (!value_to_number(v, &n))
                return result_error_arg(ERR_DONT_KNOW_WHAT, NULL, value_to_string(v));
            if (!vm_push(vm, value_number(-n)))
                return vm_error_stack();
            break;
        }
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_LT:
        case OP_GT:
        case OP_EQ:
        {
            Value rhs;
            Value lhs;
            if (!vm_pop(vm, &rhs) || !vm_pop(vm, &lhs))
                return vm_error_stack();

            if (ins.op == OP_EQ)
            {
                bool equal = values_equal(lhs, rhs);
                Value out = value_word(mem_atom_cstr(equal ? "true" : "false"));
                if (!vm_push(vm, out))
                    return vm_error_stack();
                break;
            }

            const char *op_name = "?";
            switch ((Op)ins.op)
            {
            case OP_ADD: op_name = "+"; break;
            case OP_SUB: op_name = "-"; break;
            case OP_MUL: op_name = "*"; break;
            case OP_DIV: op_name = "/"; break;
            case OP_LT: op_name = "<"; break;
            case OP_GT: op_name = ">"; break;
            default: break;
            }

            float left_n, right_n;
            if (!value_to_number(lhs, &left_n))
                return result_error_arg(ERR_DOESNT_LIKE_INPUT, op_name, value_to_string(lhs));
            if (!value_to_number(rhs, &right_n))
                return result_error_arg(ERR_DOESNT_LIKE_INPUT, op_name, value_to_string(rhs));

            if (ins.op == OP_LT || ins.op == OP_GT)
            {
                bool truth = (ins.op == OP_LT) ? (left_n < right_n) : (left_n > right_n);
                Value out = value_word(mem_atom_cstr(truth ? "true" : "false"));
                if (!vm_push(vm, out))
                    return vm_error_stack();
                break;
            }

            if (ins.op == OP_DIV && right_n == 0.0f)
                return result_error(ERR_DIVIDE_BY_ZERO);

            float result = 0.0f;
            switch ((Op)ins.op)
            {
            case OP_ADD: result = left_n + right_n; break;
            case OP_SUB: result = left_n - right_n; break;
            case OP_MUL: result = left_n * right_n; break;
            case OP_DIV: result = left_n / right_n; break;
            default: break;
            }
            if (!vm_push(vm, value_number(result)))
                return vm_error_stack();
            break;
        }
        case OP_END_INSTR:
        {
            if (vm->eval)
                vm->eval->in_tail_position = false;
            if (vm->sp > 0)
            {
                Value v;
                if (!vm_pop(vm, &v))
                    return vm_error_stack();
                return result_error_arg(ERR_DONT_KNOW_WHAT, NULL, value_to_string(v));
            }
            break;
        }
        case OP_BEGIN_INSTR:
        {
            if (vm->eval)
                vm->eval->in_tail_position = (ins.a != 0);
            break;
        }
        default:
            return result_error(ERR_UNSUPPORTED_ON_DEVICE);
        }
    }

    if (vm->sp == 0)
        return result_none();
    return result_ok(vm->stack[vm->sp - 1]);
}
