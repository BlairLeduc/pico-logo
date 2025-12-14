//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements the LogoStorage interface for PicoCalc device.
//

#pragma once

#include "devices/storage.h"

#define LOGO_STORAGE_ROOT "/Logo/"


#ifdef __cplusplus
extern "C"
{
#endif

    //
    // LogoStorage API
    //

    LogoStorage *logo_picocalc_storage_create(void);
    void logo_picocalc_storage_destroy(LogoStorage *storage);

#ifdef __cplusplus
}
#endif
