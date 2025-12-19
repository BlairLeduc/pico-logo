//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements the LogoStorage interface for host (desktop) device.
//

#pragma once

#include "devices/storage.h"

#ifdef __cplusplus
extern "C"
{
#endif

    //
    // LogoStorage API
    //

    LogoStorage *logo_host_storage_create(void);
    void logo_host_storage_destroy(LogoStorage *storage);

#ifdef __cplusplus
}
#endif
