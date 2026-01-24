//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  VM evaluator scaffolding (Phase 0).
//

#include "vm.h"
#include "error.h"

void vm_init(VM *vm)
{
    if (!vm)
        return;
    vm->bc = NULL;
}

Result vm_exec(VM *vm, Bytecode *bc)
{
    (void)vm;
    (void)bc;
    return result_error(ERR_UNSUPPORTED_ON_DEVICE);
}
