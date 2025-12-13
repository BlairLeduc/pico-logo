//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements the LogoStorage interface.
//

#include "storage.h"

#include <string.h>

void logo_storage_init(LogoStorage *storage, const LogoStorageOps *ops)
{
    if (!storage)
    {
        return;
    }

    storage->ops = ops;
}
