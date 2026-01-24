//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Compiler scaffolding for VM bytecode (Phase 0).
//

#include "compiler.h"
#include "error.h"

Result compile_expression(Compiler *c, Bytecode *bc)
{
    (void)c;
    (void)bc;
    return result_error(ERR_UNSUPPORTED_ON_DEVICE);
}

Result compile_instruction(Compiler *c, Bytecode *bc)
{
    (void)c;
    (void)bc;
    return result_error(ERR_UNSUPPORTED_ON_DEVICE);
}

Result compile_list(Compiler *c, Node list, Bytecode *bc)
{
    (void)c;
    (void)list;
    (void)bc;
    return result_error(ERR_UNSUPPORTED_ON_DEVICE);
}
