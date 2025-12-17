//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements the LogoHardware interface for PicoCalc device.
//

#pragma once

#include "devices/hardware.h"

#ifdef __cplusplus
extern "C"
{
#endif

    //
    // LogoStorage API
    //

    LogoHardware *logo_picocalc_hardware_create(void);
    void logo_picocalc_hardware_destroy(LogoHardware *hardware);

#ifdef __cplusplus
}
#endif
